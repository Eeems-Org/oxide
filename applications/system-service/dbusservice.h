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

#include "dbussettings.h"
#include "powerapi.h"
#include "wifiapi.h"
#include "appsapi.h"
#include "systemapi.h"
#include "screenapi.h"
#include "buttonhandler.h"
#include "digitizerhandler.h"

#define dbusService DBusService::singleton()

using namespace std;

struct APIEntry {
    QString path;
    QStringList* dependants;
    APIBase* instance;
};

class DBusService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GENERAL_INTERFACE)
    Q_PROPERTY(int tarnishPid READ tarnishPid)
public:
    static DBusService* singleton(){
        static DBusService* instance;
        if(instance == nullptr){
            qRegisterMetaType<QMap<QString, QDBusObjectPath>>();
            qDebug() << "Creating DBusService instance";
            instance = new DBusService();
            auto bus = QDBusConnection::systemBus();
            if(!bus.isConnected()){
                qFatal("Failed to connect to system bus.");
            }
            QDBusConnectionInterface* interface = bus.interface();
            qDebug() << "Registering service...";
            auto reply = interface->registerService(OXIDE_SERVICE);
            bus.registerService(OXIDE_SERVICE);
            if(!reply.isValid()){
                QDBusError ex = reply.error();
                qFatal("Unable to register service: %s", ex.message().toStdString().c_str());
            }
            qDebug() << "Registering object...";
            if(!bus.registerObject(OXIDE_SERVICE_PATH, instance, QDBusConnection::ExportAllContents)){
                qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
            }
            connect(bus.interface(), SIGNAL(serviceOwnerChanged(QString,QString,QString)),
                    instance, SLOT(serviceOwnerChanged(QString,QString,QString)));
            qDebug() << "Registered";
            instance->startup();
        }
        return instance;
    }
    static void shutdown(){
        qDebug() << "Killing dbus service ";
        delete singleton();
    }
    DBusService() : apis(){
        apis.insert("wifi", APIEntry{
            .path = QString(OXIDE_SERVICE_PATH) + "/wifi",
            .dependants = new QStringList(),
            .instance = new WifiAPI(this),
        });
        apis.insert("system", APIEntry{
            .path = QString(OXIDE_SERVICE_PATH) + "/system",
            .dependants = new QStringList(),
            .instance = new SystemAPI(this),
        });
        apis.insert("power", APIEntry{
            .path = QString(OXIDE_SERVICE_PATH) + "/power",
            .dependants = new QStringList(),
            .instance = new PowerAPI(this),
        });
        apis.insert("screen", APIEntry{
            .path = QString(OXIDE_SERVICE_PATH) + "/screen",
            .dependants = new QStringList(),
            .instance = new ScreenAPI(this),
        });
        apis.insert("apps", APIEntry{
            .path = QString(OXIDE_SERVICE_PATH) + "/apps",
            .dependants = new QStringList(),
            .instance = new AppsAPI(this),
        });

        connect(buttonHandler, &ButtonHandler::leftHeld, systemAPI, &SystemAPI::leftAction);
        connect(buttonHandler, &ButtonHandler::homeHeld, systemAPI, &SystemAPI::homeAction);
        connect(buttonHandler, &ButtonHandler::rightHeld, systemAPI, &SystemAPI::rightAction);
        connect(buttonHandler, &ButtonHandler::powerHeld, systemAPI, &SystemAPI::powerAction);
        connect(buttonHandler, &ButtonHandler::powerPress, systemAPI, &SystemAPI::suspend);
        connect(buttonHandler, &ButtonHandler::activity, systemAPI, &SystemAPI::activity);
        connect(touchHandler, &DigitizerHandler::activity, systemAPI, &SystemAPI::activity);
        connect(wacomHandler, &DigitizerHandler::activity, systemAPI, &SystemAPI::activity);
        connect(powerAPI, &PowerAPI::chargerStateChanged, systemAPI, &SystemAPI::activity);
        connect(systemAPI, &SystemAPI::leftAction, appsAPI, &AppsAPI::leftHeld);
        connect(systemAPI, &SystemAPI::homeAction, appsAPI, &AppsAPI::homeHeld);

        auto bus = QDBusConnection::systemBus();
        for(auto api : apis){
            bus.unregisterObject(api.path);
        }
    }
    ~DBusService(){
        qDebug() << "Removing all APIs";
        auto bus = QDBusConnection::systemBus();
        for(auto api : apis){
            api.instance->setEnabled(false);
            bus.unregisterObject(api.path);
            apiUnavailable(QDBusObjectPath(api.path));
            delete api.instance;
            delete api.dependants;
        }
        apis.clear();
    }

    QObject* getAPI(QString name){
        if(!apis.contains(name)){
            return nullptr;
        }
        return apis[name].instance;
    }

    int tarnishPid(){ return qApp->applicationPid(); }

public slots:
    QDBusObjectPath requestAPI(QString name, QDBusMessage message) {
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
    };
    Q_NOREPLY void releaseAPI(QString name, QDBusMessage message) {
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
    };
    QVariantMap APIs(){
        QVariantMap result;
        for(auto key : apis.keys()){
            auto api = apis[key];
            if(api.dependants->size()){
                result[key] = QVariant::fromValue(api.path);
            }
        }
        return result;
    };

    void startup(){
        appsAPI->startup();
    }


signals:
    void apiAvailable(QDBusObjectPath api);
    void apiUnavailable(QDBusObjectPath api);

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
        Q_UNUSED(oldOwner);
        if(newOwner.isEmpty()){
            auto bus = QDBusConnection::systemBus();
            for(auto key : apis.keys()){
                auto api = apis[key];
                api.dependants->removeAll(name);
                if(!api.dependants->size() && bus.objectRegisteredAt(api.path) != nullptr){
                    qDebug() << "Automatically unregistering " << api.path;
                    api.instance->setEnabled(false);
                    bus.unregisterObject(api.path, QDBusConnection::UnregisterNode);
                    apiUnavailable(QDBusObjectPath(api.path));
                }
            }
            systemAPI->uninhibitAll(name);
        }
    }

private:
    QMap<QString, APIEntry> apis;
};

#endif // DBUSSERVICE_H
