#include "connection.h"

#include <liboxide/signalhandler.h>
#include <liboxide/debug.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <QElapsedTimer>
#include <QCoreApplication>

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
}

Connection::~Connection(){ }

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

void Connection::readSocket(){
    auto socket = m_socketPair.writeSocket();
    while(!socket->atEnd()){
        O_DEBUG(socket->readAll());
    }
}
#include "moc_connection.cpp"
