#ifndef WIFIAPI_H
#define WIFIAPI_H

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QException>
#include <QUuid>

#include <unistd.h>

#include "apibase.h"
#include "wlan.h"
#include "network.h"
#include "bss.h"

#define wifiAPI WifiAPI::singleton()

class WifiAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_WIFI_INTERFACE)
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(QSet<QString> blobs READ blobs)
    Q_PROPERTY(QList<QDBusObjectPath> bSSs READ bSSs)
    Q_PROPERTY(int link READ link)
    Q_PROPERTY(QDBusObjectPath network READ network)
    Q_PROPERTY(QList<QDBusObjectPath> networks READ getNetworkPaths)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
public:
    static WifiAPI* singleton(WifiAPI* self = nullptr){
        static WifiAPI* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    WifiAPI(QObject* parent)
    : APIBase(parent),
      m_enabled(false),
      settings("/home/root/.config/remarkable/xochitl.conf", QSettings::IniFormat),
      wlans(),
      networks(),
      m_state(Unknown),
      m_currentNetwork("/"),
      m_link(0),
      m_scanning(false)
    {
        singleton(this);
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
                auto interface = wlan->interface();
                for(auto path : interface->networks()){
                    bool found = false;
                    for(auto network : networks){
                        if(network->path() == path.path()){
                            found = true;
                            network->addNetwork(path.path(), interface);
                            break;
                        }
                    }
                    if(!found){
                        INetwork inetwork(WPA_SUPPLICANT_SERVICE, path.path(), bus, interface);
                        auto properties = inetwork.properties();
                        auto network = new Network(getPath("network", properties["ssid"].toString()), properties, this);
                        network->addNetwork(path.path(), interface);
                        networks.append(network);
                    }
                }
                for(auto path : wlan->interface()->bSSs()){
                    auto ibss = new IBSS(WPA_SUPPLICANT_SERVICE, path.path(), bus, wlan->interface());
                    bool found = false;
                    auto bssid = ibss->bSSID();
                    for(auto bss : bsss){
                        if(bss->bssid() == bssid){
                            found = true;
                            bss->addBSS(path.path());
                            break;
                        }
                    }
                    if(!found){
                        auto bss = new BSS(getPath("bss", bssid), ibss, this);
                        bsss.append(bss);
                    }else{
                        ibss->deleteLater();
                    }
                }
            }
        }
        timer = new QTimer(this);
        timer->setSingleShot(false);
        timer->setInterval(3 * 1000); // 3 seconds
        timer->moveToThread(qApp->thread());
        connect(timer, &QTimer::timeout, this, QOverload<>::of(&WifiAPI::update));
        loadNetworks();
        if(settings.value("wifion").toBool()){
            enable();
        }else{
            disable();
        }
        update();
        setEnabled(m_enabled);
        timer->start();
    }
    ~WifiAPI(){
        qDebug() << "Unregistering all networks";
        while(!networks.isEmpty()){
            networks.takeFirst()->deleteLater();
        }
        qDebug() << "Unregistering all BSSs";
        while(!bsss.isEmpty()){
            bsss.takeFirst()->deleteLater();
        }
        qDebug() << "Killing timer";
        timer->stop();
        timer->deleteLater();
    }
    void setEnabled(bool enabled){
        qDebug() << "Wifi API" << enabled;
        m_enabled = enabled;
        if(enabled){
            for(auto network : networks){
                network->registerPath();
            }
            for(auto bss : bsss){
                bss->registerPath();
            }
        }else{
            for(auto network : networks){
                network->unregisterPath();
            }
            for(auto bss : bsss){
                bss->unregisterPath();
            }
        }
    }
    bool getEnabled(){ return m_enabled; }
    QList<Wlan*> getWlans(){ return wlans; }
    QList<Interface*> getInterfaces(){ return interfaces(); }
    enum State { Unknown, Off, Disconnected, Offline, Online};
    Q_ENUM(State)

    int state(){
        if(!hasPermission("wifi")){
            return Unknown;
        }
        return m_state;
    }
    void setState(int state){
        if(!hasPermission("wifi")){
            return;
        }
        if(state < Unknown || state > Online){
            throw QException{};
        }
        m_state = state;
        emit stateChanged(state);
    }

    Q_INVOKABLE bool enable(){
        if(!hasPermission("wifi")){
            return false;
        }
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
    Q_INVOKABLE void disable(){
        if(!hasPermission("wifi")){
            return;
        }
        qDebug() << "Turning wifi off";
        setState(Off);
        for(auto wlan : wlans){
            if(wlan->isUp() && !wlan->down()){
                qDebug() << "Failed to disable " + wlan->iface();
            }
        }
        flushBSSCache(0);
        settings.setValue("wifion", false);
        settings.sync();
    }
    Q_INVOKABLE QDBusObjectPath addNetwork(QString ssid, QVariantMap properties){
        if(!hasPermission("wifi")){
            return QDBusObjectPath("/");
        }
        Q_UNUSED(properties)
        for(auto network : networks){
            if(network->ssid() == ssid){
                return QDBusObjectPath(network->path());
            }
        }
        auto network = new Network(getPath("network", ssid), ssid, properties, this);
        network->registerNetwork();
        network->setEnabled(true);
        if(m_enabled){
            network->registerPath();
        }
        networks.append(network);
        auto path = QDBusObjectPath(network->path());
        emit networkAdded(path);
        return path;
    }
    Q_INVOKABLE QList<QDBusObjectPath> getNetwork(QVariantMap properties){
        QList<QDBusObjectPath> result;
        if(!hasPermission("wifi")){
            return result;
        }
        for(auto network : networks){
            bool found = true;
            auto props = network->properties();
            for(auto key : properties.keys()){
                auto value = properties[key];
                if(
                    (key == "ssid" && value.toString() != network->ssid())
                    || (key == "protococl" && value.toString() != network->protocol())
                    || (key != "ssid" && key != "protocol" && value != props[key])
                ){
                    found = false;
                    break;
                }
            }
            if(found){
                result.append(QDBusObjectPath(network->path()));
            }
        }
        return result;
    }
    Q_INVOKABLE QList<QDBusObjectPath> getBSS(QVariantMap properties){
        QList<QDBusObjectPath> result;
        if(!hasPermission("wifi")){
            return result;
        }
        for(auto bss : bsss){
            bool found = true;
            for(auto key : properties.keys()){
                auto value = properties[key];
                if(properties[key] != bss->property(key.toStdString().c_str())){
                    found = false;
                    break;
                }
            }
            if(found){
                result.append(QDBusObjectPath(bss->path()));
            }
        }
        return result;
    }
    Q_INVOKABLE void scan(bool active = false){
        if(!hasPermission("wifi")){
            return;
        }
        if(!m_scanning){
            m_scanning = true;
            emit scanningChanged(true);
        }
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
        if(!hasPermission("wifi")){
            return;
        }
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
        if(!hasPermission("wifi")){
            return;
        }
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->Reassociate());
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void disconnect(){
        if(!hasPermission("wifi")){
            return;
        }
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->Disconnect());
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void flushBSSCache(uint age){
        if(!hasPermission("wifi")){
            return;
        }
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->FlushBSS(age));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void addBlob(QString name, QByteArray blob){
        if(!hasPermission("wifi")){
            return;
        }
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->AddBlob(name, blob));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE void removeBlob(QString name){
        if(!hasPermission("wifi")){
            return;
        }
        QList<QDBusPendingReply<void>> replies;
        for(auto interface : interfaces()){
            replies.append(interface->RemoveBlob(name));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }
    Q_INVOKABLE QByteArray getBlob(QString name, QByteArray blob){
        if(!hasPermission("wifi")){
            return QByteArray();
        }
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
        if(!hasPermission("wifi")){
            return result;
        }
        for(auto wlan : wlans){
            result.unite(wlan->blobs());
        }
        return result;
    }
    QList<QDBusObjectPath> bSSs(){
        QList<QDBusObjectPath> result;
        if(!hasPermission("wifi")){
            return result;
        }
        for(auto bss : bsss){
            result.append(QDBusObjectPath(bss->path()));
        }
        return result;
    }
    int link(){
        if(!hasPermission("wifi")){
            return 0;
        }
        int result = 0;
        for(auto wlan : wlans){
            int link = wlan->link();
            if(result < link){
                result = link;
            }
        }
        return result;
    }
    QDBusObjectPath network(){
        if(!hasPermission("wifi")){
            return QDBusObjectPath("/");
        }
        for(auto interface : getInterfaces()){
            auto state = interface->state();
            if(state == "associated" || state == "completed"){
                auto ipath = interface->currentNetwork().path();
                for(auto network : networks){
                    if(network->paths().contains(ipath)){
                        return QDBusObjectPath(network->path());
                    }
                }
            }
        }
        return QDBusObjectPath("/");
    }
    QList<QDBusObjectPath> getNetworkPaths(){
        QList<QDBusObjectPath> result;
        for(auto network : networks){
            result.append(QDBusObjectPath(network->path()));
        }
        return result;
    }
    bool scanning(){
        if(!hasPermission("wifi")){
            return false;
        }
        for(auto interface : interfaces()){
            if(interface->scanning()){
                if(!m_scanning){
                    m_scanning = true;
                    emit scanningChanged(true);
                }
                return true;
            }
        }
        return false;
    }

    // Interface signals
    void BSSAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties){
        Q_UNUSED(wlan);
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
                bss->addBSS(sPath);
                return;
            }
        }
        qDebug() << "BSS added " << bssid.toUtf8().toHex() << ssid;
        auto bss = new BSS(getPath("bss", bssid), bssid, ssid, this);
        if(m_enabled){
            bss->registerPath();
        }
        bsss.append(bss);
        emit bssFound(QDBusObjectPath(bss->path()));
    }
    void BSSRemoved(Wlan* wlan, const QDBusObjectPath& path){
        Q_UNUSED(wlan);
        auto sPath = path.path();
        QMutableListIterator<BSS*> i(bsss);
        while(i.hasNext()){
            auto bss = i.next();
            bss->removeBSS(sPath);
            if(!bss->paths().length()){
                i.remove();
                emit bss->removed();
                emit bssRemoved(QDBusObjectPath(bss->path()));
                bss->deleteLater();
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
        if(m_enabled){
            network->registerPath();
        }
        networks.append(network);
        emit networkAdded(path);
    }
    void NetworkRemoved(Wlan* wlan, const QDBusObjectPath& path){
        Q_UNUSED(wlan);
        auto sPath = path.path();
        QMutableListIterator<Network*> i(networks);
        while(i.hasNext()){
            auto network = i.next();
            network->removeNetwork(sPath);
            if(!network->paths().length()){
                i.remove();
                emit network->removed();
                emit networkRemoved(QDBusObjectPath(network->path()));
                qDebug() << "Network removed " << network->ssid();
                network->deleteLater();
            }
        }
    }
    void NetworkSelected(Wlan* wlan, const QDBusObjectPath& path){
        Q_UNUSED(wlan);
        for(auto network : networks){
            if(network->paths().contains(path.path())){
                qDebug() << "Network selected" << path.path();
                break;
            }
        }
    }
    void InterfacePropertiesChanged(Wlan* wlan, const QVariantMap& properties){
        Q_UNUSED(wlan);
        Q_UNUSED(properties);
    }
    void ScanDone(Wlan* wlan, bool success){
        Q_UNUSED(wlan)
        Q_UNUSED(success)
        m_scanning = false;
        emit scanningChanged(false);
    }
    // BSS signals
    void BSSPropertiesChanged(const QVariantMap& properties){
        Q_UNUSED(properties);
    }

signals:
    void stateChanged(int);
    void linkChanged(int);
    void networkAdded(QDBusObjectPath);
    void networkRemoved(QDBusObjectPath);
    void networkConnected(QDBusObjectPath);
    void disconnected();
    void bssFound(QDBusObjectPath);
    void bssRemoved(QDBusObjectPath);
    void scanningChanged(bool);


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
                auto interface = wlan->interface();
                for(auto network : networks){
                    network->removeInterface(interface);
                    if(network->paths().empty()){
                        emit network->removed();
                        emit networkRemoved(QDBusObjectPath(network->path()));
                        qDebug() << "Network removed " << network->ssid();
                    }
                }
                wlan->removeInterface();
            }
        }
    }
    void PropertiesChanged(const QVariantMap &properties){
        Q_UNUSED(properties);
    }
private:
    bool m_enabled;
    QTimer* timer;
    QSettings settings;
    QList<Wlan*> wlans;
    QList<Network*> networks;
    int m_state;
    QDBusObjectPath m_currentNetwork;
    int m_link;
    QList<BSS*> bsss;
    bool m_scanning;
    Wpa_Supplicant* supplicant;

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
                struct timespec args{
                    .tv_sec = 1,
                    .tv_nsec = 0,
                }, res;
                nanosleep(&args, &res);
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
                if(m_enabled){
                    network->registerPath();
                }
                networks.append(network);
                emit networkAdded(QDBusObjectPath(network->path()));
                qDebug() << "  Registered network" << ssid;
            }
        }
        qDebug() << "Loaded networks.";
    }

    void update(){
        settings.sync();
        auto state = getCurrentState();
        bool enabled = settings.value("wifion").toBool();
        if(enabled && state == Off){
            enable();
            state = getCurrentState();
        }else if(!enabled && state != Off){
            disable();
            state = getCurrentState();
        }
        auto path = network().path();
        if((state == Online || state == Offline) && m_currentNetwork.path() != path){
            qDebug() << "Connected to" << path;
            m_currentNetwork.setPath(path);
            emit networkConnected(m_currentNetwork);
        }else if((state == Disconnected || state == Off) && m_currentNetwork.path() != "/"){
            qDebug() << "Disconnected from" << m_currentNetwork.path();
            m_currentNetwork.setPath("/");
            emit disconnected();
        }
        if(m_state != state){
            setState(state);
        }
        auto clink = link();
        if(m_link != clink){
            m_link = clink;
            emit linkChanged(clink);
        }
    }
    State getCurrentState(){
        State state = Off;
        for(auto wlan : wlans){
            if(!wlan->isUp()){
                continue;
            }
            if(state == Off){
                state = Disconnected;
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
        return state;
    }
    QString getPath(QString type, QString id){
        static const QUuid NS = QUuid::fromString(QLatin1String("{78c28d66-f558-11ea-adc1-0242ac120002}"));
        if(type == "network"){
            id= QUuid::createUuidV5(NS, id).toString(QUuid::Id128);
        }else if(type == "bss"){
            id = id.toUtf8().toHex();
        }
        if(id.isEmpty()){
            id = QUuid::createUuid().toString(QUuid::Id128);
        }
        return QString(OXIDE_SERVICE_PATH "/") + type + "/" + id;
    }
};

#endif // WIFIAPI_H
