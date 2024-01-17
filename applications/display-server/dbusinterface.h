#pragma once

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QTimer>
#include <QQmlApplicationEngine>

#include "connection.h"

#include "../../shared/liboxide/meta.h"

class DbusInterface : public QObject, public QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_INTERFACE)
    Q_PROPERTY(int pid READ pid CONSTANT)

public:
    DbusInterface(QObject* parent, QQmlApplicationEngine* engine);

    int pid();

public slots:
    QDBusUnixFileDescriptor open(QDBusMessage message);
    QString addSurface(QDBusUnixFileDescriptor fd, int x, int y, int width, int height, QDBusMessage message);

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

private:
    QQmlApplicationEngine* engine;
    QList<Connection*> connections;
    QTimer connectionTimer;

    void removeConnection(pid_t pid);
    Connection* getConnection(QDBusMessage message);
};
