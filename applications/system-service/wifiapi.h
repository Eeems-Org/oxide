#ifndef WIFIAPI_H
#define WIFIAPI_H

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QException>
#include <QFileInfo>

#include <unistd.h>

#include "dbussettings.h"
#include "sysobject.h"
#include "wpa_supplicant.h"

typedef FiW1Wpa_supplicant1Interface Wpa_Supplicant;
typedef FiW1Wpa_supplicant1BSSInterface BSS;
typedef FiW1Wpa_supplicant1InterfaceInterface Interface;

class WifiAPI : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("D-Bus Interface", OXIDE_WIFI_INTERFACE)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
public:
    WifiAPI(QObject* parent)
    : QObject(parent),
      settings("/home/root/.config/remarkable/xochitl.conf",
      QSettings::IniFormat), wlans(), interfaces()
    {
        QDir dir("/sys/class/net");
        qDebug() << "Looking for wireless devices...";
        for(auto path : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)){
            qDebug() << ("  Checking " + path + "...").toStdString().c_str();
            SysObject item(dir.path() + "/" + path);
            if(item.hasDirectory("wireless")){
                qDebug() << "    Not a wireless devices";
                break;
            }
            wlans.append(item);
        }
        QDBusConnection bus = QDBusConnection::systemBus();
        if(!bus.isConnected()){
            qWarning("Failed to connect to system bus.");
            throw QException();
        }
        QStringList serviceNames = bus.interface()->registeredServiceNames();
        if (!serviceNames.contains(WPA_SUPPLICANT_SERVICE)){
            if(!system("systemctl --quiet is-active wpa_supplicant")){
                qCritical() << "wpa_supplicant is running, but not active?";
                throw QException();
            }
            qDebug() << "Starting wpa_supplicant...";
            if(system("systemctl --quiet start wpa_supplicant")){
                qCritical() << "Failed to start wpa_supplicant";
                throw QException();
            }
            qDebug() << "Waiting for wpa_supplicant dbus service...";
            while(!serviceNames.contains(WPA_SUPPLICANT_SERVICE)){
                usleep(1000);
                serviceNames = bus.interface()->registeredServiceNames();
            }
        }
        supplicant = new Wpa_Supplicant(WPA_SUPPLICANT_SERVICE, WPA_SUPPLICANT_SERVICE_PATH, bus);
        for(auto wlan : wlans){
            auto iface = QFileInfo(wlan.path().c_str()).fileName();
            auto reply = (QDBusReply<QDBusObjectPath>)supplicant->GetInterface(iface);
            if(!reply.isValid() || reply.value().path() == "/"){
                QVariantMap args;
                args.insert("Ifname", "wlan0");
                reply = supplicant->CreateInterface(args);
            }
            if(reply.isValid()){
                interfaces.append(new Interface(WPA_SUPPLICANT_SERVICE, reply.value().path(), bus, this));
            }
        }
    }
    enum State { Unknown, Off, Disconnected, Offline, Online};
    Q_ENUM(State)

    int state(){ return m_state; }
    void setState(int state){
        if(state < Unknown || state > Online){
            throw QException{};
        }
        m_state = state;
        emit stateChanged(state);
    }

    Q_INVOKABLE bool turnOnWifi(){
        system("ifconfig wlan0 up");
        settings.setValue("wifion", true);
        settings.sync();
        return true;
    }
    Q_INVOKABLE void turnOffWifi(){
        system("ifconfig wlan0 down");
        settings.setValue("wifion", false);
        settings.sync();
    }
    Q_INVOKABLE void scan(bool active = false){
        QMap<QString, QVariant> args;
        args["Type"] = active ? "active" : "passive";
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces){
            replies.append(interface->Scan(args));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }

Q_SIGNALS:
    void stateChanged(int);

private:
    QSettings settings;
    QList<SysObject> wlans;
    QList<Interface*> interfaces;
    Wpa_Supplicant* supplicant;
    int m_state = Unknown;

    void update(){
        settings.sync();
        if(settings.value("wifion").toBool()){
            turnOnWifi();
        }else{
            turnOffWifi();
        }
    }
};

#endif // WIFIAPI_H
