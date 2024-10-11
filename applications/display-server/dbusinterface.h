#pragma once

#include <QObject>
#include <QMutex>
#include <QTimer>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QQmlApplicationEngine>
#include <QDBusUnixFileDescriptor>

#include "connection.h"

// Use direct values to make sure that cpp2xml works
#include "../../shared/liboxide/meta.h"
#include "../../shared/libblight/meta.h"

#define dbusInterface DbusInterface::singleton()

class DbusInterface : public QObject, public QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_INTERFACE)
    Q_PROPERTY(int pid READ pid CONSTANT)
    Q_PROPERTY(QByteArray clipboard READ clipboard WRITE setClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(QByteArray selection READ selection WRITE setSelection NOTIFY selectionChanged)
    Q_PROPERTY(QByteArray secondary READ secondary WRITE setSecondary NOTIFY secondaryChanged)

public:
    static DbusInterface* singleton();

    int pid();
#ifndef EPAPER
    QObject* loadComponent(QString url, QString identifier, QVariantMap properties = QVariantMap());
#endif
    void processClosingConnections();
    void processRemovedSurfaces();
    std::shared_ptr<Surface> getSurface(QString identifier);
    QList<std::shared_ptr<Surface>> surfaces();
    QList<std::shared_ptr<Surface>> sortedSurfaces();
    QList<std::shared_ptr<Surface>> visibleSurfaces();
    void sortZ();
    Connection* focused();
    void setFocus(Connection* connection);
    void inputEvents(unsigned int device, const std::vector<input_event>& events);
    bool inExclusiveMode();

    // Property getter/setters
    const QByteArray& clipboard();
    void setClipboard(const QByteArray& data);
    const QByteArray& selection();
    void setSelection(const QByteArray& data);
    const QByteArray& secondary();
    void setSecondary(const QByteArray& data);

public slots:
    QDBusUnixFileDescriptor open(QDBusMessage message);
    QDBusUnixFileDescriptor openInput(QDBusMessage message);
    ushort addSurface(
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
    QDBusUnixFileDescriptor getSurface(ushort identifier, QDBusMessage message);
    void setFlags(QString identifier, const QStringList& flags, QDBusMessage message);
    QStringList getSurfaces(QDBusMessage message);
    QDBusUnixFileDescriptor frameBuffer(QDBusMessage message);
    void lower(QString identifier, QDBusMessage message);
    void raise(QString identifier, QDBusMessage message);
    void focus(QString identifier, QDBusMessage message);
    void waitForNoRepaints(QDBusMessage message);
    void enterExclusiveMode(QDBusMessage message);
    void exitExclusiveMode(QDBusMessage message);
    void exclusiveModeRepaint(QDBusMessage message);

signals:
    void clipboardChanged(const QByteArray& data);
    void selectionChanged(const QByteArray& data);
    void secondaryChanged(const QByteArray& data);

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

private:
    DbusInterface(QObject* parent);
    QQmlApplicationEngine engine;
    QList<Connection*> connections;
    QMutex closingMutex;
    QList<Connection*> closingConnections;
    Connection* m_focused;
    struct {
        QByteArray clipboard;
        QByteArray selection;
        QByteArray secondary;
    } clipboards;
    bool m_exlusiveMode;

    Connection* getConnection(QDBusMessage message);
    Connection* getConnection(QString identifier);
    QObject* workspace();
    Connection* createConnection(int pid);
};
