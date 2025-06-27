#include "connection.h"
#include "dbusinterface.h"

#include <liboxide/signalhandler.h>
#include <liboxide/debug.h>
#include <libblight/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <cstring>
#include <sys/syscall.h>
#include <unistd.h>
#include <assert.h>

#ifdef EPAPER
#include "guithread.h"
#include <mxcfb.h>
#endif
#include "surface.h"

#define C_DEBUG(msg) O_DEBUG("[" << id() << "]" << msg)
#define C_WARNING(msg) O_WARNING("[" << id() << "]" << msg)
#define C_INFO(msg) O_INFO("[" << id() << "]" << msg)

#ifndef SYS_pidfd_open
# define SYS_pidfd_open 434
#endif

static int pidfd_open(pid_t pid, unsigned int flags){
    return syscall(SYS_pidfd_open, pid, flags);
}

Connection::Connection(pid_t pid, pid_t pgid)
: QObject(),
  m_pid{pid},
  m_pgid{pgid},
  m_closed{false},
  pingId{0},
  m_surfaceId{0},
  m_lastEventOffset{0}
{
    m_pidFd = pidfd_open(m_pid, 0);
    if(m_pidFd < 0){
        O_WARNING(std::strerror(errno));
    }else{
        connect(&m_pidNotifier, &QLocalSocket::disconnected, this, &Connection::close);
        m_pidNotifier.setSocketDescriptor(m_pidFd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly);
    }
    int fds[2];
    if(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) == -1){
        C_WARNING("Unable to open socket pair:" << strerror(errno));
    }
    m_clientFd = fds[0];
    m_serverFd = fds[1];
    m_notifier = new QSocketNotifier(m_serverFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &Connection::readSocket);

    if(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) == -1){
        C_WARNING("Unable to open input socket pair:" << strerror(errno));
    }
    m_clientInputFd = fds[0];
    m_serverInputFd = fds[1];


    m_pingTimer.setParent(this);
    m_pingTimer.setTimerType(Qt::PreciseTimer);
    m_pingTimer.setInterval(1000);
    m_pingTimer.setSingleShot(true);
    connect(&m_pingTimer, &QTimer::timeout, this, &Connection::ping);
    m_pingTimer.start();

    m_notRespondingTimer.setParent(this);
    m_notRespondingTimer.setTimerType(Qt::PreciseTimer);
    m_notRespondingTimer.setInterval(m_pingTimer.interval() * 2);
    m_notRespondingTimer.setSingleShot(true);
    connect(&m_notRespondingTimer, &QTimer::timeout, this, &Connection::notResponding);
    m_notRespondingTimer.start();

    m_inputQueueTimer.setParent(this);
    m_inputQueueTimer.setTimerType(Qt::PreciseTimer);
    m_inputQueueTimer.setInterval(10);
    m_inputQueueTimer.setSingleShot(true);
    connect(&m_inputQueueTimer, &QTimer::timeout, this, &Connection::processInputEvents);

    C_INFO("Connection created");
}

Connection::~Connection(){
    close();
    surfaces.clear();
    processRemovedSurfaces();
    if(m_notifier != nullptr){
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    ::close(m_clientInputFd);
    ::close(m_clientFd);
    ::close(m_serverFd);
    ::close(m_pidFd);
    C_INFO("Connection destroyed");
}

void Connection::processRemovedSurfaces(){
    removedMutex.lock();
    if(!removedSurfaces.empty()){
        C_DEBUG("Cleaning up old surfaces");
        removedSurfaces.clear();
    }
    removedMutex.unlock();
}

QString Connection::id(){ return QString("connection/%1").arg(m_pid); }

pid_t Connection::pid() const{ return m_pid; }

pid_t Connection::pgid() const{ return m_pgid; }

int Connection::socketDescriptor(){ return m_clientFd; }

int Connection::inputSocketDescriptor(){ return m_clientInputFd; }

bool Connection::isValid(){ return m_clientFd > 0 && m_serverFd > 0 && isRunning(); }

bool Connection::isRunning(){ return getpgid(m_pid) != -1; }

bool Connection::isStopped(){
    QFile file(QString("/proc/%1/stat").arg(m_pid));
    if(!file.open(QFile::ReadOnly)){
        O_WARNING("Failed checking process status" << file.errorString());
        return false;
    }
    return file.readAll().split(' ')[2] == "T";
}

bool Connection::signal(int signal){
    if(!isRunning()){
        errno = ESRCH;
        return false;
    }
    return ::kill(m_pid, signal) != -1;
}

bool Connection::signalGroup(int signal){
    if(!isRunning()){
        errno = ESRCH;
        return false;
    }
    return ::kill(-m_pid, signal) != -1;
}

void Connection::pause(){
    // TODO - Send pause request over socket instead
    QElapsedTimer timer;
    bool replied = false;
    auto conn = connect(signalHandler, &Oxide::SignalHandler::sigUsr2, [&timer, &replied]{
        replied = true;
        timer.invalidate();
    });
    signalGroup(SIGUSR2);
    timer.start();
    while(timer.isValid() && !timer.hasExpired(1000)){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
    signalHandler->disconnect(conn);
    if(replied){
        return;
    }
    signalGroup(SIGSTOP);
    siginfo_t info;
    waitid(P_PID, m_pid, &info, WSTOPPED);
}

void Connection::resume(){
    // TODO - Send resume request over socket instead
    QElapsedTimer timer;
    bool replied = false;
    auto conn = connect(signalHandler, &Oxide::SignalHandler::sigUsr1, [&timer, &replied]{
        replied = true;
        timer.invalidate();
    });
    signalGroup(SIGUSR1);
    timer.start();
    while(timer.isValid() && !timer.hasExpired(1000)){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
    signalHandler->disconnect(conn);
    if(replied){
        return;
    }
    signalGroup(SIGCONT);
    siginfo_t info;
    waitid(P_PID, m_pid, &info, WCONTINUED);
}

void Connection::close(){
    if(!m_closed.test_and_set()){
        m_pingTimer.stop();
        m_notRespondingTimer.stop();
        m_inputQueueTimer.stop();
        for(auto& item : surfaces){
            item.second->removed();
        }
        emit finished();
    }
}

std::shared_ptr<Surface> Connection::addSurface(int fd, QRect geometry, int stride, QImage::Format format){
    // TODO - add validation that id is never 0, and that it doesn't point to an existsing surface
    auto surfaceId = ++m_surfaceId;
    std::shared_ptr<Surface> surface{nullptr};
    try{
        surface = std::make_shared<Surface>(this, fd, surfaceId, geometry, stride, format);
    }catch(const std::bad_alloc&){
        C_WARNING("Unable to add surface, out of memory");
        return surface;
    }
    if(!surface->isValid()){
        return surface;
    }
    surfaces.insert_or_assign(surfaceId, surface);
    dbusInterface->sortZ();
    surface->repaint();
    return surface;
}

std::shared_ptr<Surface> Connection::getSurface(QString identifier){
    for(auto& item: surfaces){
        auto& surface = item.second;
        if(surface == nullptr){
            continue;
        }
        if(surface->id() == identifier){
            return surface;
        }
    }
    return nullptr;
}

std::shared_ptr<Surface> Connection::getSurface(Blight::surface_id_t id){
    if(!surfaces.contains(id)){
        return nullptr;
    }
    return surfaces[id];
}

QStringList Connection::getSurfaceIdentifiers(){
    QStringList identifiers;
    for(auto& item: surfaces){
        auto& surface = item.second;
        if(surface == nullptr){
            continue;
        }
        identifiers.append(surface->id());
    }
    return identifiers;
}

const QList<std::shared_ptr<Surface>> Connection::getSurfaces(){
    QList<std::shared_ptr<Surface>> values;
    for(auto& item: surfaces){
        auto& surface = item.second;
        if(surface != nullptr){
            values.append(surface);
        }
    }
    return values;
}

void Connection::inputEvents(unsigned int device, const std::vector<input_event>& events){
    if(!isRunning() || isStopped()){
        dbusInterface->sortZ();
        return;
    }
    // TODO - don't allow queue to grow forever
    //        instead clear and properly send SYN_DROPPED for proper devices
    //        when it grows too large
    C_DEBUG("Queuing" << events.size() << "input events");
    for(unsigned int i = 0; i < events.size(); i++){
        Blight::event_packet_t packet;
        auto& ie = events[i];
        packet.device = device;
        auto& event = packet.event;
        event.type = ie.type;
        event.code = ie.code;
        event.value = ie.value;
        m_inputQueue.push(packet);
    }
    processInputEvents();
}
void Connection::processInputEvents(){
    if(m_inputQueue.empty()){
        return;
    }
    while(!m_inputQueue.empty()){
        if(m_lastEventOffset >= sizeof(Blight::event_packet_t)){
            // Should never be larger than size, only check in debug builds
            assert(m_lastEventOffset <= sizeof(Blight::event_packet_t));
            // Last send sent full packet, move to the next one
            m_lastEventOffset = 0;
            m_inputQueue.pop();
            continue;
        }
        int res = send(
            m_serverInputFd,
            reinterpret_cast<const char*>(&m_inputQueue.front()) + m_lastEventOffset,
            sizeof(Blight::event_packet_t) - m_lastEventOffset,
            MSG_EOR | MSG_NOSIGNAL
        );
        if(res >= 0){
            // Successful write, could be partial, so check again at the top
            m_lastEventOffset += res;
            continue;
        }
        if(errno == EINTR){
            // We were interrupted, try again
            continue;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            // socket is blocking, try again later
            m_inputQueueTimer.start();
            break;
        }
        C_WARNING("Failed to write input event: " << std::strerror(errno));
    }
}

bool Connection::has(const QString& flag){ return flags.contains(flag); }

void Connection::set(const QString& flag){
    if(!has(flag)){
        flags.append(flag);
    }
}

void Connection::unset(const QString& flag){ flags.removeAll(flag); }

void Connection::readSocket(){
    if(m_notifier == nullptr){
        C_DEBUG("Connection already closed, discarding data");
        return;
    }
    m_notifier->setEnabled(false);
#ifdef ACK_DEBUG
    C_DEBUG("Data received");
#endif
    while(true){
        auto message = Blight::message_t::from_socket(m_serverFd);
        if(message == nullptr){
            QThread::msleep(1);
            continue;
        }
        if(message->header.type == Blight::MessageType::Invalid){
            break;
        }
#ifndef ACK_DEBUG
        if(
            message->header.type != Blight::MessageType::Ping
            && message->header.type != Blight::MessageType::Ack
        ){
#endif
            C_DEBUG(
                "Handling message:"
                << "type=" << message->header.type
                << ", ackid=" << message->header.ackid
                << ", size=" << message->header.size
                << ", socket=" << m_serverFd
            );
#ifndef ACK_DEBUG
        }
#endif
        bool do_ack = true;
        unsigned int ack_size = 0;
        Blight::data_t ack_data = nullptr;
        std::function<void()> ack_free = NULL;
        switch(message->header.type){
            case Blight::MessageType::Repaint:{
                auto repaint = Blight::repaint_t::from_message(message.get());
                C_DEBUG(
                    "Repaint requested:"
                    << QString("%1 (%2,%3) %4x%5 %6 %7")
                        .arg(repaint.identifier)
                        .arg(repaint.x)
                        .arg(repaint.y)
                        .arg(repaint.width)
                        .arg(repaint.height)
                        .arg(repaint.waveform)
                        .arg(repaint.mode)
                        .arg(repaint.marker)
                        .toStdString()
                        .c_str()
                );
                auto surface = getSurface(repaint.identifier);
                if(surface == nullptr){
                    C_WARNING("Could not find surface" << repaint.identifier);
                    break;
                }
                QRect rect(
                    repaint.x,
                    repaint.y,
                    repaint.width,
                    repaint.height
                );
#ifdef EPAPER
                guiThread->enqueue(
                    surface,
                    rect,
                    repaint.waveform,
                    repaint.mode,
                    repaint.marker,
                    false,
                    [message, this]{ ack(message, 0, nullptr); }
                );
                do_ack = false;
#else
                emit surface->update(rect);
#endif
                break;
            }
            case Blight::MessageType::Move:{
                auto move = Blight::move_t::from_message(message.get());
                C_DEBUG(
                    "Move requested:"
                    << QString("%3 (%1,%2)")
                           .arg(move.x)
                           .arg(move.y)
                           .arg(move.identifier)
                           .toStdString()
                           .c_str()
                );
                auto surface = getSurface(move.identifier);
                if(surface == nullptr){
                    C_WARNING("Could not find surface" << move.identifier);
                    break;
                }
#ifdef EPAPER
                auto rect = surface->geometry();
                surface->move(move.x, move.y);
                guiThread->enqueue(
                    surface,
                    surface->rect(),
                    Blight::WaveformMode::HighQualityGrayscale,
                    Blight::UpdateMode::PartialUpdate,
                    message->header.ackid,
                    false,
                    [message, this]{ ack(message, 0, nullptr); }
                );
                guiThread->enqueue(
                    nullptr,
                    rect,
                    Blight::WaveformMode::HighQualityGrayscale,
                    Blight::UpdateMode::PartialUpdate,
                    message->header.ackid,
                    true,
                    nullptr
                );
                do_ack = false;
#else
                surface->move(move.header.x, move.header.y);
#endif
                break;
            }
            case Blight::MessageType::Info:{
                auto identifier = (Blight::surface_id_t)*message->data.get();
                C_DEBUG("Info requested:" << identifier);
                if(!surfaces.contains(identifier)){
                    C_WARNING("Could not find surface" << identifier);
                    break;
                }
                auto surface = surfaces[identifier];
                auto geometry = surface->geometry();
                try{
                    ack_data = reinterpret_cast<Blight::data_t>(new Blight::surface_info_t{
                        {
                          .x = geometry.x(),
                          .y = geometry.y(),
                          .width = (unsigned int)geometry.width(),
                          .height = (unsigned int)geometry.height(),
                          .stride = surface->stride(),
                          .format = (Blight::Format)surface->format(),
                        }
                    });
                    ack_size = sizeof(Blight::surface_info_t);
                }catch(const std::bad_alloc&){
                    C_WARNING("Could not allocate ack_data, not enough memory");
                }
                break;
            }
            case Blight::MessageType::Delete:{
                auto identifier = (Blight::surface_id_t)*message->data.get();
                C_DEBUG("Delete requested:" << identifier);
                if(!surfaces.contains(identifier)){
                    C_WARNING("Could not find surface" << identifier);

                    break;
                }
                removedMutex.lock();
                auto surface = surfaces[identifier];
                surface->removed();
                removedSurfaces.push_back(surface);
                removedMutex.unlock();
                surfaces.erase(identifier);
                guiThread->notify();
                break;
            }
            case Blight::MessageType::List:{
                C_DEBUG("List requested");
                std::vector<Blight::surface_id_t> list;
                for(auto& item: surfaces){
                    if(item.second != nullptr){
                        list.push_back(item.first);
                    }
                }
                ack_size = list.size() * sizeof(decltype(list)::value_type);
                ack_data = (Blight::data_t)malloc(ack_size);
                memcpy(ack_data, list.data(), ack_size);
                ack_free = [&ack_data]{ free(ack_data); };
                break;
            }
            case Blight::MessageType::Raise:{
                auto identifier = (Blight::surface_id_t)*message->data.get();
                C_DEBUG("Raise requested:" << identifier);
                if(!surfaces.contains(identifier)){
                    C_WARNING("Could not find surface" << identifier);
                    break;
                }
                auto surface = surfaces[identifier];
                surface->setVisible(true);
                surface->setZ(std::numeric_limits<int>::max());
                dbusInterface->sortZ();
#ifdef EPAPER
                guiThread->enqueue(
                    surface,
                    surface->rect(),
                    Blight::WaveformMode::HighQualityGrayscale,
                    Blight::UpdateMode::PartialUpdate,
                    message->header.ackid,
                    false,
                    [message, this]{ ack(message, 0, nullptr); }
                );
                do_ack = false;
#endif
                break;
            }
            case Blight::MessageType::Lower:{
                auto identifier = (Blight::surface_id_t)*message->data.get();
                C_DEBUG("Lower requested:" << identifier);
                if(!surfaces.contains(identifier)){
                    C_WARNING("Could not find surface" << identifier);
                    break;
                }
                auto surface = surfaces[identifier];
                surface->setVisible(false);
                dbusInterface->sortZ();
#ifdef EPAPER
                guiThread->enqueue(
                    nullptr,
                    surface->geometry(),
                    Blight::WaveformMode::HighQualityGrayscale,
                    Blight::UpdateMode::PartialUpdate,
                    message->header.ackid,
                    true,
                    [message, this]{ ack(message, 0, nullptr); }
                );
                do_ack = false;
#endif
                break;
            }
            case Blight::MessageType::Wait:{
#ifdef EPAPER
                auto marker = (unsigned int)*message->data.get();
                C_DEBUG("Wait requested:" << marker);
                mxcfb_update_marker_data data{ marker, 0 };
                ioctl(guiThread->framebuffer(), MXCFB_WAIT_FOR_UPDATE_COMPLETE, &data);
#endif
                break;
            }
            case Blight::MessageType::Focus:{
                C_DEBUG("Focus requested");
                dbusInterface->setFocus(this);
                break;
            }
            case Blight::MessageType::Ping:{
#ifdef ACK_DEBUG
                O_DEBUG("Pong" << message->header.ackid);
#endif
                break;
            }
            case Blight::MessageType::Ack:
                do_ack = false;
                if(message->header.ackid == pingId){
#ifdef ACK_DEBUG
                    O_DEBUG("Pong recieved" << message->header.ackid);
#endif
                    m_notRespondingTimer.stop();
                    m_pingTimer.stop();
                    m_pingTimer.start();
                    m_notRespondingTimer.start();
                    break;
                }
                C_WARNING("Unexpected ack from client" << message->header.ackid);
                break;
            default:
                C_WARNING("Unexpected message type" << message->header.type);
        }
        if(ack_size && ack_data == nullptr){
            C_WARNING("Ack expected data, but none sent");
            ack_size = 0;
        }
        if(do_ack){
            ack(message, ack_size, ack_data);
        }
        if(ack_free){
            ack_free();
        }else if(ack_data != nullptr){
            delete ack_data;
        }
    };
    m_notifier->setEnabled(true);
}

void Connection::notResponding(){
    if(!isRunning()){
        close();
        m_pingTimer.stop();
        return;
    }
    if(!isStopped()){
        C_WARNING("Connection failed to respond to ping in time:" << id());
    }else if(this == dbusInterface->focused()){
        dbusInterface->sortZ();
    }
    m_notRespondingTimer.start();
}

void Connection::ack(Blight::message_ptr_t message, unsigned int size, Blight::data_t data){
    auto ack = Blight::message_t::create_ack(message.get(), size);
    if(!Blight::send_blocking(
        m_serverFd,
        reinterpret_cast<Blight::data_t>(&ack),
        sizeof(Blight::header_t)
    )){
        C_WARNING("Failed to write ack header to socket:" << strerror(errno));
    }else if(size && !Blight::send_blocking(m_serverFd, data, size)){
        C_WARNING("Failed to write ack data to socket:" << strerror(errno));
    }else{
        C_DEBUG("Acked:" << ack.ackid);
    }
}

void Connection::ping(){
    if(!isRunning()){
        return;
    }
    if(!isStopped()){
        Blight::header_t ping{
           {
             .type = Blight::MessageType::Ping,
             .ackid = ++pingId,
             .size = 0
           }
        };
        if(!Blight::send_blocking(
            m_serverFd,
            reinterpret_cast<Blight::data_t>(&ping),
            sizeof(Blight::header_t)
        )){
            C_WARNING("Failed to write to socket:" << strerror(errno));
        }else{
#ifdef ACK_DEBUG
            C_DEBUG("Ping" << ping.ackid);
#endif
        }
    }
    m_pingTimer.start();
}
#include "moc_connection.cpp"
