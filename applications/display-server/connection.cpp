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

#ifdef EPAPER
#include "guithread.h"
#include <mxcfb.h>
#endif
#include "surface.h"

static int pidfd_open(pid_t pid, unsigned int flags){
    return syscall(SYS_pidfd_open, pid, flags);
}

Connection::Connection(pid_t pid, pid_t pgid)
: QObject(),
  m_pid{pid},
  m_pgid{pgid},
  m_closed{false},
  pingId{0},
  m_surfaceId{0}
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
        O_WARNING("Unable to open socket pair:" << strerror(errno));
    }
    m_clientFd = fds[0];
    m_serverFd = fds[1];
    m_notifier = new QSocketNotifier(m_serverFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &Connection::readSocket);

    if(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) == -1){
        O_WARNING("Unable to open input socket pair:" << strerror(errno));
    }
    m_clientInputFd = fds[0];
    m_serverInputFd = fds[1];


    m_pingTimer.setTimerType(Qt::PreciseTimer);
    m_pingTimer.setInterval(1000);
    m_pingTimer.setSingleShot(true);
    connect(&m_pingTimer, &QTimer::timeout, this, &Connection::ping);
    m_pingTimer.start();

    m_notRespondingTimer.setTimerType(Qt::PreciseTimer);
    m_notRespondingTimer.setInterval(m_pingTimer.interval() * 2);
    m_notRespondingTimer.setSingleShot(true);
    connect(&m_notRespondingTimer, &QTimer::timeout, this, &Connection::notResponding);
    m_notRespondingTimer.start();

    O_INFO("Connection" << id() << "created");
}

Connection::~Connection(){
    close();
    surfaces.clear();
    processRemovedSurfaces();
    if(m_notifier != nullptr){
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    ::close(m_clientFd);
    ::close(m_serverFd);
    ::close(m_pidFd);
    O_INFO("Connection" << id() << "destroyed");
}

void Connection::processRemovedSurfaces(){
    removedMutex.lock();
    if(!removedSurfaces.empty()){
        O_DEBUG("Cleaning up old surfaces");
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
        for(auto& item : surfaces){
            item.second->removed();
        }
        emit finished();
    }
}

std::shared_ptr<Surface> Connection::addSurface(int fd, QRect geometry, int stride, QImage::Format format){
    // TODO - add validation that id is never 0, and that it doesn't point to an existsing surface
    auto id = ++m_surfaceId;
    auto surface = std::shared_ptr<Surface>(new Surface(this, fd, id, geometry, stride, format));
    surfaces.insert_or_assign(id, surface);
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
    // TODO - Allow non-blocking send.
    //        If the client isn't reading input, the buffer will fill up, and then the entire
    //        display server will freeze until the client reads it's first event.
    //        It's probably worth adding some sort of acking mechanism to this to ensure that
    //        events are being read, as well as don't send anything until the client tells you
    //        that it's ready to start reading events.
//    O_DEBUG("Writing" << events.size() << "input events");
    std::vector<Blight::event_packet_t> data(events.size());
    for(unsigned int i = 0; i < events.size(); i++){
        auto& ie = events[i];
        auto& packet = data[i];
        packet.device = device;
        auto& event = packet.event;
        event.type = ie.type;
        event.code = ie.code;
        event.value = ie.value;
    }
    if(!Blight::send_blocking(
        m_serverInputFd,
        reinterpret_cast<Blight::data_t>(data.data()),
        sizeof(Blight::event_packet_t) * data.size()
    )){
        O_WARNING("Failed to write input event: " << std::strerror(errno));
    }else{
//        O_DEBUG("Write finished");
    }
}

void Connection::readSocket(){
    if(m_notifier == nullptr){
        O_DEBUG("Connection already closed, discarding data");
        return;
    }
    m_notifier->setEnabled(false);
    O_DEBUG("Data received");
    while(true){
        auto message = Blight::message_t::from_socket(m_serverFd);
        if(message->header.type == Blight::MessageType::Invalid){
            break;
        }
        O_DEBUG(
            "Handling message:"
            << "type=" << message->header.type
            << ", ackid=" << message->header.ackid
            << ", size=" << message->header.size
            << ", socket=" << m_serverFd
        );
        bool do_ack = true;
        unsigned int ack_size = 0;
        Blight::data_t ack_data = nullptr;
        std::function<void()> ack_free = NULL;
        switch(message->header.type){
            case Blight::MessageType::Repaint:{
                auto repaint = Blight::repaint_t::from_message(message.get());
                O_DEBUG(
                    "Repaint requested:"
                    << QString("%6 (%1,%2) %3x%4 %5 %7")
                        .arg(repaint.x)
                        .arg(repaint.y)
                        .arg(repaint.width)
                        .arg(repaint.height)
                        .arg(repaint.waveform)
                        .arg(repaint.identifier)
                        .arg(repaint.marker)
                        .toStdString()
                        .c_str()
                );
                auto surface = getSurface(repaint.identifier);
                if(surface == nullptr){
                    O_WARNING("Could not find surface" << repaint.identifier);
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
                    (EPFrameBuffer::WaveformMode)repaint.waveform,
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
                O_DEBUG(
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
                    O_WARNING("Could not find surface" << move.identifier);
                    break;
                }
#ifdef EPAPER
                auto rect = surface->geometry();
                surface->move(move.x, move.y);
                guiThread->enqueue(
                    surface,
                    // Surface repaints are local to their coordinates, thus (0,0)
                    QRect(QPoint(0, 0), surface->size()),
                    EPFrameBuffer::WaveformMode::HighQualityGrayscale,
                    message->header.ackid,
                    false,
                    [message, this]{ ack(message, 0, nullptr); }
                );
                guiThread->enqueue(
                    nullptr,
                    rect,
                    EPFrameBuffer::WaveformMode::HighQualityGrayscale,
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
                O_DEBUG("Info requested:" << identifier);
                if(!surfaces.contains(identifier)){
                    O_WARNING("Could not find surface" << identifier);
                    break;
                }
                auto surface = surfaces[identifier];
                auto geometry = surface->geometry();
                ack_data = (Blight::data_t)new Blight::surface_info_t{
                    .x = geometry.x(),
                    .y = geometry.y(),
                    .width = geometry.width(),
                    .height = geometry.height(),
                    .stride = surface->stride(),
                    .format = (Blight::Format)surface->format(),
                };
                ack_size = sizeof(Blight::surface_info_t);
                break;
            }
            case Blight::MessageType::Delete:{
                auto identifier = (Blight::surface_id_t)*message->data.get();
                O_DEBUG("Delete requested:" << identifier);
                if(!surfaces.contains(identifier)){
                    O_WARNING("Could not find surface" << identifier);
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
                O_DEBUG("List requested");
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
                O_DEBUG("Raise requested:" << identifier);
                if(!surfaces.contains(identifier)){
                    O_WARNING("Could not find surface" << identifier);
                    break;
                }
                auto surface = surfaces[identifier];
                surface->setVisible(true);
                dbusInterface->sortZ();
#ifdef EPAPER
                guiThread->enqueue(
                    surface,
                    // Surface repaints are local to their coordinates, thus (0,0)
                    QRect(QPoint(0, 0), surface->size()),
                    EPFrameBuffer::WaveformMode::HighQualityGrayscale,
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
                O_DEBUG("Lower requested:" << identifier);
                if(!surfaces.contains(identifier)){
                    O_WARNING("Could not find surface" << identifier);
                    break;
                }
                auto surface = surfaces[identifier];
                surface->setVisible(false);
                dbusInterface->sortZ();
#ifdef EPAPER
                guiThread->enqueue(
                    nullptr,
                    surface->geometry(),
                    EPFrameBuffer::WaveformMode::HighQualityGrayscale,
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
                O_DEBUG("Wait requested:" << marker);
                mxcfb_update_marker_data data{ marker, 0 };
                ioctl(guiThread->framebuffer(), MXCFB_WAIT_FOR_UPDATE_COMPLETE, &data);
#endif
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
                O_WARNING("Unexpected ack from client" << message->header.ackid);
                break;
            default:
                O_WARNING("Unexpected message type" << message->header.type);
        }
        if(ack_size && ack_data == nullptr){
            O_WARNING("Ack expected data, but none sent");
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
    O_WARNING("Connection failed to respond to ping in time:" << id());
    m_notRespondingTimer.start();
}

void Connection::ack(Blight::message_ptr_t message, unsigned int size, Blight::data_t data){
    auto ack = Blight::message_t::create_ack(message.get(), size);
    if(!Blight::send_blocking(
        m_serverFd,
        reinterpret_cast<Blight::data_t>(&ack),
        sizeof(Blight::header_t)
    )){
        O_WARNING("Failed to write ack header to socket:" << strerror(errno));
    }else if(size && !Blight::send_blocking(m_serverFd, data, size)){
        O_WARNING("Failed to write ack data to socket:" << strerror(errno));
    }else{
        O_DEBUG("Acked:" << ack.ackid);
    }
}

void Connection::ping(){
    if(!isRunning()){
        return;
    }
    Blight::header_t ping{
        .type = Blight::MessageType::Ping,
        .ackid = ++pingId,
        .size = 0
    };
    if(!Blight::send_blocking(
        m_serverFd,
        reinterpret_cast<Blight::data_t>(&ping),
        sizeof(Blight::header_t)
    )){
        O_WARNING("Failed to write to socket:" << strerror(errno));
    }else{
        O_DEBUG("Ping" << ping.ackid);
    }
    m_pingTimer.start();
}
#include "moc_connection.cpp"
