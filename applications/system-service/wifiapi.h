#ifndef WIFIAPI_H
#define WIFIAPI_H

#include <QObject>

#include "apibase.h"
#include "wlan.h"
#include "network.h"
#include "bss.h"

#define wifiAPI WifiAPI::singleton()

class WifiAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_WIFI_INTERFACE)
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(QStringList blobs READ blobs)
    Q_PROPERTY(QList<QDBusObjectPath> bSSs READ bSSs)
    Q_PROPERTY(int link READ link NOTIFY linkChanged)
    Q_PROPERTY(int rssi READ rssi NOTIFY rssiChanged)
    Q_PROPERTY(QDBusObjectPath network READ network)
    Q_PROPERTY(QList<QDBusObjectPath> networks READ getNetworkPaths)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)

public:
    static WifiAPI* singleton(WifiAPI* self = nullptr);
    WifiAPI(QObject* parent);
    ~WifiAPI();
    void setEnabled(bool enabled);
    bool isEnabled(){ return m_enabled; }
    QList<Wlan*> getWlans(){ return wlans; }
    QList<Interface*> getInterfaces(){ return interfaces(); }
    enum State { Unknown, Off, Disconnected, Offline, Online};
    Q_ENUM(State)

    int state();
    void setState(int state);

    Q_INVOKABLE bool enable();
    Q_INVOKABLE void disable();
    Q_INVOKABLE QDBusObjectPath addNetwork(QString ssid, QVariantMap properties);
    Q_INVOKABLE QList<QDBusObjectPath> getNetwork(QVariantMap properties);
    Q_INVOKABLE QList<QDBusObjectPath> getBSS(QVariantMap properties);
    Q_INVOKABLE void scan(bool active = false);
    Q_INVOKABLE void reconnect();
    Q_INVOKABLE void reassosiate();
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void flushBSSCache(uint age);
    Q_INVOKABLE void addBlob(QString name, QByteArray blob);
    Q_INVOKABLE void removeBlob(QString name);
    Q_INVOKABLE QByteArray getBlob(QString name, QByteArray blob);
    QStringList blobs();
    QList<QDBusObjectPath> bSSs();
    int link();
    int rssi();
    QDBusObjectPath network();
    QList<QDBusObjectPath> getNetworkPaths();
    bool scanning();

    // Interface signals
    void BSSAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties);
    void BSSRemoved(Wlan* wlan, const QDBusObjectPath& path);
    void BlobAdded(Wlan* wlan, const QString& name);
    void BlobRemoved(Wlan* wlan, const QString& name);
    void NetworkAdded(Wlan* wlan, const QDBusObjectPath& path, const QVariantMap& properties);
    void NetworkRemoved(Wlan* wlan, const QDBusObjectPath& path);
    void NetworkSelected(Wlan* wlan, const QDBusObjectPath& path);
    void InterfacePropertiesChanged(Wlan* wlan, const QVariantMap& properties);
    void ScanDone(Wlan* wlan, bool success);
    // BSS signals
    void BSSPropertiesChanged(const QVariantMap& properties);
    void stopUpdating(){ timer->stop(); }
    void resumeUpdating(){ timer->start(); }

signals:
    void stateChanged(int);
    void linkChanged(int);
    void rssiChanged(int);
    void networkAdded(QDBusObjectPath);
    void networkRemoved(QDBusObjectPath);
    void networkConnected(QDBusObjectPath);
    void disconnected();
    void bssFound(QDBusObjectPath);
    void bssRemoved(QDBusObjectPath);
    void scanningChanged(bool);

private slots:
    // wpa_supplicant signals
    void InterfaceAdded(const QDBusObjectPath &path, const QVariantMap &properties);
    void InterfaceRemoved(const QDBusObjectPath &path);
    void PropertiesChanged(const QVariantMap &properties);

private:
    bool m_enabled;
    QTimer* timer;
    QList<Wlan*> wlans;
    QList<Network*> networks;
    int m_state;
    QDBusObjectPath m_currentNetwork;
    int m_link;
    int m_rssi;
    QList<BSS*> bsss;
    bool m_scanning;
    Wpa_Supplicant* supplicant;

    QList<Interface*> interfaces();

    void validateSupplicant();

    void loadNetworks();

    void update();
    State getCurrentState();
    QString getPath(QString type, QString id);
};

#endif // WIFIAPI_H
