#include "dbusinterface.h"
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QCoreApplication>
#include <unistd.h>
#include <liboxide/debug.h>

DbusInterface::DbusInterface(QObject* parent) : QObject(parent) {}

void DbusInterface::registerService(){
#ifdef EPAPER
    auto bus = QDBusConnection::systemBus();
#else
    auto bus = QDBusConnection::sessionBus();
#endif
    if(!bus.isConnected()){
        qFatal("Failed to connect to system bus.");
    }
    QDBusConnectionInterface* interface = bus.interface();
    auto reply = interface->registerService(BLIGHT_SERVICE);
    bus.registerService(BLIGHT_SERVICE);
    if(!reply.isValid()){
        QDBusError ex = reply.error();
        qFatal("Unable to register service: %s", ex.message().toStdString().c_str());
    }
    if(!bus.registerObject("/", this, QDBusConnection::ExportAllContents)){
        qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
    }
    if(!bus.objectRegisteredAt("/")){
        qFatal("Object not appearing!");
    }
    if(!bus.registerObject(BLIGHT_SERVICE_PATH, this, QDBusConnection::ExportAllContents)){
        qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
    }
    if(!bus.objectRegisteredAt(BLIGHT_SERVICE_PATH)){
        qFatal("Object not appearing!");
    }
    O_DEBUG("Object registered to " BLIGHT_SERVICE_PATH);
    connect(
        interface,
        &QDBusConnectionInterface::serviceOwnerChanged,
        this,
        &DbusInterface::serviceOwnerChanged
        );
    O_DEBUG("Connected service to bus");
}

int DbusInterface::pid(){ return qApp->applicationPid(); }

QDBusUnixFileDescriptor DbusInterface::open(QDBusMessage message){
    pid_t pid = connection().interface()->servicePid(message.service());;
    pid_t pgid = ::getpgid(pid);
    O_INFO("Open connection for: " << pid << pgid);
    for(auto connection : qAsConst(connections)){
        if(connection->pid() == pid){
            O_DEBUG("Found existing");
            return QDBusUnixFileDescriptor(connection->socket()->socketDescriptor());
        }
    }
    auto connection = new Connection(this, pid, pgid);
    if(!connection->isValid()){
        O_WARNING("Not valid");
        connection->deleteLater();
        return QDBusUnixFileDescriptor();
    }
    connect(connection, &Connection::finished, this, [this, connection]{
        removeConnection(connection->pid());
    });
    connections.append(connection);
    O_DEBUG("success" << connection->socket()->socketDescriptor());
    return QDBusUnixFileDescriptor(connection->socket()->socketDescriptor());
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

#include "moc_dbusinterface.cpp"
