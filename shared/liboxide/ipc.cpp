#include "ipc.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

#include "debug.h"
#include "liboxide.h"

namespace Oxide {

  enum class MessageType : char { Files = 0x01, Message = 0x02 };

  IPC::IPC(QCoreApplication* app, QObject* parent)
    : IPC(app->applicationName(), parent) {}

  IPC::IPC(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name) {
    if (m_name.isEmpty()) {
      O_WARNING("IPC: applicationName must not be empty");
    }
    m_lockPath = "/run/oxide/" + m_name + ".lock";
    m_socketPath = "/run/oxide/" + m_name + ".socket";
  }

  IPC* IPC::instance() {
    if (!qApp) {
      return nullptr;
    }
    static IPC inst(qApp);
    return &inst;
  }

  IPC::~IPC() {
    if (m_server) {
      m_server->close();
      QFile::remove(m_socketPath);
    }
    if (m_lockFd >= 0) {
      releaseLock(m_lockFd, m_lockPath.toUtf8().constData());
      m_lockFd = -1;
    }
  }

  bool IPC::start() {
    m_lockFd = tryGetLock(m_lockPath.toUtf8().constData());
    if (m_lockFd < 0) {
      O_WARNING("IPC: failed to acquire lock for " << m_name);
      return false;
    }

    m_server = new QLocalServer(this);
    QFile::remove(m_socketPath);
    if (!m_server->listen(m_socketPath)) {
      O_WARNING(
        "IPC: listen failed for " << m_socketPath << ": "
                                  << m_server->errorString()
      );
      releaseLock(m_lockFd, m_lockPath.toUtf8().constData());
      m_lockFd = -1;
      return false;
    }

    O_WARNING("IPC: server started for " << m_name << " at " << m_socketPath);
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
      QLocalSocket* socket = m_server->nextPendingConnection();
      if (!socket) {
        return;
      }
      connect(
        socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater
      );
      connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
        char type;
        if (!socket->getChar(&type)) {
          return;
        }
        switch (static_cast<MessageType>(type)) {
          case MessageType::Files: {
            QDataStream stream(socket->readAll());
            stream.setVersion(QDataStream::Qt_6_0);
            quint32 count;
            stream >> count;
            QStringList fileList;
            for (quint32 i = 0; i < count; ++i) {
              QString file;
              stream >> file;
              fileList << file;
            }
            O_WARNING("IPC::recv type=files count=" << fileList.size());
            emit files(fileList);
            socket->write("ok");
            socket->flush();
            break;
          }
          case MessageType::Message: {
            QByteArray data = socket->readAll();
            O_WARNING("IPC::recv type=message size=" << data.size());
            emit message(data);
            socket->write("ok");
            socket->flush();
            break;
          }
          default: {
            O_WARNING(
              "IPC::recv unknown message type " << static_cast<int>(type)
            );
            socket->write("nok");
            socket->flush();
            break;
          }
        }
        socket->disconnectFromServer();
      });
    });

    return true;
  }

  bool IPC::send(const QStringList& files, int timeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(m_socketPath);
    if (!socket.waitForConnected(timeoutMs)) {
      O_WARNING("IPC::send(QStringList) connect failed to " << m_socketPath);
      return false;
    }

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<quint32>(files.size());
    for (const QString& file : files) {
      stream << file;
    }

    QByteArray message;
    message.append(static_cast<char>(MessageType::Files));
    message.append(payload);

    if (socket.write(message) != message.size()) {
      O_WARNING("IPC::send(QStringList) write failed");
      return false;
    }
    socket.flush();

    if (!socket.waitForReadyRead(timeoutMs)) {
      O_WARNING("IPC::send(QStringList) timeout waiting for response");
      return false;
    }
    QByteArray response = socket.readAll();
    bool ok = response == "ok";
    if (!ok) {
      O_WARNING("IPC::send(QStringList) unexpected response " << response);
    }
    return ok;
  }

  bool IPC::send(const QByteArray& data, int timeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(m_socketPath);
    if (!socket.waitForConnected(timeoutMs)) {
      O_WARNING("IPC::send(QByteArray) connect failed to " << m_socketPath);
      return false;
    }

    QByteArray message;
    message.append(static_cast<char>(MessageType::Message));
    message.append(data);

    if (socket.write(message) != message.size()) {
      O_WARNING("IPC::send(QByteArray) write failed");
      return false;
    }
    socket.flush();

    if (!socket.waitForReadyRead(timeoutMs)) {
      O_WARNING("IPC::send(QByteArray) timeout waiting for response");
      return false;
    }
    QByteArray response = socket.readAll();
    bool ok = response == "ok";
    if (!ok) {
      O_WARNING("IPC::send(QByteArray) unexpected response " << response);
    }
    return ok;
  }

} // namespace Oxide
