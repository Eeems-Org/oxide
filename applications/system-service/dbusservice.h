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
#include "childentry.h"

#define dbusService DBusService::singleton()

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

    Q_INVOKABLE QDBusUnixFileDescriptor registerChild(){
        auto childPid = getSenderPid();
        auto childPgid = getSenderPgid();
        qDebug() << "registerChild::" << childPid << childPgid;
        for(auto child : children){
            if(child->pid() == childPid){
                qDebug() << "registerChild::Found existing";
                return QDBusUnixFileDescriptor(child->socket()->socketDescriptor());
            }
        }
        auto child = new ChildEntry(this, childPid, childPgid);
        if(!child->isValid()){
            qDebug() << "registerChild::Not valid";
            child->deleteLater();
            return QDBusUnixFileDescriptor();
        }
        connect(child, &ChildEntry::finished, this, [this, child]{ unregisterChild(child->pid()); });
        children.append(child);
        qDebug() << "registerChild::success";
        return QDBusUnixFileDescriptor(child->socket()->socketDescriptor());
    }

    Q_INVOKABLE void unregisterChild(){ unregisterChild(getSenderPid()); }

    bool isChild(pid_t pid){
        for(auto child : children){
            if(child->pid() == pid){
                return true;
            }
        }
        return false;
    }

    bool isChildGroup(pid_t pgid){
        for(auto child : children){
            if(child->pgid() == pgid){
                return true;
            }
        }
        return false;
    }

public slots:
    QDBusObjectPath requestAPI(QString name, QDBusMessage message) {
#ifdef SENTRY
        sentry_breadcrumb("dbusservice", ("requestAPI() " + name).toStdString().c_str(), "query");
#endif
        if(!apis.contains(name)){
            return QDBusObjectPath("/");
        }
        auto api = apis[name];
        switch(api.security){
            case APISecurity::Default:
                if(!hasPermission(name)){
                    return QDBusObjectPath("/");
                }
                break;
            case APISecurity::Secure:
                if(!hasPermissionStrict(name)){
                    return QDBusObjectPath("/");
                }
                break;
            case APISecurity::Open:
                break;
            default:
                qWarning() << "Unknown APISecurity" << api.security;
                return QDBusObjectPath("/");
        }
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
        systemAPI->uninhibitAll(name);
    }

private:
    QMap<QString, APIEntry> apis;
    QList<ChildEntry*> children;
    void unregisterChild(pid_t pid){
        QMutableListIterator<ChildEntry*> i(children);
        while(i.hasNext()){
            auto child = i.next();
            if(child->pid() != pid){
                continue;
            }
            O_DEBUG("unregisterChild" << child->pid() << child->pgid());
            guiAPI->closeWindows(child->pid());
            i.remove();
            child->deleteLater();
        }
    }
    static DBusService* instance;
};

#endif // DBUSSERVICE_H
