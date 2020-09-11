#ifndef DBUSSERVICE_H
#define DBUSSERVICE_H

#include <QFile>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusConnectionInterface>
#include <QDBusConnection>

#include <fstream>

#include "powerapi.h"
#include "wifiapi.h"

using namespace std;

struct APIEntry {
    QString path;
    QStringList dependants;
    QObject* instance;
};

class DBusService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_GENERAL_INTERFACE)
public:
    static DBusService* singleton(){
        static DBusService* instance;
        if(instance == nullptr){
            qRegisterMetaType<QMap<QString, QDBusObjectPath>>();
            qDebug() << "Creating DBusService instance";
            ensureConfig();
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

        }
        return instance;
    }
    static void ensureConfig(){
        QFile outfile(QString(OXIDE_DBUS_CONFIG_PATH));
        if(!outfile.exists()){
            qDebug() << "DBus configuration missing...";
        }
        QFile infile(QString(OXIDE_DBUS_CONFIG_LOCAL_PATH));
        if(!infile.exists()){
            qWarning("Embedded configuration file missing!");
        }else if(!outfile.open(QIODevice::WriteOnly)){
            qWarning("Unable create dbus configuration!");
        }else if(!infile.open(QIODevice::ReadOnly)){
            qWarning("Unable read embedded onfiguration file!");
            outfile.close();
        }else{
            qDebug("Writing dbus configuration...");
            outfile.write(infile.readAll());
            infile.close();
            outfile.close();
            qDebug("Reloading dbus...");
            system("systemctl reload dbus");
            qDebug("Done updating DBus configuration");
        }
    }
    DBusService() : apis(){
        apis.insert("power", APIEntry{
            .path = QString(OXIDE_SERVICE_PATH) + "/power",
            .dependants = QStringList(),
            .instance = new PowerAPI(this),
        });
        apis.insert("wifi", APIEntry{
            .path = QString(OXIDE_SERVICE_PATH) + "/wifi",
            .dependants = QStringList(),
            .instance = new WifiAPI(this),
        });
    }
    ~DBusService() {}

    QObject* getAPI(QString name){
        if(!apis.contains(name)){
            return nullptr;
        }
        return apis[name].instance;
    }

public Q_SLOTS:
    QDBusObjectPath requestAPI(QString name, QDBusMessage message) {
        if(!apis.contains(name)){
            return QDBusObjectPath("/");
        }
        auto api = apis[name];
        if(!api.dependants.length()){
            QDBusConnection::systemBus().registerObject(api.path, api.instance, QDBusConnection::ExportAllContents);
            apiAvailable(QDBusObjectPath(api.path));
        }
        api.dependants.append(message.service());
        return QDBusObjectPath(api.path);
    };
    Q_NOREPLY void releaseAPI(QString name, QDBusMessage message) {
        if(!apis.contains(name)){
            return;
        }
        auto api = apis[name];
        auto client = message.service();
        api.dependants.removeAll(client);
        if(!api.dependants.length()){
            QDBusConnection::systemBus().unregisterObject(api.path);
            apiUnavailable(QDBusObjectPath(api.path));
        }
    };
    QVariantMap APIs(){
        QVariantMap result;
        for(auto key : apis.keys()){
            auto api = apis[key];
            if(api.dependants.length()){
                result[key] = QVariant::fromValue(api.path);
            }
        }
        return result;
    };


Q_SIGNALS:
    void apiAvailable(QDBusObjectPath api);
    void apiUnavailable(QDBusObjectPath api);

private Q_SLOTS:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
        Q_UNUSED(oldOwner);
        if(newOwner.isEmpty()){
            for(auto key : apis.keys()){
                auto api = apis[key];
                api.dependants.removeAll(name);
                if(!api.dependants.length()){
                    QDBusConnection::systemBus().unregisterObject(api.path);
                    apiUnavailable(QDBusObjectPath(api.path));
                }
            }
        }
    }

private:
    QMap<QString, APIEntry> apis;
};

#endif // DBUSSERVICE_H
