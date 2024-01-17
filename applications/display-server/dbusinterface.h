#pragma once

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

#include "connection.h"

#include "../../shared/liboxide/meta.h"

class DbusInterface : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_INTERFACE)
    Q_PROPERTY(int pid READ pid CONSTANT)

public:
    DbusInterface(QObject* parent);
    void registerService();

    int pid();

public slots:
    QDBusUnixFileDescriptor open(QDBusMessage message);

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

private:
    QList<Connection*> connections;

    void removeConnection(pid_t pid);
};
