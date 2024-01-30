#ifndef DBUSSERVICE_H
#define DBUSSERVICE_H

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusConnectionInterface>
#include <QDBusConnection>
#include <fstream>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <liboxide.h>

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

#define dbusService DBusService::singleton()

using namespace std;
using namespace Oxide::Sentry;

struct APIEntry {
    QString path;
    QStringList* dependants;
    APIBase* instance;
};

class DBusService : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GENERAL_INTERFACE)
    Q_PROPERTY(int tarnishPid READ tarnishPid)

public:
    static DBusService* singleton();
    DBusService(QObject* parent);
    ~DBusService();
    void setEnabled(bool enabled);

    QObject* getAPI(QString name);
    QQmlApplicationEngine* engine();

    int tarnishPid();
    void startup(QQmlApplicationEngine* engine);

public slots:
    QDBusObjectPath requestAPI(QString name, QDBusMessage message);
    Q_NOREPLY void releaseAPI(QString name, QDBusMessage message);
    QVariantMap APIs();


signals:
    void apiAvailable(QDBusObjectPath api);
    void apiUnavailable(QDBusObjectPath api);
    void aboutToQuit();

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

private:
    QQmlApplicationEngine* m_engine;
    QMap<QString, APIEntry> apis;
};

#endif // DBUSSERVICE_H
