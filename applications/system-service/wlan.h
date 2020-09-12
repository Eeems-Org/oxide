#ifndef WLAN_H
#define WLAN_H

#include "dbussettings.h"
#include "sysobject.h"
#include "wpa_supplicant.h"

typedef FiW1Wpa_supplicant1Interface Wpa_Supplicant;
typedef FiW1Wpa_supplicant1BSSInterface BSS;
typedef FiW1Wpa_supplicant1InterfaceInterface Interface;

class Wlan : public QObject, public SysObject {
    Q_OBJECT
public:
    Wlan(QString path, QObject* parent) : QObject(parent), SysObject(path), m_blobs(), m_bsss(){
        m_interface = nullptr;
    }
    void setInterface(QString path){
        if(m_interface != nullptr && m_interface->path() == path){
            return;
        }
        removeInterface();
        m_interface = new Interface(WPA_SUPPLICANT_SERVICE, path, QDBusConnection::systemBus(), this);
        m_blobs = m_interface->blobs().toSet();
        m_bsss = m_interface->bSSs();
        connect(m_interface, &Interface::BSSAdded, this, &Wlan::BSSAdded);
        connect(m_interface, &Interface::BSSRemoved, this, &Wlan::BSSRemoved);
        connect(m_interface, &Interface::BlobAdded, this, &Wlan::BlobAdded);
        connect(m_interface, &Interface::BlobRemoved, this, &Wlan::BlobRemoved);
        connect(m_interface, &Interface::Certification, this, &Wlan::Certification);
        connect(m_interface, &Interface::EAP, this, &Wlan::EAP);
        connect(m_interface, &Interface::NetworkAdded, this, &Wlan::NetworkAdded);
        connect(m_interface, &Interface::NetworkRemoved, this, &Wlan::NetworkRemoved);
        connect(m_interface, &Interface::NetworkSelected, this, &Wlan::NetworkSelected);
        connect(m_interface, &Interface::PropertiesChanged, this, &Wlan::PropertiesChanged);
        connect(m_interface, &Interface::ScanDone, this, &Wlan::ScanDone);
        connect(m_interface, &Interface::StaAuthorized, this, &Wlan::StaAuthorized);
        connect(m_interface, &Interface::StaDeauthorized, this, &Wlan::StaDeauthorized);
    }
    void removeInterface(){
        if(m_interface != nullptr){
            delete m_interface;
            m_interface = nullptr;
        }
    }
    Interface* interface() { return m_interface; }
    QSet<QString> blobs(){ return m_blobs; }
    QList<QDBusObjectPath> BSSs() { return m_bsss; }
private slots:
    void BSSAdded(const QDBusObjectPath &path, const QVariantMap &properties);
    void BSSRemoved(const QDBusObjectPath &path);
    void BlobAdded(const QString &name);
    void BlobRemoved(const QString &name);
    void Certification(const QVariantMap &certification);
    void EAP(const QString &status, const QString &parameter);
    void NetworkAdded(const QDBusObjectPath &path, const QVariantMap &properties);
    void NetworkRemoved(const QDBusObjectPath &path);
    void NetworkSelected(const QDBusObjectPath &path);
    void ProbeRequest(const QVariantMap &args);
    void PropertiesChanged(const QVariantMap &properties);
    void ScanDone(bool success);
    void StaAuthorized(const QString &name);
    void StaDeauthorized(const QString &name);
private:
    Interface* m_interface;
    QSet<QString> m_blobs;
    QList<QDBusObjectPath> m_bsss;
};

#endif // WLAN_H
