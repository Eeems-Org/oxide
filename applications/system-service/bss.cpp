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

bool BSS::hasPermission(QString permission, const char* sender){ return wifiAPI->hasPermission(permission,sender); }
