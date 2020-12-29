#include "bss.h"
#include "wifiapi.h"

BSS::BSS(QString path, QString bssid, QString ssid, QObject* parent)
: QObject(parent),
  m_path(path),
  bsss(),
  m_bssid(bssid),
  m_ssid(ssid),
  mutex() {
    for(auto bss : bsss){
        if(bss != nullptr){
            QObject::connect(bss, &IBSS::PropertiesChanged, this, &BSS::PropertiesChanged, Qt::QueuedConnection);
        }
    }
}

QDBusObjectPath BSS::connect(){
    auto path = network();
    if(path.path() != "/"){
        return path;
    }
    return wifiAPI->addNetwork(ssid(), QVariantMap());
}
QDBusObjectPath BSS::network(){
    QVariantMap args;
    args.insert("ssid", ssid());
    auto networks = wifiAPI->getNetwork(args);
    if(!networks.size()){
        return QDBusObjectPath("/");
    }
    return networks.first();
}
