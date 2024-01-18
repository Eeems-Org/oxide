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
  m_pgid{pgid},
  m_socketPair{true}
{
    connect(&m_socketPair, &SocketPair::readyRead, this, &Connection::readSocket);
    connect(&m_socketPair, &SocketPair::disconnected, this, &Connection::finished);
    m_socketPair.setEnabled(true);
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

QLocalSocket* Connection::socket(){ return m_socketPair.readSocket(); }

bool Connection::isValid(){ return m_socketPair.isValid() && isRunning(); }

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

qint64 Connection::write(QByteArray data){ return m_socketPair.write(data); }

QString Connection::errorString(){ return m_socketPair.errorString(); }

void Connection::close(){
    blockSignals(true);
    m_socketPair.setEnabled(false);
    m_socketPair.close();
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
    auto socket = m_socketPair.writeSocket();
    QByteArray buf;
    while(!socket->atEnd()){
        buf += socket->readAll();
    }
    while(!buf.isEmpty()){
        auto message = Blight::message_t::from_data(buf.data());
        qDebug() << buf.mid(0, sizeof(Blight::header_t) + message.header.size);
        O_DEBUG(
            "Handling message: "
            << "type=" << message.header.type
            << ", ackid=" << message.header.ackid
            << ", size=" << message.header.size
        );
        switch(message.header.type){
            case Blight::MessageType::Repaint:{
                auto repaint = Blight::repaint_t::from_message(&message);
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
                O_WARNING("Unexpected message type" << message.header.type);
        }
        auto ack = Blight::message_t::create_ack(message);
        auto res = socket->write(
            reinterpret_cast<char*>(ack),
            sizeof(Blight::header_t)
        );
        delete ack;
        if(res < 0){
            O_WARNING("Failed to write to socket:" << socket->errorString());
        }else if(res != sizeof(Blight::header_t)){
            O_WARNING("Failed to write to socket: Size of written bytes doesn't match!");
        }else{
            O_DEBUG("Acked" << message.header.ackid);
        }
        buf.remove(0, sizeof(Blight::header_t) + message.header.size);
    }
}
#include "moc_connection.cpp"
