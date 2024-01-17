#include "dbusinterface.h"
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QCoreApplication>
#include <unistd.h>
#include <liboxide/debug.h>

DbusInterface::DbusInterface(QObject* parent, QQmlApplicationEngine* engine)
: QObject(parent),
  engine(engine)
{
    // Needed to trick QtDBus into exposing the object
    setProperty("__init__", true);
#ifdef EPAPER
    auto bus = QDBusConnection::systemBus();
#else
    auto bus = QDBusConnection::sessionBus();
#endif
    if(!bus.isConnected()){
        qFatal("Failed to connect to system bus.");
    }
    QDBusConnectionInterface* interface = bus.interface();
    if(!bus.registerObject(
        "/",
        this,
        QDBusConnection::ExportAllContents | QDBusConnection::ExportChildObjects
    )){
        qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
    }
    if(!bus.objectRegisteredAt("/")){
        qFatal("Object not appearing!");
    }
    auto reply = interface->registerService(BLIGHT_SERVICE);
    bus.registerService(BLIGHT_SERVICE);
    if(!reply.isValid()){
        QDBusError ex = reply.error();
        qFatal("Unable to register service: %s", ex.message().toStdString().c_str());
    }
    connect(
        interface,
        &QDBusConnectionInterface::serviceOwnerChanged,
        this,
        &DbusInterface::serviceOwnerChanged
        );
    O_DEBUG("Connected service to bus");
    connect(&connectionTimer, &QTimer::timeout, this, [this]{
        for(auto connection : qAsConst(connections)){
            if(!connection->isRunning()){
                QMetaObject::invokeMethod(connection, "finished", Qt::QueuedConnection);
            }
        }
    });
    connectionTimer.setInterval(100);
    connectionTimer.start();
}

int DbusInterface::pid(){ return qApp->applicationPid(); }

QDBusUnixFileDescriptor DbusInterface::open(QDBusMessage message){
    pid_t pid = connection().interface()->servicePid(message.service());;
    pid_t pgid = ::getpgid(pid);
    O_INFO("Open connection for: " << pid << pgid);
    auto connection = getConnection(message);
    if(connection != nullptr){
        O_DEBUG("Found existing");
        return QDBusUnixFileDescriptor(connection->socket()->socketDescriptor());
    }
    connection = new Connection(this, pid, pgid);
    if(!connection->isValid()){
        O_WARNING("Not valid");
        connection->deleteLater();
        sendErrorReply(QDBusError::InternalError, "Unable to create connection");
        return QDBusUnixFileDescriptor();
    }
    connect(connection, &Connection::finished, this, [this, connection]{
        removeConnection(connection->pid());
    });
    connections.append(connection);
    O_DEBUG("success" << connection->socket()->socketDescriptor());
    return QDBusUnixFileDescriptor(connection->socket()->socketDescriptor());
}

QString DbusInterface::addSurface(QDBusUnixFileDescriptor fd, int x, int y, int width, int height, QDBusMessage message){
    if(!fd.isValid()){
        sendErrorReply(QDBusError::InvalidArgs, "Invalid file descriptor");
        return "";
    }
    auto connection = getConnection(message);
    if(connection == nullptr){
        sendErrorReply(QDBusError::AccessDenied, "You must first open a connection");
        return "";
    }
    auto surface = connection->addSurface(fd.fileDescriptor(), QRect(x, y, width, height));
    if(surface == nullptr){
        sendErrorReply(QDBusError::InternalError, "Unable to create surface");
        return "";
    }
    engine->load(QUrl(QStringLiteral("qrc:/Surface.qml")));
    return surface->id();
}

void DbusInterface::serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
    Q_UNUSED(oldOwner);
    if(!newOwner.isEmpty()){
        return;
    }
    Q_UNUSED(name);
    // TODO - keep track of things this name owns and remove them

}

void DbusInterface::removeConnection(pid_t pid){
    O_DEBUG("unregisterChild" << pid << "requested");
    // TODO - Use STL style iterators
    //        https://doc.qt.io/qt-5/containers.html#stl-style-iterators
    QMutableListIterator<Connection*> i(connections);
    while(i.hasNext()){
        auto connection = i.next();
        if(connection->pid() != pid){
            continue;
        }
        O_INFO("unregisterChild" << connection->pid() << connection->pgid());
        // TODO - remove anything this connection owns
        i.remove();
        connection->close();
        connection->deleteLater();
    }
}

Connection* DbusInterface::getConnection(QDBusMessage message){
    pid_t pid = connection().interface()->servicePid(message.service());;
    for(auto connection : qAsConst(connections)){
        if(!connection->isRunning()){
            continue;
        }
        if(connection->pid() == pid){
            return connection;
        }
    }
    return nullptr;
}

#include "moc_dbusinterface.cpp"
