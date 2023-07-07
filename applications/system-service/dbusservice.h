#ifndef DBUSSERVICE_H
#define DBUSSERVICE_H

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusConnectionInterface>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QLocalServer>
#include <fstream>
#include <liboxide.h>

#include "powerapi.h"
#include "wifiapi.h"
#include "appsapi.h"
#include "systemapi.h"
#include "screenapi.h"
#include "notificationapi.h"
#include "guiapi.h"
#include "buttonhandler.h"
#include "digitizerhandler.h"

#define dbusService DBusService::singleton()

using namespace std;
using namespace Oxide::Sentry;

struct APIEntry {
    QString path;
    QStringList* dependants;
    APIBase* instance;
};

struct ChildEntry {
    std::string service;
    qint64 pid;
    std::string name;
    int stdout;
    int stderr;
    std::string uniqueName() const{
        return QString("%1-%2-%3")
            .arg(service.c_str())
            .arg(name.c_str())
            .arg(pid)
            .toStdString();
    }
};

class DBusService : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GENERAL_INTERFACE)
    Q_PROPERTY(int tarnishPid READ tarnishPid)

public:
    static DBusService* singleton();
    DBusService(QObject* parent);
    ~DBusService();
    void setEnabled(bool enabled){ Q_UNUSED(enabled); }
    bool isEnabled();

    QObject* getAPI(QString name){
        if(!apis.contains(name)){
            return nullptr;
        }
        return apis[name].instance;
    }

    int tarnishPid(){ return qApp->applicationPid(); }

    Q_INVOKABLE void registerChild(qint64 childPid, QString name, QDBusUnixFileDescriptor stdout, QDBusUnixFileDescriptor stderr, QDBusMessage message){
        auto bus = QDBusConnection::systemBus();
        if(!QDBusUnixFileDescriptor::isSupported()){
            qDebug() << "QDBusUnixFileDescriptor is not supported";
            bus.send(message.createErrorReply(QDBusError::InternalError, "QDBusUnixFileDescriptor is not supported"));
            return;
        }
        if(!stdout.isValid()){
            qDebug() << "stdout passed in by" << childPid << "is invalid";
            bus.send(message.createErrorReply(QDBusError::AccessDenied, "stdout invalid"));
            return;
        }
        if(!stderr.isValid()){
            qDebug() << "stderr passed in by" << childPid << "is invalid";
            bus.send(message.createErrorReply(QDBusError::AccessDenied, "stderr invalid"));
            return;
        }
        qDebug() << "registerChild" << childPid << name;
        children.append(ChildEntry{
            .service = message.service().toStdString(),
            .pid = childPid,
            .name = name.toStdString(),
            .stdout = dup(stdout.fileDescriptor()),
            .stderr = dup(stderr.fileDescriptor())
        });
    }

    Q_INVOKABLE void unregisterChild(QDBusMessage message){ unregisterChild(message.service().toStdString()); }

public slots:
    QDBusObjectPath requestAPI(QString name, QDBusMessage message) {
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", ("requestAPI() " + name).toStdString().c_str(), "query");
#endif
        if(!hasPermission(name)){
            return QDBusObjectPath("/");
        }
        if(!apis.contains(name)){
            return QDBusObjectPath("/");
        }
        auto api = apis[name];
        auto bus = QDBusConnection::systemBus();
        if(bus.objectRegisteredAt(api.path) == nullptr){
            bus.registerObject(api.path, api.instance, QDBusConnection::ExportAllContents);
        }
        if(!api.dependants->size()){
            qDebug() << "Registering " << api.path;
            api.instance->setEnabled(true);
            emit apiAvailable(QDBusObjectPath(api.path));
        }
        api.dependants->append(message.service());
        return QDBusObjectPath(api.path);
    }
    Q_NOREPLY void releaseAPI(QString name, QDBusMessage message) {
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", ("releaseAPI() " + name).toStdString().c_str(), "query");
#endif
        if(!apis.contains(name)){
            return;
        }
        auto api = apis[name];
        auto client = message.service();
        api.dependants->removeAll(client);
        if(!api.dependants->size()){
            qDebug() << "Unregistering " << api.path;
            api.instance->setEnabled(false);
            QDBusConnection::systemBus().unregisterObject(api.path, QDBusConnection::UnregisterNode);
            emit apiUnavailable(QDBusObjectPath(api.path));
        }
    }
    QVariantMap APIs(){
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", "APIs()", "query");
#endif
        QVariantMap result;
        for(auto key : apis.keys()){
            auto api = apis[key];
            if(api.dependants->size()){
                result[key] = QVariant::fromValue(api.path);
            }
        }
        return result;
    }
    void shutdown();

signals:
    void apiAvailable(QDBusObjectPath api);
    void apiUnavailable(QDBusObjectPath api);
    void aboutToQuit();

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
        Q_UNUSED(oldOwner);
        // TODO - replace with QDBusServiceWatcher
        if(!newOwner.isEmpty()){
            return;
        }
        auto bus = QDBusConnection::systemBus();
        for(auto key : apis.keys()){
            auto api = apis[key];
            api.dependants->removeAll(name);
            if(!api.dependants->size() && bus.objectRegisteredAt(api.path) != nullptr){
                qDebug() << "Automatically unregistering " << api.path;
                api.instance->setEnabled(false);
                bus.unregisterObject(api.path, QDBusConnection::UnregisterNode);
                emit apiUnavailable(QDBusObjectPath(api.path));
            }
        }
        unregisterChild(name.toStdString());
        systemAPI->uninhibitAll(name);
    }

private:
    QMap<QString, APIEntry> apis;
    QList<ChildEntry> children;
    void unregisterChild(std::string service){
        QMutableListIterator<ChildEntry> i(children);
        while(i.hasNext()){
            auto child = i.next();
            if(child.service != service){
                continue;
            }
            O_DEBUG("unregisterChild" << child.pid << child.name.c_str());
            i.remove();
        }
    }
    static DBusService* instance;
};

#endif // DBUSSERVICE_H
