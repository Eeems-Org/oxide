#include "childentry.h"

#include <liboxide/signalhandler.h>
#include <unistd.h>
#include <sys/syscall.h>

ChildEntry::ChildEntry(QObject* parent, pid_t pid, pid_t pgid)
: QObject(parent),
  m_pid{pid},
  m_pgid{pgid},
  m_socketPair{true}
{
    connect(&m_socketPair, &SocketPair::readyRead, this, &ChildEntry::readSocket);
    connect(&m_socketPair, &SocketPair::disconnected, this, &ChildEntry::finished);
    m_socketPair.setEnabled(true);
    // TODO - handle communication on m_socketPair
    // TODO - Allow child to request that tarnish connect with ptrace to avoid touching the
    //        framebuffer at the same time.
    //        https://man7.org/linux/man-pages/man2/ptrace.2.html     PTRACE_SEIZE
    //        https://nullprogram.com/blog/2018/06/23/
}

ChildEntry::~ChildEntry(){ }

pid_t ChildEntry::pid() const{ return m_pid; }

pid_t ChildEntry::pgid() const{ return m_pgid; }

QLocalSocket* ChildEntry::socket(){ return m_socketPair.readSocket(); }

bool ChildEntry::isValid(){ return m_socketPair.isValid() && isRunning(); }

bool ChildEntry::isRunning(){ return getpgid(m_pid) != -1; }

bool ChildEntry::signal(int signal){
    if(!isRunning()){
        errno = ESRCH;
        return false;
    }
    return kill(m_pid, signal) != -1;
}

bool ChildEntry::signalGroup(int signal){
    if(!isRunning()){
        errno = ESRCH;
        return false;
    }
    return kill(-m_pid, signal) != -1;
}

void ChildEntry::pause(){
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

void ChildEntry::resume(){
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

qint64 ChildEntry::write(QByteArray data){ return m_socketPair.write(data); }

QString ChildEntry::errorString(){ return m_socketPair.errorString(); }

void ChildEntry::close(){
    blockSignals(true);
    m_socketPair.setEnabled(false);
    m_socketPair.close();
}

void ChildEntry::readSocket(){
    auto socket = m_socketPair.writeSocket();
    while(!socket->atEnd()){
        O_DEBUG(socket->readAll());
    }
}
