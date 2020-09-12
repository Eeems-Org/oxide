#ifndef WIFIAPI_H
#define WIFIAPI_H

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QException>
#include <QFileInfo>

#include <unistd.h>

#include "wlan.h"

class WifiAPI : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("D-Bus Interface", OXIDE_WIFI_INTERFACE)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(QSet<QString> blobs READ blobs)
    Q_PROPERTY(QList<QDBusObjectPath> BSSs READ BSSs)
public:
    WifiAPI(QObject* parent)
    : QObject(parent),
      settings("/home/root/.config/remarkable/xochitl.conf",
      QSettings::IniFormat),
      wlans()
    {
        QDir dir("/sys/class/net");
        qDebug() << "Looking for wireless devices...";
        for(auto path : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)){
            qDebug() << ("  Checking " + path + "...").toStdString().c_str();
            Wlan* item = new Wlan(dir.path() + "/" + path, this);
            if(!item->hasDirectory("wireless")){
                qDebug() << "    Not a wireless device";
                continue;
            }
            qDebug() << "    Wireless device found!";
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
        connect(supplicant, &Wpa_Supplicant::InterfaceAdded, this, &WifiAPI::InterfaceAdded);
        connect(supplicant, &Wpa_Supplicant::InterfaceRemoved, this, &WifiAPI::InterfaceRemoved);
        connect(supplicant, &Wpa_Supplicant::PropertiesChanged, this, &WifiAPI::PropertiesChanged);
        for(auto wlan : wlans){
            auto iface = QFileInfo(wlan->path().c_str()).fileName();
            auto reply = (QDBusReply<QDBusObjectPath>)supplicant->GetInterface(iface);
            if(!reply.isValid() || reply.value().path() == "/"){
                QVariantMap args;
                args.insert("Ifname", iface);
                reply = supplicant->CreateInterface(args);
            }
            if(reply.isValid()){
                wlan->setInterface(reply.value().path());

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
        for(auto interface : interfaces()){
            replies.append(interface->Scan(args));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void reconnect(){
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->Reconnect());
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void reassosiate(){
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->Reassociate());
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void flushBSSCache(uint age){
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->FlushBSS(age));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void addBlob(QString name, QByteArray blob){
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->AddBlob(name, blob));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void removeBlob(QString name){
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->RemoveBlob(name));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE QByteArray getBlob(QString name, QByteArray blob){
        for(auto interface : interfaces()){
            auto reply = (QDBusReply<QByteArray>)interface->AddBlob(name, blob);
            if(reply.isValid()){
                return reply;
            }
        }
        return QByteArray();
    }
    QSet<QString> blobs(){
        QSet<QString> result;
        for(auto wlan : wlans){
            result.unite(wlan->blobs());
        }
        return result;
    }
    QList<QDBusObjectPath> BSSs(){
        QList<QDBusObjectPath> result;
        for(auto wlan : wlans){
            result.append(wlan->BSSs());
        }
        return result;
    }

    // Interface signals
    void BSSAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties){
        Q_UNUSED(wlan);
        Q_UNUSED(properties);
        emit bssFound(path);
    }
    void BSSRemoved(Wlan* wlan, const QDBusObjectPath& path){
        Q_UNUSED(wlan);
        emit bssRemoved(path);
    }
    void BlobAdded(Wlan* wlan, const QString& name){
        Q_UNUSED(wlan);
        Q_UNUSED(name);
    }
    void BlobRemoved(Wlan* wlan, const QString& name){
        Q_UNUSED(wlan);
        Q_UNUSED(name);
    }
    void Certification(Wlan* wlan, const QVariantMap& certification){
        Q_UNUSED(wlan);
        Q_UNUSED(certification);
    }
    void EAP(Wlan* wlan, const QString& status, const QString& parameter){
        Q_UNUSED(wlan);
        Q_UNUSED(status);
        Q_UNUSED(parameter);
    }
    void NetworkAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties){
        Q_UNUSED(wlan);
        Q_UNUSED(properties);
        emit networkAdded(path);
    }
    void NetworkRemoved(Wlan* wlan, const QDBusObjectPath &path){
        Q_UNUSED(wlan);
        emit networkRemoved(path);
    }
    void NetworkSelected(Wlan* wlan, const QDBusObjectPath& path){
        Q_UNUSED(wlan);
        Q_UNUSED(path);
    }
    void ProbeRequest(Wlan* wlan, const QVariantMap& args){
        Q_UNUSED(wlan);
        Q_UNUSED(args);
    }
    void InterfacePropertiesChanged(Wlan* wlan, const QVariantMap& properties){
        Q_UNUSED(wlan);
        Q_UNUSED(properties);
    }
    void ScanDone(Wlan* wlan, bool success){
        Q_UNUSED(wlan);
        Q_UNUSED(success);
    }
    void StaAuthorized(Wlan* wlan, const QString& name){
        Q_UNUSED(wlan);
        Q_UNUSED(name);
    }
    void StaDeauthorized(Wlan* wlan, const QString& name){
        Q_UNUSED(wlan);
        Q_UNUSED(name);
    }
    // BSS signals
    void BSSPropertiesChanged(const QVariantMap &properties){
        Q_UNUSED(properties);
    }

Q_SIGNALS:
    void stateChanged(int);
    void networkAdded(QDBusObjectPath);
    void networkRemoved(QDBusObjectPath);
    void networkConnected(QDBusObjectPath);
    void disconnected();
    void bssFound(QDBusObjectPath);
    void bssRemoved(QDBusObjectPath);


private slots:
    // wpa_supplicant signals
    void InterfaceAdded(const QDBusObjectPath &path, const QVariantMap &properties){
        for(auto wlan : wlans){
            auto iface = QFileInfo(wlan->path().c_str()).fileName();
            if(properties["Ifname"].toString() == iface){
                wlan->setInterface(path.path());
                break;
            }
        }
    }
    void InterfaceRemoved(const QDBusObjectPath &path){
        for(auto wlan : wlans){
            if(wlan->interface() != nullptr && wlan->interface()->path() == path.path()){
                wlan->removeInterface();
            }
        }
    }
    void PropertiesChanged(const QVariantMap &properties){
        Q_UNUSED(properties);
    }
private:
    QSettings settings;
    QList<Wlan*> wlans;
    Wpa_Supplicant* supplicant;
    int m_state = Unknown;

    QList<Interface*> interfaces(){
        QList<Interface*> result;
        for(auto wlan : wlans){
            if(wlan->interface() != nullptr){
                result.append(wlan->interface());
            }
        }
        return result;
    }

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
