#include "socketpair.h"
#include "debug.h"

#include <sys/socket.h>

namespace Oxide{
    SocketPair::SocketPair(bool allowWriteSocketRead)
    : QObject{nullptr},
      m_readSocket{this},
      m_writeSocket{this},
      m_enabled{false},
      m_allowWriteSocketRead{allowWriteSocketRead}
    {
        int fds[2];
        if(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1){
            O_WARNING("Unable to open socket pair:" << strerror(errno));
        }
        if(!m_readSocket.setSocketDescriptor(fds[1], QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            O_WARNING("Unable to open socket pair read socket:" << m_readSocket.errorString());
        }
        auto mode = allowWriteSocketRead ? QLocalSocket::ReadWrite : QLocalSocket::WriteOnly;
        if(!m_writeSocket.setSocketDescriptor(fds[0], QLocalSocket::ConnectedState, mode | QLocalSocket::Unbuffered)){
            O_WARNING("Unable to open socket pair write socket:" << m_writeSocket.errorString());
        }
        if(allowWriteSocketRead){
            connect(&m_writeSocket, &QLocalSocket::readyRead, this, &SocketPair::_readyRead);
        }
        connect(&m_writeSocket, &QLocalSocket::disconnected, this, &SocketPair::disconnected);
        connect(&m_writeSocket, &QLocalSocket::bytesWritten, this, &SocketPair::bytesWritten);
    }

    SocketPair::~SocketPair(){ close(); }

    bool SocketPair::isValid(){ return m_readSocket.isValid() && m_writeSocket.isValid(); }

    bool SocketPair::isReadable(){ return m_enabled && m_allowWriteSocketRead && m_writeSocket.isReadable(); }

    bool SocketPair::isOpen(){ return m_readSocket.isOpen() && m_writeSocket.isOpen(); }

    QLocalSocket* SocketPair::readSocket(){ return &m_readSocket; }

    QLocalSocket* SocketPair::writeSocket(){ return &m_writeSocket; }

    void SocketPair::setEnabled(bool enabled){ m_enabled = enabled; }

    bool SocketPair::enabled(){ return m_enabled; }

    bool SocketPair::atEnd(){ return !m_enabled || m_writeSocket.atEnd(); }

    QByteArray SocketPair::readLine(qint64 maxlen){ return m_allowWriteSocketRead ? m_writeSocket.readLine(maxlen) : QByteArray(); }

    QByteArray SocketPair::readAll(){ return readLine(); }

    QByteArray SocketPair::read(qint64 maxlen){ return m_allowWriteSocketRead ? m_writeSocket.read(maxlen) : QByteArray(); }

    qint64 SocketPair::bytesAvailable(){ return m_allowWriteSocketRead ? m_writeSocket.bytesAvailable() : 0; }

    qint64 SocketPair::_write(const char* data, qint64 size){ return m_writeSocket.write(data, size); }

    qint64 SocketPair::write(const char* data, qint64 size){
        if(!m_enabled){
            return 0;
        }
        return _write(data, size);
    }

    qint64 SocketPair::_write(QByteArray data){ return m_writeSocket.write(data); }

    qint64 SocketPair::write(QByteArray data){
        if(!m_enabled){
            return 0;
        }
        return _write(data);
    }

    QString SocketPair::errorString(){
        if(!m_enabled){
            return "SocketPair not enabled";
        }
        return m_writeSocket.errorString();
    }

    void SocketPair::close(){
        m_readSocket.close();
        m_writeSocket.close();
    }

    void SocketPair::_readyRead(){
        if(m_enabled){
            emit readyRead();
        }else{
            while(m_writeSocket.skip(1024) > 0){}
        }
    }
}
