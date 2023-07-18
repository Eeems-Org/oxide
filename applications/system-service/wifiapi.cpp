#include "wifiapi.h"

#include <QDebug>
#include <QDir>
#include <QUuid>
#include <QException>

#include <unistd.h>
#include <liboxide.h>

WifiAPI* WifiAPI::__singleton(WifiAPI* self){
    static WifiAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}

WifiAPI::WifiAPI(QObject* parent)
    : APIBase(parent),
      m_enabled(false),
      wlans(),
      networks(),
      m_state(Unknown),
      m_currentNetwork("/"),
      m_link(0),
      m_scanning(false)
{
    Oxide::Sentry::sentry_transaction("wifi", "init", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::sentry_span(t, "singleton", "Setup singleton", [this]{
            __singleton(this);
        });
        Oxide::Sentry::sentry_span(t, "sysfs", "Finding wireless devices", [this](Oxide::Sentry::Span* s){
            QDir dir("/sys/class/net");
            O_INFO("Looking for wireless devices...");
            for(auto path : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)){
                O_INFO(("  Checking " + path + "...").toStdString().c_str());
                Wlan* item = new Wlan(dir.path() + "/" + path, this);
                if(!item->hasDirectory("wireless")){
                    O_INFO("    Not a wireless device");
                    continue;
                }
                O_INFO("    Wireless device found!");
                Oxide::Sentry::sentry_span(s, path.toStdString(), "Connect to wireless device", [this, item]{
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
                });
            }
        });
        Oxide::Sentry::sentry_span(t, "wpa_supplicant", "Connect to the wpa_supplicant", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::Span* span = Oxide::Sentry::start_span(s, "connect", "Connect to DBus interface");
            QDBusConnection bus = QDBusConnection::systemBus();
            if(!bus.isConnected()){
                O_WARNING("Failed to connect to system bus.");
                throw QException();
            }
            validateSupplicant();
            supplicant = new Wpa_Supplicant(WPA_SUPPLICANT_SERVICE, WPA_SUPPLICANT_SERVICE_PATH, bus);
            connect(supplicant, &Wpa_Supplicant::InterfaceAdded, this, &WifiAPI::InterfaceAdded);
            connect(supplicant, &Wpa_Supplicant::InterfaceRemoved, this, &WifiAPI::InterfaceRemoved);
            connect(supplicant, &Wpa_Supplicant::PropertiesChanged, this, &WifiAPI::PropertiesChanged);
            Oxide::Sentry::stop_span(span);
            delete span;
            Oxide::Sentry::sentry_span(s, "wlans", "Setting up wlan instances", [this, bus](Oxide::Sentry::Span* s){
                for(auto wlan : wlans){
                    Oxide::Sentry::sentry_span(s, wlan->iface().toStdString(), "Load current networks and BSSs", [this, bus, wlan]{
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
                                    network->setEnabled(true);
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
                    });
                }
            });
        });
        Oxide::Sentry::sentry_span(t, "load", "Load state", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "timer", "Setup timer", [this]{
                timer = new QTimer(this);
                timer->setSingleShot(false);
                timer->setInterval(3 * 1000); // 3 seconds
                timer->moveToThread(qApp->thread());
                connect(timer, &QTimer::timeout, this, QOverload<>::of(&WifiAPI::update));
            });
            Oxide::Sentry::sentry_span(s, "networks", "Load networks from disk", [this]{
                loadNetworks();
            });
            Oxide::Sentry::sentry_span(s, "state", "Syncronize state with settings", [this]{
                m_state = getCurrentState();
                if(xochitlSettings.wifion()){
                    enable();
                }else{
                    disable();
                }
            });
            Oxide::Sentry::sentry_span(s, "update", "Update current state", [this]{
                update();
            });
            Oxide::Sentry::sentry_span(s, "setEnabled", "Enable/disable API objects", [this]{
                setEnabled(m_enabled);
            });
            Oxide::Sentry::sentry_span(s, "timer", "Start timer", [this]{
                timer->start();
            });
        });
    });
}

WifiAPI::~WifiAPI(){
    O_INFO("Unregistering all networks");
    while(!networks.isEmpty()){
        networks.takeFirst()->deleteLater();
    }
    O_INFO("Unregistering all BSSs");
    while(!bsss.isEmpty()){
        bsss.takeFirst()->deleteLater();
    }
    O_INFO("Killing timer");
    timer->stop();
    timer->deleteLater();
}

void WifiAPI::setEnabled(bool enabled){
    O_INFO("Wifi API" << enabled);
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

int WifiAPI::state(){
    if(!hasPermission("wifi")){
        return Unknown;
    }
    return m_state;
}

void WifiAPI::setState(int state){
    if(!hasPermission("wifi")){
        return;
    }
    if(state < Unknown || state > Online){
        throw QException{};
    }
    m_state = state;
    emit stateChanged(state);
}

bool WifiAPI::enable(){
    if(!hasPermission("wifi")){
        return false;
    }
    O_INFO("Turning wifi on");
    if(m_state == Off){
        setState(Disconnected);
    }
    if(system("rfkill unblock wifi")){
        O_INFO("Failed to enable wifi devices");
        return false;
    }
    for(auto wlan : wlans){
        if(!wlan->isUp() && !wlan->up()){
            O_INFO("Failed to enable" << wlan->iface());
            continue;
        }
    }
    xochitlSettings.set_wifion(true);
    validateSupplicant();
    auto state = getCurrentState();
    m_state = state;
    if(state != Online){
        O_INFO("State:" << state);
        reconnect();
    }
    return true;
}

void WifiAPI::disable(){
    if(!hasPermission("wifi")){
        return;
    }
    O_INFO("Turning wifi off");
    setState(Off);
    for(auto wlan : wlans){
        if(wlan->isUp() && !wlan->down()){
            O_INFO("Failed to disable" << wlan->iface());
        }
    }
    if(system("rfkill block wifi")){
        O_INFO("Failed to disable wifi devices");
    }
    flushBSSCache(0);
    xochitlSettings.set_wifion(false);
}

QDBusObjectPath WifiAPI::addNetwork(QString ssid, QVariantMap properties){
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

QList<QDBusObjectPath> WifiAPI::getNetwork(QVariantMap properties){
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

QList<QDBusObjectPath> WifiAPI::getBSS(QVariantMap properties){
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

void WifiAPI::scan(bool active){
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

void WifiAPI::reconnect(){
    if(!hasPermission("wifi")){
        return;
    }
    O_INFO("Reconnecting to wifi");
    QList<QDBusPendingReply<void>> replies;
    for(auto interface : interfaces()){
        replies.append(interface->Reconnect());
    }
    for(auto reply : replies){
        reply.waitForFinished();
    }
}

void WifiAPI::reassosiate(){
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

void WifiAPI::disconnect(){
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

void WifiAPI::flushBSSCache(uint age){
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

void WifiAPI::addBlob(QString name, QByteArray blob){
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

void WifiAPI::removeBlob(QString name){
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

QByteArray WifiAPI::getBlob(QString name, QByteArray blob){
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

QStringList WifiAPI::blobs(){
    QSet<QString> result;
    if(!hasPermission("wifi")){
        return result.values();
    }
    for(auto wlan : wlans){
        result.unite(wlan->blobs());
    }
    return result.values();
}

QList<QDBusObjectPath> WifiAPI::bSSs(){
    QList<QDBusObjectPath> result;
    if(!hasPermission("wifi")){
        return result;
    }
    for(auto bss : bsss){
        result.append(QDBusObjectPath(bss->path()));
    }
    return result;
}

int WifiAPI::link(){
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

int WifiAPI::rssi(){
    if(!hasPermission("wifi")){
        return -100;
    }
    int result = -100;
    for(auto wlan : wlans){
        if(!wlan->isUp()){
            continue;
        }
        int rssi = wlan->rssi();
        if(result < rssi){
            result = rssi;
        }
    }
    return result;
}

QDBusObjectPath WifiAPI::network(){
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

QList<QDBusObjectPath> WifiAPI::getNetworkPaths(){
    QList<QDBusObjectPath> result;
    for(auto network : networks){
        result.append(QDBusObjectPath(network->path()));
    }
    return result;
}

bool WifiAPI::scanning(){
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

void WifiAPI::BSSAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties){
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
    O_INFO("BSS added " << bssid.toUtf8().toHex() << ssid);
    auto bss = new BSS(getPath("bss", bssid), bssid, ssid, this);
    if(m_enabled){
        bss->registerPath();
    }
    bsss.append(bss);
    emit bssFound(QDBusObjectPath(bss->path()));
}

void WifiAPI::BSSRemoved(Wlan* wlan, const QDBusObjectPath& path){
    Q_UNUSED(wlan);
    auto sPath = path.path();
    // TODO - Use STL style iterators https://doc.qt.io/qt-5/containers.html#stl-style-iterators
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

void WifiAPI::BlobAdded(Wlan* wlan, const QString& name){
    Q_UNUSED(wlan);
    Q_UNUSED(name);
}

void WifiAPI::BlobRemoved(Wlan* wlan, const QString& name){
    Q_UNUSED(wlan);
    Q_UNUSED(name);
}

void WifiAPI::NetworkAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties){
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
    O_INFO("Network added " << ssid);
    auto network = new Network(getPath("network", ssid), properties, this);
    network->addNetwork(path.path(), wlan->interface());
    if(m_enabled){
        network->registerPath();
    }
    networks.append(network);
    emit networkAdded(path);
}

void WifiAPI::NetworkRemoved(Wlan* wlan, const QDBusObjectPath& path){
    Q_UNUSED(wlan);
    auto sPath = path.path();
    // TODO - Use STL style iterators https://doc.qt.io/qt-5/containers.html#stl-style-iterators
    QMutableListIterator<Network*> i(networks);
    while(i.hasNext()){
        auto network = i.next();
        network->removeNetwork(sPath);
        if(!network->paths().length()){
            i.remove();
            emit network->removed();
            emit networkRemoved(QDBusObjectPath(network->path()));
            O_INFO("Network removed " << network->ssid());
            network->deleteLater();
        }
    }
}

void WifiAPI::NetworkSelected(Wlan* wlan, const QDBusObjectPath& path){
    Q_UNUSED(wlan);
    for(auto network : networks){
        if(network->paths().contains(path.path())){
            O_INFO("Network selected" << path.path());
            break;
        }
    }
}

void WifiAPI::InterfacePropertiesChanged(Wlan* wlan, const QVariantMap& properties){
    Q_UNUSED(wlan);
    Q_UNUSED(properties);
}

void WifiAPI::ScanDone(Wlan* wlan, bool success){
    Q_UNUSED(wlan)
    Q_UNUSED(success)
    m_scanning = false;
    emit scanningChanged(false);
}

void WifiAPI::BSSPropertiesChanged(const QVariantMap& properties){
    Q_UNUSED(properties);
}

void WifiAPI::InterfaceAdded(const QDBusObjectPath& path, const QVariantMap& properties){
    for(auto wlan : wlans){
        if(properties["Ifname"].toString() == wlan->iface()){
            wlan->setInterface(path.path());
            break;
        }
    }
}

void WifiAPI::InterfaceRemoved(const QDBusObjectPath& path){
    for(auto wlan : wlans){
        if(wlan->interface() != nullptr && wlan->interface()->path() == path.path()){
            auto interface = wlan->interface();
            for(auto network : networks){
                network->removeInterface(interface);
                if(network->paths().empty()){
                    emit network->removed();
                    emit networkRemoved(QDBusObjectPath(network->path()));
                    O_INFO("Network removed " << network->ssid());
                }
            }
            wlan->removeInterface();
        }
    }
}

void WifiAPI::PropertiesChanged(const QVariantMap& properties){
    Q_UNUSED(properties);
}

QList<Interface*> WifiAPI::interfaces(){
    QList<Interface*> result;
    for(auto wlan : wlans){
        if(wlan->interface() != nullptr){
            result.append(wlan->interface());
        }
    }
    return result;
}

void WifiAPI::validateSupplicant(){
    QDBusConnection bus = QDBusConnection::systemBus();
    QStringList serviceNames = bus.interface()->registeredServiceNames();
    if (!serviceNames.contains(WPA_SUPPLICANT_SERVICE)){
        if(system("systemctl --quiet is-active wpa_supplicant")){
            O_INFO("Starting wpa_supplicant...");
            if(system("systemctl --quiet start wpa_supplicant")){
                qCritical() << "Failed to start wpa_supplicant";
                throw QException();
            }
        }
        O_INFO("Waiting for wpa_supplicant dbus service...");
        while(!serviceNames.contains(WPA_SUPPLICANT_SERVICE)){
            timespec args{
                .tv_sec = 1,
                .tv_nsec = 0
            };
            nanosleep(&args, NULL);
            serviceNames = bus.interface()->registeredServiceNames();
        }
    }
}

void WifiAPI::loadNetworks(){
    O_INFO("Loading networks from settings...");
    QList<QMap<QString, QVariant>> registeredNetworks = xochitlSettings.wifinetworks().values();
    O_INFO("Registering networks...");
    for(auto registration : registeredNetworks){
        bool found = false;
        auto ssid = registration["ssid"].toString();
        auto protocol = registration["protocol"].toString();
        for(auto network : networks){
            if(network->ssid() == ssid && network->protocol() == protocol){
                O_INFO("  Found network" << network->ssid());
                found = true;
                if(network->password() != registration["password"].toString()){
                    network->setPassword(registration["password"].toString());
                }
                if(!network->enabled()){
                    network->setEnabled(true);
                }
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
            O_INFO("  Registered network" << ssid);
        }
    }
    O_INFO("Loaded networks.");
}

void WifiAPI::update(){
    auto state = getCurrentState();
    bool enabled = xochitlSettings.wifion();
    if(enabled && state == Off){
        enable();
        state = getCurrentState();
    }else if(!enabled && state != Off){
        disable();
        state = getCurrentState();
    }
    auto path = network().path();
    if((state == Online || state == Offline) && m_currentNetwork.path() != path){
        O_INFO("Connected to" << path);
        m_currentNetwork.setPath(path);
        emit networkConnected(m_currentNetwork);
    }else if((state == Disconnected || state == Off) && m_currentNetwork.path() != "/"){
        O_INFO("Disconnected from" << m_currentNetwork.path());
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
    auto crssi = rssi();
    if(m_rssi != crssi){
        m_rssi = crssi;
        emit rssiChanged(crssi);
    }
}

WifiAPI::State WifiAPI::getCurrentState(){
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
            break;
        }
    }
    return state;
}

QString WifiAPI::getPath(QString type, QString id){
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
