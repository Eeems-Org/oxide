#include "wlan.h"
#include "wifiapi.h"

void Wlan::BSSAdded(const QDBusObjectPath &path, const QVariantMap &properties){
    m_bsss.append(path);
    ((WifiAPI*)parent())->BSSAdded(this, path, properties);
}
void Wlan::BSSRemoved(const QDBusObjectPath &path){
    m_bsss.removeAll(path);
    ((WifiAPI*)parent())->BSSRemoved(this, path);
}
void Wlan::BlobAdded(const QString &name){
    if(!m_blobs.contains(name)){
        m_blobs.insert(name);
    }
    ((WifiAPI*)parent())->BlobAdded(this, name);
}
void Wlan::BlobRemoved(const QString &name){
    m_blobs.remove(name);
    ((WifiAPI*)parent())->BlobRemoved(this, name);
}
void Wlan::Certification(const QVariantMap &certification){
    ((WifiAPI*)parent())->Certification(this, certification);
}
void Wlan::EAP(const QString &status, const QString &parameter){
    ((WifiAPI*)parent())->EAP(this, status, parameter);
}
void Wlan::NetworkAdded(const QDBusObjectPath &path, const QVariantMap &properties){
    ((WifiAPI*)parent())->NetworkAdded(this, path, properties); }
void Wlan::NetworkRemoved(const QDBusObjectPath &path){
    ((WifiAPI*)parent())->NetworkRemoved(this, path);
}
void Wlan::NetworkSelected(const QDBusObjectPath &path){
    ((WifiAPI*)parent())->NetworkSelected(this, path);
}
void Wlan::ProbeRequest(const QVariantMap &args){
    ((WifiAPI*)parent())->ProbeRequest(this, args);
}
void Wlan::PropertiesChanged(const QVariantMap &properties){
    ((WifiAPI*)parent())->InterfacePropertiesChanged(this, properties);
}
void Wlan::ScanDone(bool success){
    ((WifiAPI*)parent())->ScanDone(this, success);
}
void Wlan::StaAuthorized(const QString &name){
    ((WifiAPI*)parent())->StaAuthorized(this, name);
}
void Wlan::StaDeauthorized(const QString &name){
    ((WifiAPI*)parent())->StaDeauthorized(this, name);
}
