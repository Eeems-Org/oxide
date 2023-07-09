#include "eventpipe.h"

#include <sys/socket.h>

EventPipe::EventPipe()
: QObject{nullptr},
  m_readSocket{this},
  m_writeSocket{this},
  m_enabled{false}
{
    int fds[2];
    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1){
        O_WARNING(__PRETTY_FUNCTION__ << "Unable to open event pipe:" << strerror(errno));
    }
    if(!m_readSocket.setSocketDescriptor(fds[1], QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
        O_WARNING(__PRETTY_FUNCTION__ << "Unable to open event pipe read socket:" << m_readSocket.errorString());
    }
    if(!m_writeSocket.setSocketDescriptor(fds[0], QLocalSocket::ConnectedState, QLocalSocket::WriteOnly | QLocalSocket::Unbuffered)){
        O_WARNING(__PRETTY_FUNCTION__ << "Unable to open event pipe write socket:" << m_writeSocket.errorString());
    }
}

EventPipe::~EventPipe(){
    close();
}

bool EventPipe::isValid(){ return m_readSocket.isValid() && m_writeSocket.isValid(); }

bool EventPipe::isOpen(){ return m_readSocket.isOpen() && m_writeSocket.isOpen(); }

void EventPipe::close(){
    m_readSocket.close();
    m_writeSocket.close();
}

QLocalSocket* EventPipe::readSocket(){ return &m_readSocket; }

QLocalSocket* EventPipe::writeSocket(){ return &m_writeSocket; }

void EventPipe::setEnabled(bool enabled){ m_enabled = enabled; }

bool EventPipe::enabled(){ return m_enabled; }

qint64 EventPipe::write(const char* data, qint64 size){
    if(!m_enabled){
        return 0;
    }
    return m_writeSocket.write(data, size);
}

bool EventPipe::flush(){ return m_writeSocket.flush(); }
