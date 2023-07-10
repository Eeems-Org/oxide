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
    qRegisterMetaType<QAbstractSocket::SocketState>();
    connect(&m_socketPair, &SocketPair::readyRead, this, &ChildEntry::readSocket);
    auto pidfd = syscall(SYS_pidfd_open, pid, 0);
    if(pidfd == -1){
        O_WARNING("Failed to open pidfd for child:" << strerror(errno));
        return;
    }
    if(!m_process.open(pidfd, QFile::ReadOnly, QFile::AutoCloseHandle)){
        O_WARNING("Failed to open pidfd for child:" << m_process.errorString());
        ::close(pidfd);
        return;
    }
    connect(&m_process, &QFile::readChannelFinished, this, &ChildEntry::finished);
    m_socketPair.setEnabled(true);
    // TODO - handle communication on m_socketPair
}

ChildEntry::~ChildEntry(){ }

pid_t ChildEntry::pid() const{ return m_pid; }

pid_t ChildEntry::pgid() const{ return m_pgid; }

QLocalSocket* ChildEntry::socket(){ return m_socketPair.readSocket(); }

bool ChildEntry::isValid(){ return m_socketPair.isValid() && m_process.isOpen(); }

bool ChildEntry::signal(int signal){
    if(!m_process.isOpen()){
        errno = ESRCH;
        return false;
    }
    return kill(m_pid, signal) != -1;
}

bool ChildEntry::signalGroup(int signal){
    if(!m_process.isOpen()){
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

void ChildEntry::readSocket(){
    auto socket = m_socketPair.writeSocket();
    while(!socket->atEnd()){
        O_DEBUG(socket->readAll());
    }
}
