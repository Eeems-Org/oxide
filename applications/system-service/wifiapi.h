#ifndef WIFIAPI_H
#define WIFIAPI_H

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QException>
#include <QUuid>
#include <QReadWriteLock>

#include <unistd.h>

#include "wlan.h"
#include "network.h"
#include "bss.h"

const QUuid NS = QUuid::fromString(QLatin1String("{78c28d66-f558-11ea-adc1-0242ac120002}"));

class WifiAPI : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_WIFI_INTERFACE)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(QSet<QString> blobs READ blobs)
    Q_PROPERTY(QList<QDBusObjectPath> bSSs READ bSSs)
    Q_PROPERTY(int link READ link)
public:
    WifiAPI(QObject* parent)
    : QObject(parent),
      settings("/home/root/.config/remarkable/xochitl.conf",
      QSettings::IniFormat),
      wlans(),
      networks(),
      lock()
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
            connect(item, &Wlan::BSSAdded, this, &WifiAPI::BSSAdded, Qt::QueuedConnection);
            connect(item, &Wlan::BSSRemoved, this, &WifiAPI::BSSRemoved, Qt::QueuedConnection);
            connect(item, &Wlan::BlobAdded, this, &WifiAPI::BlobAdded, Qt::QueuedConnection);
            connect(item, &Wlan::BlobRemoved, this, &WifiAPI::BlobRemoved, Qt::QueuedConnection);
            connect(item, &Wlan::NetworkAdded, this, &WifiAPI::NetworkAdded, Qt::QueuedConnection);
            connect(item, &Wlan::NetworkRemoved, this, &WifiAPI::NetworkRemoved, Qt::QueuedConnection);
            connect(item, &Wlan::NetworkSelected, this, &WifiAPI::NetworkSelected, Qt::QueuedConnection);
            connect(item, &Wlan::PropertiesChanged, this, &WifiAPI::InterfacePropertiesChanged, Qt::QueuedConnection);
            connect(item, &Wlan::ScanDone, this, &WifiAPI::ScanDone, Qt::QueuedConnection);
        }
        QDBusConnection bus = QDBusConnection::systemBus();
        if(!bus.isConnected()){
            qWarning("Failed to connect to system bus.");
            throw QException();
        }
        validateSupplicant();
        supplicant = new Wpa_Supplicant(WPA_SUPPLICANT_SERVICE, WPA_SUPPLICANT_SERVICE_PATH, bus);
        connect(supplicant, &Wpa_Supplicant::InterfaceAdded, this, &WifiAPI::InterfaceAdded);
        connect(supplicant, &Wpa_Supplicant::InterfaceRemoved, this, &WifiAPI::InterfaceRemoved);
        connect(supplicant, &Wpa_Supplicant::PropertiesChanged, this, &WifiAPI::PropertiesChanged);
        for(auto wlan : wlans){
            auto iface = wlan->iface();
            auto reply = (QDBusReply<QDBusObjectPath>)supplicant->GetInterface(iface);
            if(!reply.isValid() || reply.value().path() == "/"){
                QVariantMap args;
                args.insert("Ifname", iface);
                reply = supplicant->CreateInterface(args);
            }
            if(reply.isValid()){
                wlan->setInterface(reply.value().path());
                for(auto path : wlan->interface()->networks()){
                    auto inetwork = new INetwork(WPA_SUPPLICANT_SERVICE, path.path(), bus, wlan->interface());
                    auto properties = inetwork->properties();
                    bool found = false;
                    for(auto network : networks){
                        if(network->path() == path.path()){
                            found = true;
                            network->addNetwork(inetwork);
                            break;
                        }
                    }
                    if(!found){
                        auto network = new Network(getPath("network", properties["ssid"].toString()), properties, this);
                        networks.append(network);
                        network->addNetwork(inetwork);
                    }
                }
                for(auto path : wlan->interface()->bSSs()){
                    auto ibss = new IBSS(WPA_SUPPLICANT_SERVICE, path.path(), bus, wlan->interface());
                    bool found = false;
                    auto bssid = ibss->bSSID();
                    for(auto bss : bsss){
                        if(bss->bssid() == bssid){
                            found = true;
                            bss->addBSS(ibss);
                            break;
                        }
                    }
                    if(!found){
                        bsss.append(new BSS(getPath("bss", bssid), ibss, this));
                    }
                }
            }
        }
        loadNetworks();
        if(settings.value("wifion").toBool()){
            turnOnWifi();
        }else{
            turnOffWifi();
        }
        update();
        timer = new QTimer(this);
        timer->setSingleShot(false);
        timer->setInterval(3 * 1000); // 3 seconds
        timer->moveToThread(qApp->thread());
        connect(timer, &QTimer::timeout, this, QOverload<>::of(&WifiAPI::update));
        timer->start();
    }
    ~WifiAPI(){
        qDebug() << "Unregistering all networks";
        while(!networks.isEmpty()){
            delete networks.takeFirst();
        }
        qDebug() << "Unregistering all BSSs";
        while(!bsss.isEmpty()){
            delete bsss.takeFirst();
        }
        qDebug() << "Killing timer";
        timer->stop();
        delete timer;
    }
    QList<Wlan*> getWlans(){ return wlans; }
    QList<Interface*> getInterfaces(){ return interfaces(); }
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
        qDebug() << "Turning wifi on";
        if(m_state == Off){
            setState(Disconnected);
        }
        for(auto wlan : wlans){
            if(!wlan->isUp() && !wlan->up()){
                qDebug() << "Failed to enable " + wlan->iface();
                continue;
            }
        }
        settings.setValue("wifion", true);
        validateSupplicant();
        if(m_state != Online){
            reconnect();
        }
        return true;
    }
    Q_INVOKABLE void turnOffWifi(){
        qDebug() << "Turning wifi off";
        setState(Off);
        for(auto wlan : wlans){
            if(wlan->isUp() && !wlan->down()){
                qDebug() << "Failed to disable " + wlan->iface();
            }
        }
        settings.setValue("wifion", false);
        settings.sync();
    }
    Q_INVOKABLE QDBusObjectPath addNetwork(QString ssid, QVariantMap properties){
        Q_UNUSED(ssid)
        Q_UNUSED(properties)
        // TODO implement addNetwork
    }
    Q_INVOKABLE QDBusObjectPath getNetwork(QVariantMap properties){
        Q_UNUSED(properties)
        // TODO implement getNetwork
    }
    Q_INVOKABLE QDBusObjectPath getBSS(QVariantMap properties){
        Q_UNUSED(properties)
        // TODO implement getBSS
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
        qDebug() << "Reconnecting to wifi";
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
    QList<QDBusObjectPath> bSSs(){
        QList<QDBusObjectPath> result;
        for(auto bss : bsss){
            result.append(QDBusObjectPath(bss->path()));
        }
        return result;
    }
    int link(){
        int result = 0;
        for(auto wlan : wlans){
            int link = wlan->link();
            if(result < link){
                result = link;
            }
        }
        return result;
    }

    // Interface signals
    void BSSAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties){
        auto sPath = path.path();
        auto bssid = properties["BSSID"].toString();
        if(bssid.isEmpty()){
            return;
        }
        auto ssid = properties["SSID"].toString();
        for(auto bss : bsss){
            for(auto nPath : bss->paths()){
                if(nPath == sPath){
                    return;
                }
            }
            if(bss->ssid() == ssid && bss->bssid() == bssid){
                bss->addBSS(sPath, wlan->interface());
                return;
            }
        }
        qDebug() << "BSS added " << bssid.toUtf8().toHex() << ssid;
        bsss.append(new BSS(getPath("bss", bssid), bssid, ssid, this));
        emit bssFound(path);
    }
    void BSSRemoved(Wlan* wlan, const QDBusObjectPath& path){
        Q_UNUSED(wlan);
        auto sPath = path.path();
        for(auto bss : bsss){
            for(auto nPath : bss->paths()){
                bss->removeBSS(sPath);
            }
            if(!bss->paths().length()){
                bsss.removeAll(bss);
                emit bssRemoved(QDBusObjectPath(bss->path()));
                delete bss;
                break;
            }
        }
    }
    void BlobAdded(Wlan* wlan, const QString& name){
        Q_UNUSED(wlan);
        Q_UNUSED(name);
    }
    void BlobRemoved(Wlan* wlan, const QString& name){
        Q_UNUSED(wlan);
        Q_UNUSED(name);
    }
    void NetworkAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties){
        auto sPath = path.path();
        auto ssid = properties["ssid"].toString();
        if(ssid.isEmpty()){
            return;
        }
        ssid = ssid.mid(1, ssid.length() - 2);
        for(auto network : networks){
            for(auto nPath : network->paths()){
                if(nPath == sPath){
                    return;
                }
            }
            if(network->ssid() == ssid){
                network->addNetwork(sPath, wlan->interface());
                return;
            }
        }
        qDebug() << "Network added " << ssid;
        auto network = new Network(getPath("network", ssid), properties, this);
        network->addNetwork(path.path(), wlan->interface());
        networks.append(network);
        emit networkAdded(path);
    }
    void NetworkRemoved(Wlan* wlan, const QDBusObjectPath& path){
        Q_UNUSED(wlan);
        auto sPath = path.path();
        for(auto network : networks){
            for(auto nPath : network->paths()){
                if(nPath == sPath){
                    network->removeNetwork(sPath);
                }
            }
            if(!network->paths().length()){
                networks.removeAll(network);
                emit networkRemoved(QDBusObjectPath(network->path()));
                qDebug() << "Network removed " << network->ssid();
                delete network;
                break;
            }
        }
    }
    void NetworkSelected(Wlan* wlan, const QDBusObjectPath& path){
        Q_UNUSED(wlan);
        Q_UNUSED(path);
    }
    void InterfacePropertiesChanged(Wlan* wlan, const QVariantMap& properties){
        Q_UNUSED(wlan);
        Q_UNUSED(properties);
    }
    void ScanDone(Wlan* wlan, bool success){
        Q_UNUSED(wlan);
        Q_UNUSED(success);
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
            if(properties["Ifname"].toString() == wlan->iface()){
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
    QTimer* timer;
    QSettings settings;
    QList<Wlan*> wlans;
    QList<Network*> networks;
    QList<BSS*> bsss;
    Wpa_Supplicant* supplicant;
    QReadWriteLock lock;
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

    void validateSupplicant(){
        QDBusConnection bus = QDBusConnection::systemBus();
        QStringList serviceNames = bus.interface()->registeredServiceNames();
        if (!serviceNames.contains(WPA_SUPPLICANT_SERVICE)){
            if(system("systemctl --quiet is-active wpa_supplicant")){
                qDebug() << "Starting wpa_supplicant...";
                if(system("systemctl --quiet start wpa_supplicant")){
                    qCritical() << "Failed to start wpa_supplicant";
                    throw QException();
                }
            }
            qDebug() << "Waiting for wpa_supplicant dbus service...";
            while(!serviceNames.contains(WPA_SUPPLICANT_SERVICE)){
                usleep(1000);
                serviceNames = bus.interface()->registeredServiceNames();
            }
        }
    }

    void loadNetworks(){
        qDebug() << "Loading networks from settings...";
        settings.sync();
        settings.beginGroup("wifinetworks");
        QList<QVariantMap> registeredNetworks;
        for(const QString& childKey : settings.allKeys()){
            QVariantMap network = settings.value(childKey).toMap();
            registeredNetworks.append(network);
        }
        settings.endGroup();
        qDebug() << "Registering networks...";
        for(auto registration : registeredNetworks){
            bool found = false;
            auto ssid = registration["ssid"].toString();
            auto protocol = registration["protocol"].toString();
            for(auto network : networks){
                if(network->ssid() == ssid && network->protocol() == protocol){
                    qDebug() << "  Found network" << network->ssid();
                    found = true;
                    network->setPassword(registration["password"].toString());
                    network->setEnabled(true);
                    break;
                }
            }
            if(!found){
                QVariantMap args;
                auto network = new Network(getPath("network", ssid), ssid, QVariantMap(), this);
                network->setProtocol(protocol);
                if(registration.contains("password")){
                    network->setPassword(registration["password"].toString());
                }
                network->registerNetwork();
                network->setEnabled(true);
                qDebug() << "  Registered network" << ssid;
            }
        }
        qDebug() << "Loaded networks.";
    }

    void update(){
        settings.sync();
        bool enabled = settings.value("wifion").toBool();
        if(enabled && m_state == Off){
            turnOnWifi();
        }else if(!enabled && m_state != Off){
            turnOffWifi();
        }
        State state = Off;
        if(enabled){
            state = Disconnected;
            for(auto wlan : wlans){
                if(!wlan->isUp()){
                    continue;
                }
                if(wlan->operstate() == "up"){
                    state = Offline;
                }
                if(wlan->isConnected()){
                    state = Online;
                }
                if(state == Online){
                    break;
                }
            }
        }
        if(m_state != state){
            setState(state);
        }
    }
    QString getPath(QString type, QString id){
        if(type == "network"){
            id= QUuid::createUuidV5(NS, id).toString(QUuid::Id128);
        }else if(type == "bss"){
            id = id.toUtf8().toHex();
        }
        return QString(OXIDE_SERVICE_PATH "/") + type + "/" + id;
    }
};

#endif // WIFIAPI_H
