#include "bss.h"
#include "wifiapi.h"

BSS::BSS(QString path, QString bssid, QString ssid, QObject* parent)
: QObject(parent),
  m_path(path),
  bsss(),
  m_bssid(bssid),
  m_ssid(ssid) {
    for(auto bss : bsss){
        QObject::connect(bss, &IBSS::PropertiesChanged, this, &BSS::PropertiesChanged, Qt::QueuedConnection);
    }
}

BSS::~BSS(){ unregisterPath(); }

QString BSS::path(){ return m_path; }

void BSS::registerPath(){
    auto bus = QDBusConnection::systemBus();
    bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
    if(bus.registerObject(path(), this, QDBusConnection::ExportAllContents)){
        qDebug() << "Registered" << path() << OXIDE_BSS_INTERFACE;
    }else{
        qDebug() << "Failed to register" << path();
    }
}

void BSS::unregisterPath(){
    auto bus = QDBusConnection::systemBus();
    if(bus.objectRegisteredAt(path()) != nullptr){
        qDebug() << "Unregistered" << path();
        bus.unregisterObject(path());
    }
}

QString BSS::bssid(){
    if(!hasPermission("wifi")){
        return "";
    }
    return m_bssid; }

QString BSS::ssid(){
    if(!hasPermission("wifi")){
        return "";
    }
    return m_ssid;
}

QList<QString> BSS::paths(){
    QList<QString> result;
    if(!hasPermission("wifi")){
        return result;
    }
    for(auto bss : bsss){
        result.append(bss->path());
    }
    return result;
}

void BSS::addBSS(const QString& path){
    if(paths().contains(path)){
        return;
    }
    auto bss = new IBSS(WPA_SUPPLICANT_SERVICE, path, QDBusConnection::systemBus());
    bsss.append(bss);
    QObject::connect(bss, &IBSS::PropertiesChanged, this, &BSS::PropertiesChanged, Qt::QueuedConnection);
}

void BSS::removeBSS(const QString& path){
    QMutableListIterator<IBSS*> i(bsss);
    while(i.hasNext()){
        auto bss = i.next();
        if(!bss->isValid() || bss->path() == path){
            i.remove();
            bss->deleteLater();
        }
    }
}

bool BSS::privacy(){
    if(!hasPermission("wifi")){
        return false;
    }
    for(auto bss : bsss){
        if(bss->privacy()){
            return true;
        }
    }
    return false;
}

ushort BSS::frequency(){
    if(!hasPermission("wifi")){
        return 0;
    }
    if(!bsss.size()){
        return 0;
    }
    return bsss.first()->frequency();
}

short BSS::signal(){
    if(!hasPermission("wifi")){
        return 0;
    }
    if(!bsss.size()){
        return 0;
    }
    int signal = 0;
    for(auto bss : bsss){
        auto s = bss->signal();
        if(s > signal){
            signal = s;
        }
    }
    return signal;
}

QDBusObjectPath BSS::connect(){
    if(!hasPermission("wifi")){
        return QDBusObjectPath("/");
    }
    auto path = network();
    if(path.path() != "/"){
        return path;
    }
    return wifiAPI->addNetwork(ssid(), QVariantMap());
}

void BSS::PropertiesChanged(const QVariantMap& properties){
    emit propertiesChanged(properties);
}
QDBusObjectPath BSS::network(){
    if(!hasPermission("wifi")){
        return QDBusObjectPath("/");
    }
    QVariantMap args;
    args.insert("ssid", ssid());
    auto networks = wifiAPI->getNetwork(args);
    if(!networks.size()){
        return QDBusObjectPath("/");
    }
    return networks.first();
}

QStringList BSS::key_mgmt(){
    QStringList result;
    if(!hasPermission("wifi")){
        return result;
    }
    if(!bsss.size()){
        return result;
    }
    auto bss = bsss.first();
    result.append(bss->wPA()["KeyMgmt"].value<QStringList>());
    result.append(bss->rSN()["KeyMgmt"].value<QStringList>());
    return result;
}

bool BSS::hasPermission(QString permission, const char* sender){ return wifiAPI->hasPermission(permission,sender); }

#include "moc_bss.cpp"
