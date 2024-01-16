#include "dbusinterface.h"
#include <QDBusConnection>

DbusInterface::DbusInterface(QObject* parent) : QObject(parent) {
    auto bus = QDBusConnection::systemBus();
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
    if(!bus.registerObject(BLIGHT_SERVICE_PATH, this, QDBusConnection::ExportAllContents)){
        qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
    }
    connect(
        interface,
        &QDBusConnectionInterface::serviceOwnerChanged,
        this,
        &DbusInterface::serviceOwnerChanged
    );
}

void DbusInterface::serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
    Q_UNUSED(oldOwner);
    if(!newOwner.isEmpty()){
        return;
    }
    Q_UNUSED(name);
    // TODO - keep track of things this name owns and remove them

}
