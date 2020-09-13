#include "bss.h"
#include "wifiapi.h"

BSS::BSS(QString path, QString bssid, QString ssid, QObject* parent)
: QObject(parent),
  m_path(path),
  bsss(),
  m_bssid(bssid),
  m_ssid(ssid) {
    auto api = (WifiAPI*)parent;
    for(auto bss : bsss){
        connect(bss, &IBSS::PropertiesChanged, this, &BSS::PropertiesChanged, Qt::QueuedConnection);
    }
    QDBusConnection::systemBus().registerObject(path, this);
}