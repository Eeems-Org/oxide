#ifndef WLAN_H
#define WLAN_H

#include <QFileInfo>

#include <liboxide.h>

#include "supplicant.h"

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/sysobject.h"

using Oxide::SysObject;

class Wlan : public QObject, public SysObject {
    Q_OBJECT

public:
    Wlan(QString path, QObject* parent);
    void setInterface(QString path);
    void removeInterface();
    QString iface();
    bool up();
    bool down();
    bool isUp();
    Interface* interface();
    QSet<QString> blobs();
    QString operstate();
    bool pingIP(std::string ip, const char* port);
    bool isConnected();
    int link();
    signed int rssi();
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
