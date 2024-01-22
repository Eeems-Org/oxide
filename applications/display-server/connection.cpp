#include "connection.h"

#include <liboxide/signalhandler.h>
#include <liboxide/debug.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <cstring>

#include "surface.h"

Connection::Connection(QObject* parent, pid_t pid, pid_t pgid)
: QObject(parent),
  m_pid{pid},
  m_pgid{pgid}
{
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

    O_DEBUG("Connection" << id() << "created");
}

Connection::~Connection(){
    O_DEBUG("Connection" << id() << "destroyed");
    surfaces.empty();
}

QString Connection::id(){ return QString("connection/%1").arg(m_pid); }

pid_t Connection::pid() const{ return m_pid; }

pid_t Connection::pgid() const{ return m_pgid; }

int Connection::socketDescriptor(){ return m_clientFd; }

int Connection::inputReadSocketDescriptor(){ return m_clientInputFd; }
int Connection::inputWriteSocketDescriptor(){ return m_serverInputFd; }

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
    blockSignals(true);
    m_notifier->deleteLater();
    ::close(m_clientFd);
    ::close(m_serverFd);
}

std::shared_ptr<Surface> Connection::addSurface(int fd, QRect geometry, int stride, QImage::Format format){
    auto surface = std::shared_ptr<Surface>(new Surface(this, fd, geometry, stride, format));
    connect(surface.get(), &Surface::destroyed, this, [this, surface]{
        QMutableListIterator<std::shared_ptr<Surface>> i(surfaces);
        while(i.hasNext()){
            auto item = i.next();
            if(item == surface){
                i.remove();
            }
        }
    });
    surfaces.append(surface);
    return surface;
}

std::shared_ptr<Surface> Connection::getSurface(QString identifier){
    for(auto surface : qAsConst(surfaces)){
        if(surface == nullptr){
            continue;
        }
        if(surface->id() == identifier){
            return surface;
        }
    }
    return nullptr;
}

QStringList Connection::getSurfaces(){
    QStringList identifiers;
    for(auto surface : qAsConst(surfaces)){
        if(surface == nullptr){
            continue;
        }
        identifiers.append(surface->id());
    }
    return identifiers;
}

void Connection::inputEvent(const input_event& event){
    int res = -1;
    while(res < 0){
        res = ::send(m_serverInputFd, &event, sizeof(event), MSG_EOR);
        if(res > -1){
            break;
        }
        if(errno == EAGAIN || errno == EINTR){
            timespec remaining;
            timespec requested{
                .tv_sec = 0,
                .tv_nsec = 5000
            };
            nanosleep(&requested, &remaining);
            continue;
        }
        break;
    }
    if(res < 0){
        O_WARNING("Failed to write input event: " << std::strerror(errno));
    }else if(res != sizeof(event)){
        O_WARNING("Failed to write input event: Size mismatch!");
    }
}

void Connection::readSocket(){
    m_notifier->setEnabled(false);
    O_DEBUG("Data received");
    while(true){
        auto message = Blight::message_t::from_socket(m_serverFd);
        if(message->header.type == Blight::MessageType::Invalid){
            break;
        }
        O_DEBUG(
            "Handling message: "
            << "type=" << message->header.type
            << ", ackid=" << message->header.ackid
            << ", size=" << message->header.size
            << ", socket=" << m_serverFd
        );
        unsigned int ack_size = 0;
        Blight::data_t ack_data = nullptr;
        std::function<void()> ack_free = NULL;
        switch(message->header.type){
            case Blight::MessageType::Repaint:{
                auto repaint = Blight::repaint_t::from_message(message.get());
                O_DEBUG(
                    "Repaint requested: "
                    << repaint.identifier.c_str()
                    << QString("(%1,%2) %3x%4")
                        .arg(repaint.header.x)
                        .arg(repaint.header.y)
                        .arg(repaint.header.width)
                        .arg(repaint.header.height)
                        .toStdString()
                        .c_str()
                );
                auto surface = getSurface(repaint.identifier.c_str());
                if(surface == nullptr){
                    O_WARNING("Could not find surface " << repaint.identifier.c_str());
                    break;
                }
                emit surface->update(QRect(
                    repaint.header.x,
                    repaint.header.y,
                    repaint.header.width,
                    repaint.header.height
                ));
                break;
            }
            case Blight::MessageType::Move:{
                auto move = Blight::move_t::from_message(message.get());
                O_DEBUG(
                    "Move requested: "
                    << move.identifier.c_str()
                    << QString("(%1,%2)")
                           .arg(move.header.x)
                           .arg(move.header.y)
                           .toStdString()
                           .c_str()
                    );
                auto surface = getSurface(move.identifier.c_str());
                if(surface == nullptr){
                    O_WARNING("Could not find surface " << move.identifier.c_str());
                    break;
                }
                surface->move(move.header.x, move.header.y);
                break;
            }
            case Blight::MessageType::Info:{
                std::string identifier;
                identifier.assign(
                    reinterpret_cast<char*>(message->data.get()),
                    message->header.size
                );
                auto surface = getSurface(identifier.c_str());
                if(surface == nullptr){
                    O_WARNING("Could not find surface " << identifier.c_str());
                    break;
                }
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
                std::string identifier;
                identifier.assign(
                    reinterpret_cast<char*>(message->data.get()),
                    message->header.size
                );
                auto surface = getSurface(identifier.c_str());
                if(surface == nullptr){
                    O_WARNING("Could not find surface " << identifier.c_str());
                    break;
                }
                surfaces.removeAll(surface);
                break;
            }
            case Blight::MessageType::List:{
                O_DEBUG("List requested");
                std::vector<std::string> list;
                for(QString& surface : getSurfaces()){
                    auto string = surface.toStdString();
                    ack_size += sizeof(Blight::list_item_t::identifier_len) + string.size();
                    list.push_back(string);
                }
                ack_size += sizeof(Blight::list_header_t);
                ack_data = (Blight::data_t)malloc(ack_size);
                unsigned int offset = sizeof(Blight::list_header_t);
                Blight::list_header_t header{
                    .count = (unsigned int)list.size()
                };
                memcpy(ack_data, &header, offset);
                for(auto& identifier : list){
                    auto size = identifier.size();
                    memcpy(&ack_data[offset], &size, sizeof(Blight::list_item_t::identifier_len));
                    offset += sizeof(Blight::list_item_t::identifier_len);
                    memcpy(&ack_data[offset], identifier.data(), size);
                    offset += size;
                }
                if(offset != ack_size){
                    O_WARNING("Size mismatch on list data");
                }
                ack_free = [&ack_data]{ free(ack_data); };
                break;
            }
            case Blight::MessageType::Ack:
                O_WARNING("Unexpected ack from client");
                break;
            default:
                O_WARNING("Unexpected message type" << message->header.type);
        }
        if(ack_size && ack_data == nullptr){
            O_WARNING("Ack expected data, but none sent");
            ack_size = 0;
        }
        auto ack = Blight::message_t::create_ack(message.get(), ack_size);
        auto res = ::send(
            m_serverFd,
            reinterpret_cast<char*>(ack),
            sizeof(Blight::header_t),
            MSG_EOR
        );
        if(res < 0){
            O_WARNING("Failed to write to socket:" << strerror(errno));
        }else if(res != sizeof(Blight::header_t)){
            O_WARNING("Failed to write to socket: Size of written bytes doesn't match!");
        }else if(ack_size){
            res = ::send(m_serverFd, ack_data, ack_size, MSG_EOR);
            if(res < 0){
                O_WARNING("Failed to write to socket:" << strerror(errno));
            }else if((unsigned int)res != ack_size){
                O_WARNING("Failed to write to socket: Size of written bytes doesn't match!");
            }else{
                O_DEBUG("Acked" << message->header.ackid);
            }
        }else{
            O_DEBUG("Acked" << message->header.ackid);
        }
        if(ack_free){
            ack_free();
        }else if(ack_data != nullptr){
            delete ack_data;
        }
    };
    m_notifier->setEnabled(true);
}
#include "moc_connection.cpp"
