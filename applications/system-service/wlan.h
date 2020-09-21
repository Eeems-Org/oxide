#ifndef WLAN_H
#define WLAN_H

#include <QFileInfo>

#include "dbussettings.h"
#include "sysobject.h"
#include "supplicant.h"

class Wlan : public QObject, public SysObject {
    Q_OBJECT
public:
    Wlan(QString path, QObject* parent) : QObject(parent), SysObject(path), m_blobs(), m_iface(){
        m_iface = QFileInfo(path).fileName();
        m_interface = nullptr;
    }
    void setInterface(QString path);
    void removeInterface(){
        if(m_interface != nullptr){
            delete m_interface;
            m_interface = nullptr;
        }
    }
    QString iface() { return m_iface; }
    bool up() { return !system(("ifconfig " + iface() + " up").toStdString().c_str()); }
    bool down() { return !system(("ifconfig " + iface() + " down").toStdString().c_str()); }
    bool isUp(){ return !system(("ip addr show " + iface() + " | grep UP > /dev/null").toStdString().c_str()); }
    Interface* interface() { return m_interface; }
    QSet<QString> blobs(){ return m_blobs; }
    QString operstate(){
        if(hasProperty("operstate")){
            return QString(strProperty("operstate").c_str());
        }
        return "";
    }
    bool isConnected(){
        auto ip = exec("ip r | grep " + iface() + " | grep default | awk '{print $3}'");
        return ip != "" && !system(("echo -n > /dev/tcp/" + ip.substr(0, ip.length() - 1) + "/53").c_str());
    }
    int link(){
        return std::stoi(exec("cat /proc/net/wireless | grep " + iface() + " | awk '{print $3}'"));
    }
signals:
    void BSSAdded(Wlan*, QDBusObjectPath, QVariantMap);
    void BSSRemoved(Wlan*, QDBusObjectPath);
    void BlobAdded(Wlan*, QString);
    void BlobRemoved(Wlan*, QString);
    void NetworkAdded(Wlan*, QDBusObjectPath, QVariantMap);
    void NetworkRemoved(Wlan*, QDBusObjectPath);
    void NetworkSelected(Wlan*, QDBusObjectPath);
    void PropertiesChanged(Wlan*, QVariantMap);
    void ScanDone(Wlan*, bool);
private slots:
    void onBSSAdded(const QDBusObjectPath& path, const QVariantMap& properties);
    void onBSSRemoved(const QDBusObjectPath& path);
    void onBlobAdded(const QString& name);
    void onBlobRemoved(const QString& name);
    void onNetworkAdded(const QDBusObjectPath& path, const QVariantMap& properties);
    void onNetworkRemoved(const QDBusObjectPath& path);
    void onNetworkSelected(const QDBusObjectPath& path);
    void onPropertiesChanged(const QVariantMap& properties);
    void onScanDone(bool success);

private:
    Interface* m_interface;
    QSet<QString> m_blobs;
    QString m_iface;

    std::string exec(QString cmd);
};

#endif // WLAN_H
