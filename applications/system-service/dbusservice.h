#ifndef DBUSSERVICE_H
#define DBUSSERVICE_H

#include <QObject>

#include "apibase.h"
#include "childentry.h"

#define dbusService DBusService::__singleton()

using namespace std;
using namespace Oxide::Sentry;

enum APISecurity{
    Default = 0, // Registered apps require the permission, otherwise it's open
    Secure, // Only registered apps with permissions can use it
    Open // Anybody can use it
};

struct APIEntry {
    QString path;
    QStringList* dependants;
    APIBase* instance;
    APISecurity security = APISecurity::Default;
};

class DBusService : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GENERAL_INTERFACE)
    Q_PROPERTY(int tarnishPid READ tarnishPid)

public:
    static DBusService* __singleton();
    DBusService(QObject* parent);
    ~DBusService();
    void setEnabled(bool enabled);
    bool isEnabled();

    QObject* getAPI(QString name);

    int tarnishPid();

    Q_INVOKABLE QDBusUnixFileDescriptor registerChild();
    Q_INVOKABLE void unregisterChild();

    bool isChild(pid_t pid);
    bool isChildGroup(pid_t pgid);

public slots:
    QDBusObjectPath requestAPI(QString name, QDBusMessage message);
    Q_NOREPLY void releaseAPI(QString name, QDBusMessage message);
    QVariantMap APIs();
    void shutdown();

signals:
    void apiAvailable(QDBusObjectPath api);
    void apiUnavailable(QDBusObjectPath api);
    void aboutToQuit();

protected:
    void timerEvent(QTimerEvent* event) override;

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

private:
    QMap<QString, APIEntry> apis;
    QList<ChildEntry*> children;
    int watchdogTimer;
    void unregisterChild(pid_t pid);
    static DBusService* instance;
};

#endif // DBUSSERVICE_H
