#include "connection.h"

#include <liboxide/signalhandler.h>
#include <liboxide/debug.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <QElapsedTimer>
#include <QCoreApplication>

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

    // TODO - handle communication on m_socketPair
    // TODO - Allow child to request that tarnish connect with ptrace to avoid touching the
    //        framebuffer at the same time.
    //        https://man7.org/linux/man-pages/man2/ptrace.2.html     PTRACE_SEIZE
    //        https://nullprogram.com/blog/2018/06/23/
    O_DEBUG("Connection" << id() << "created");
}

Connection::~Connection(){
    O_DEBUG("Connection" << id() << "destroyed");
    while(!surfaces.isEmpty()){
        delete surfaces.takeFirst();
    }
}

QString Connection::id(){ return QString("connection/%1").arg(m_pid); }

pid_t Connection::pid() const{ return m_pid; }

pid_t Connection::pgid() const{ return m_pgid; }

int Connection::socketDescriptor(){ return m_clientFd; }

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

Surface* Connection::addSurface(int fd, QRect geometry, int stride, QImage::Format format){
    auto surface = new Surface(this, fd, geometry, stride, format);
    surfaces.append(surface);
    return surface;
}

Surface* Connection::getSurface(QString identifier){
    for(auto surface : qAsConst(surfaces)){
        if(surface->id() == identifier){
            return surface;
        }
    }
    return nullptr;
}

void Connection::readSocket(){
    m_notifier->setEnabled(false);
    O_DEBUG("Data received");
    while(true){
        auto message = Blight::message_t::from_socket(m_serverFd);
        if(message == nullptr){
            break;
        }
        O_DEBUG(
            "Handling message: "
            << "type=" << message->header.type
            << ", ackid=" << message->header.ackid
            << ", size=" << message->header.size
            << ", socket=" << m_serverFd
        );
        switch(message->header.type){
            case Blight::MessageType::Repaint:{
                auto repaint = Blight::repaint_t::from_message(message);
                O_DEBUG("Repaint requested: " << repaint.identifier.c_str());
                auto surface = getSurface(repaint.identifier.c_str());
                if(surface != nullptr){
                    emit surface->update(QRect(
                        repaint.header.x,
                        repaint.header.y,
                        repaint.header.width,
                        repaint.header.height
                    ));
                }else{
                    O_WARNING("Could not find surface for identifier repaint");
                }
                break;
            }
            case Blight::MessageType::Ack:
                O_WARNING("Unexpected ack from client");
                break;
            default:
                O_WARNING("Unexpected message type" << message->header.type);
        }
        auto ack = Blight::message_t::create_ack(message);
        auto res = ::send(
            m_serverFd,
            reinterpret_cast<char*>(ack),
            sizeof(Blight::header_t),
            MSG_EOR
        );
        delete ack;
        if(res < 0){
            O_WARNING("Failed to write to socket:" << strerror(errno));
        }else if(res != sizeof(Blight::header_t)){
            O_WARNING("Failed to write to socket: Size of written bytes doesn't match!");
        }else{
            O_DEBUG("Acked" << message->header.ackid);
        }
        if(message->data != nullptr){
            delete message->data;
            message->data = nullptr;
        }
        delete message;
    };
    m_notifier->setEnabled(true);
}
#include "moc_connection.cpp"
