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
    auto path = network();
    if(path.path() != "/"){
        return path;
    }
    auto api = reinterpret_cast<WifiAPI*>(parent());
    return api->addNetwork(ssid(), QVariantMap());
}
QDBusObjectPath BSS::network(){
    auto api = reinterpret_cast<WifiAPI*>(parent());
    QVariantMap args;
    args.insert("ssid", ssid());
    auto networks = api->getNetwork(args);
    if(!networks.size()){
        return QDBusObjectPath("/");
    }
    return networks.first();
}
