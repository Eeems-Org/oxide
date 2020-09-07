#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <QtDBus>
#include <unistd.h>

#define SERVICE QString("fi.w1.wpa_supplicant1")

class DBusInterface : public QDBusInterface {
    Q_OBJECT
public:
    DBusInterface(const QString& service, const QString& path, const QString& interface, const QDBusConnection& connection)
     : QDBusInterface(service, path, interface, connection),
       propertyInterface(service, path, "org.freedesktop.DBus.Properties", connection) {
    }
    ~DBusInterface() {};


protected:
    QDBusInterface propertyInterface;

    QDBusMessage getProperty(QString name){
         return propertyInterface.call("Get", this->interface(), name);
    }

    QDBusMessage setProperty(QString name, QVariant value){
        return propertyInterface.call("Set", this->interface(), name, value);
    }

    template <typename T>
    T setProperty(QString name, QVariant value){
        QVariant variant = (QDBusReply<QVariant>)setProperty(name, value);
        return variant.value<T>();
    }

    template <typename T>
    T getProperty(QString name){
        QVariant variant = (QDBusReply<QVariant>)getProperty(name);
        return variant.value<T>();
    }
    template <typename T>
    QList<T> getArrayProperty(QString name){
        const QDBusArgument& a = getProperty(name)
                .arguments()
                .first()
                .value<QDBusVariant>()
                .variant()
                .value<QDBusArgument>();
        a.beginArray();
        QList<T> results;
        while(!a.atEnd()){
            T item;
            a >> item;
            results.append(item);
        }
        a.endArray();
        return results;
    }
    template <typename KeyT, typename ValueT>
    QMap<KeyT, ValueT> getMapProperty(QString name){
        const QDBusArgument& a = getProperty(name)
                .arguments()
                .first()
                .value<QDBusVariant>()
                .variant()
                .value<QDBusArgument>();
        a.beginMap();
        QMap<KeyT, ValueT> results;
        while(!a.atEnd()){
            KeyT key;
            ValueT value;
            a.beginMapEntry();
            a >> key >> value;
            a.endMapEntry();
            results.insert(key, value);
        }
        a.endMap();
        return results;
    }
};

class BSS : DBusInterface {
public:
    BSS(QDBusObjectPath path, const QDBusConnection& connection)
        : DBusInterface(SERVICE, path.path(), SERVICE + ".BSS", connection),
          _path(path) {}
    QDBusObjectPath getPath(){ return _path; }
    QString BSSID(){ return getProperty<QString>("BSSID"); }
    QString SSID(){ return getProperty<QString>("SSID"); }
    QString Mode(){ return getProperty<QString>("Mode"); }
    bool Privacy(){ return getProperty<bool>("Privacy"); }
    ushort Frequency(){ return getProperty<ushort>("Frequency"); }
    uint Age(){ return getProperty<uint>("Age"); }
    QList<uint> Rates() { return getArrayProperty<uint>("Rates"); }
    QByteArray IEs(){ return getProperty<QByteArray>("IEs"); }

private:
    QDBusObjectPath _path;
};

class Network : DBusInterface {
public:
    Network(const QDBusObjectPath& path, const QDBusConnection& connection)
        : DBusInterface(SERVICE, path.path(), SERVICE + ".Network", connection),
          _path(path) {}
    QDBusObjectPath getPath(){ return _path; }
    bool setEnabled(bool enabled){ return setProperty<bool>("Enabled", QVariant::fromValue(QDBusVariant(enabled))); }
    bool Enabled(){ return getProperty<bool>("Enabled"); }
    QString SSID(){
        auto ssid = Properties()["ssid"].toString();
        return ssid.mid(1, ssid.length() - 2);
    }
    QVariantMap Properties(){ return getMapProperty<QString, QVariant>("Properties"); }

private:
    QDBusObjectPath _path;
};

class WifiManager : public DBusInterface
{
public:
    static WifiManager* singleton(){
        auto path = getWlan0Path().path();
        if(path == "/"){
            return nullptr;
        }
        return new WifiManager(path, QDBusConnection::systemBus());
    }
    static bool ensureService(){
        QDBusConnection bus = QDBusConnection::systemBus();
        if(!bus.isConnected()){
            qWarning("Failed to connect to system bus.");
            return false;
        }
        // Connect to service
        QStringList serviceNames = bus.interface()->registeredServiceNames();
        if (!serviceNames.contains(SERVICE)){
            auto wpa_supplicant = new QProcess();
            wpa_supplicant->setProgram("wpa_supplicant");
            wpa_supplicant->setArguments(QStringList() << "-u");
            if(!wpa_supplicant->startDetached()){
                qCritical() << "Failed to start wifi daemon";
                return false;
            }
            while(!serviceNames.contains(SERVICE)){
                usleep(1000);
                serviceNames = bus.interface()->registeredServiceNames();
            }
        }
        return true;
    }
    static QDBusObjectPath getWlan0Path(){
        if(!ensureService()){
            return QDBusObjectPath("/");
        }
        QDBusConnection bus = QDBusConnection::systemBus();
        QDBusInterface supplicant(SERVICE, "/fi/w1/wpa_supplicant1", "fi.w1.wpa_supplicant1", bus);
        QDBusReply<QDBusObjectPath> reply = supplicant.call("GetInterface", "wlan0");
        if(!reply.isValid()){
            QVariantMap args;
            args.insert("Ifname", "wlan0");
            reply = supplicant.call("CreateInterface", args);
        }
        return reply;
    }

    WifiManager(const QString& path, const QDBusConnection& connection)
         : DBusInterface(SERVICE, path, SERVICE + ".Interface", connection),
           xochitlSettings("/home/root/.config/remarkable/xochitl.conf", QSettings::IniFormat) {
        // this->connection().connect("fi.w1.wpa_supplicant1", wlan0Path, "fi.w1.wpa_supplicant1.Interface", "ScanDone", this, SLOT(scanDone()));
        xochitlSettings.sync();
        xochitlSettings.beginGroup("wifinetworks");
        QList<QVariantMap> networks;
        for(const QString& childKey : xochitlSettings.allKeys()){
            QVariantMap network = xochitlSettings.value(childKey).toMap();
            networks.append(network);
        }
        xochitlSettings.endGroup();
        for(auto network : Networks()){
            QString ssid = network->Properties()["ssid"].toString();
            for(auto item : networks){
                if(item["ssid"].toString() == ssid.mid(1, ssid.length() - 2)){
                    networks.removeAll(item);
                }
            }
            network->setEnabled(true);
        }
        for(auto item : networks){
            QString ssid = item["ssid"].toString();
            QVariantMap args;
            args.insert("ssid", ssid);
            args.insert(item["protocol"].toString(), item["password"].toString());
            auto network = AddNetwork(args);
            qDebug() << "Registered network " + network->SSID();
            network->setEnabled(true);
        }
    };
    ~WifiManager() {};
    void waitForScan(){
        while(Scanning()){
            usleep(1000);
        }
    }
    bool isConnected(){
        return CurrentNetwork()->getPath().path() != "/";
    }
    void Scan(bool active = false){
        QMap<QString, QVariant> args;
        args["Type"] = active ? "active" : "passive";
        this->call("Scan", args);
    }
    void Disconnect(){ this->call("Disconnect"); }
    Network* AddNetwork(QVariantMap args){
         QDBusObjectPath path = (QDBusReply<QDBusObjectPath>)this->call("AddNetwork", args);
         return new Network(path, this->connection());
    }
    void RemoveAllNetworks(){ this->call("RemoveAllNetworks"); }
    void SelectNetwork(Network* network){
        this->call("SelectNetwork", QVariant::fromValue(network->getPath()));
    }
    void Reassociate(){ this->call("Reassociate"); }
    void Reattach(){ this->call("Reattach"); }
    void Reconnect(){ this->call("Reconnect"); }
    void AddBlob(QString name, QByteArray blob){ this->call("AddBlob", name, blob); }
    void RemoveBlob(QString name){ this->call("RemoveBlob", name); }
    QByteArray GetBlob(QString name){
        QByteArray blob = (QDBusReply<QByteArray>)this->call("GetBlob", name);
        return blob;
    }
    void AutoScan(QString arg){ this->call("AutoScan", arg); }
    void TDLSDiscover(QString peer_address){ this->call("TDLSDiscover", peer_address); }
    void TDLSSetup(QString peer_address){ this->call("TDLSSetup", peer_address); }
    void TDLSStatus(QString peer_address){ this->call("TDLSStatus", peer_address); }
    void TDLSTeardown(QString peer_address){ this->call("TDLSTeardown", peer_address); }
    void EAPLogoff(){ this->call("EAPLogoff"); }
    void EAPLogon(){ this->call("EAPLogon"); }
    void NetworkReply(Network* network, QString field, QString value){
        auto path = QDBusArgument() << network->getPath();
        this->call("NetworkReply", path.asVariant(), field, value);
    }
    void SetPKCS11EngineAndModulePath(QString pkcs11_engine_path, QString pkcs11_module_path){
        this->call("SetPKCS11EngineAndModulePath", pkcs11_engine_path, pkcs11_module_path);
    }
    void FlushBSS(uint age){ this->call("FlushBSS", age); }
    void SubscribeProbeReq(){ this->call("SubscribeProbeReq"); }
    void UnsubscribeProbeReq(){ this->call("UnsubscribeProbeReq"); }

    bool Scanning(){ return (QDBusReply<bool>)getProperty("Scanning"); }
    QString State(){ return getProperty<QString>("State"); }
    QString Country(){ return getProperty<QString>("Country"); }
    QString Ifname(){ return getProperty<QString>("Ifname"); }
    QString BridgeIfname(){ return getProperty<QString>("BridgeIfname"); }
    QString Driver(){ return getProperty<QString>("Driver"); }
    QString CurrentAuthMode(){ return getProperty<QString>("CurrentAuthMode"); }
    QString PKCS11EnginePath(){ return getProperty<QString>("PKCS11EnginePath"); }
    QString PKCS11ModulePath(){ return getProperty<QString>("PKCS11ModulePath"); }
    uint ApScan(){ return getProperty<uint>("ApScan"); }
    uint BSSExpireAge(){ return getProperty<uint>("BSSExpireAge"); }
    uint BSSExpireCount(){ return getProperty<uint>("BSSExpireCount"); }
    QStringList Blobs(){ return getProperty<QStringList>("Blobs"); }
    QMap<QString, QStringList> Capabilities(){
        auto capabilities = getMapProperty<QString, QVariant>("Capabilities");
        QMap<QString, QStringList> result;
        for(auto key : capabilities.keys()){
            result.insert(key, capabilities[key].toStringList());
        }
        return result;
    }
    bool FastReauth(){ return getProperty<bool>("FastReauth"); }
    int ScanInterval(){ return getProperty<int>("ScanInterval"); }
    int DisconnectReason(){ return getProperty<int>("DisconnectReason"); }
    BSS* CurrentBSS(){
        auto path = getProperty<QDBusObjectPath>("CurrentBSS");
        if(path.path() == "/"){
            return nullptr;
        }
        return new BSS(path, this->connection());
    }
    Network* CurrentNetwork(){
        auto path = getProperty<QDBusObjectPath>("CurrentNetwork");
        return new Network(path, this->connection());
    }
    QList<BSS*> BSSs(){
        QList<BSS*> result;
        for(auto path : getArrayProperty<QDBusObjectPath>("BSSs")){
            result.append(new BSS(path, this->connection()));
        }
        return result;
    }
    QList<Network*> Networks(){
        QList<Network*> result;
        for(auto path : getArrayProperty<QDBusObjectPath>("Networks")){
            result.append(new Network(path, this->connection()));
        }
        return result;
    }
    QStringList SSIDs(){
        QStringList result;
        for(BSS* bss : BSSs()){
            auto ssid = bss->SSID();
            if(!ssid.isEmpty()){
                result << ssid;
            }
        }
        return result;
    }


private:
    QSettings xochitlSettings;
};

#endif // WIFIMANAGER_H
