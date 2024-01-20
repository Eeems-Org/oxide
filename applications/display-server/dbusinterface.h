#pragma once

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#pragma once
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QDBusUnixFileDescriptor>

#include "connection.h"

#include "../../shared/liboxide/meta.h"

class DbusInterface : public QObject, public QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_INTERFACE)
    Q_PROPERTY(int pid READ pid CONSTANT)

public:
    static DbusInterface* singleton;
    DbusInterface(QObject* parent, QQmlApplicationEngine* engine);

    int pid();
    QObject* loadComponent(QString url, QString identifier, QVariantMap properties = QVariantMap());
    Surface* getSurface(QString identifier);

public slots:
    QDBusUnixFileDescriptor open(QDBusMessage message);
    QString addSurface(
        QDBusUnixFileDescriptor fd,
        int x,
        int y,
        int width,
        int height,
        int stride,
        int format,
        QDBusMessage message
    );
    void repaint(QString identifier, QDBusMessage message);
    QDBusUnixFileDescriptor getSurface(QString identifier, QDBusMessage message);

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

private:
    QQmlApplicationEngine* engine;
    QList<Connection*> connections;
    QTimer connectionTimer;

    Connection* getConnection(QDBusMessage message);
    QObject* workspace();
    Connection* createConnection(int pid);
};
