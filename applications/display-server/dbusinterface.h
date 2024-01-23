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

#define dbusInterface DbusInterface::singleton()

class DbusInterface : public QObject, public QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_INTERFACE)
    Q_PROPERTY(int pid READ pid CONSTANT)
    Q_PROPERTY(QByteArray clipboard READ clipboard WRITE setClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(QByteArray selection READ selection WRITE setSelection NOTIFY selectionChanged)

public:
    static DbusInterface* singleton(){
        static DbusInterface* instance = nullptr;
        if(instance == nullptr){
            instance = new DbusInterface(qApp);
        }
        return instance;
    }

    int pid();
    QObject* loadComponent(QString url, QString identifier, QVariantMap properties = QVariantMap());
    std::shared_ptr<Surface> getSurface(QString identifier);
    QList<std::shared_ptr<Surface>> surfaces();
    QList<std::shared_ptr<Surface>> sortedSurfaces();
    QList<std::shared_ptr<Surface>> visibleSurfaces();
    const QByteArray& clipboard();
    void setClipboard(const QByteArray& data);
    const QByteArray& selection();
    void setSelection(const QByteArray& data);


public slots:
    QDBusUnixFileDescriptor open(QDBusMessage message);
    QDBusUnixFileDescriptor openInput(QDBusMessage message);
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
    std::shared_ptr<Connection> focused();
    void sortZ();

signals:
    void clipboardChanged(const QByteArray& data);
    void selectionChanged(const QByteArray& data);

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);
    void inputEvents(unsigned int device, const std::vector<input_event>& events);

private:
    DbusInterface(QObject* parent);
    QQmlApplicationEngine engine;
    QList<std::shared_ptr<Connection>> connections;
    std::shared_ptr<Connection> m_focused;
    QTimer connectionTimer;
    struct {
        QByteArray clipboard;
        QByteArray selection;
    } clipboards;

    std::shared_ptr<Connection> getConnection(QDBusMessage message);
    QObject* workspace();
    std::shared_ptr<Connection> createConnection(int pid);
};
