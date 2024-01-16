#include "xochitlsettings.h"

using namespace Oxide;

namespace Oxide{
    WifiNetworks XochitlSettings::wifinetworks(){
        beginGroup("wifinetworks");
        QMap<QString, QVariantMap> wifinetworks;
        for(const QString& key : allKeys()){
            QVariantMap network = value(key).toMap();
            wifinetworks[key] = network;
        }
        endGroup();
        return wifinetworks;
    }
    void XochitlSettings::setWifinetworks(const WifiNetworks& wifinetworks){
        beginGroup("wifinetworks");
        for(const QString& key : wifinetworks.keys()){
            setValue(key, wifinetworks.value(key));
        }
        endGroup();
        sync();
    }
    QVariantMap XochitlSettings::getWifiNetwork(const QString& name){
        beginGroup("wifinetworks");
        QVariantMap network = value(name).toMap();
        endGroup();
        return network;
    }
    void XochitlSettings::setWifiNetwork(const QString& name, QVariantMap properties){
        beginGroup("wifinetworks");
        setValue(name, properties);
        endGroup();
        sync();
    }
    void XochitlSettings::resetWifinetworks(){}
    XochitlSettings::~XochitlSettings(){}
    O_SETTINGS_PROPERTY_BODY(XochitlSettings, QString, General, passcode)
    O_SETTINGS_PROPERTY_BODY(XochitlSettings, bool, General, wifion)
}
#include "moc_xochitlsettings.cpp"
