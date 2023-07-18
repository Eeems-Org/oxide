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
    Wlan(QString path, QObject* parent) : QObject(parent), SysObject(path), m_blobs(), m_iface(){
        m_iface = QFileInfo(path).fileName();
        m_interface = nullptr;
    }
    void setInterface(QString path);
    void removeInterface(){
        if(m_interface != nullptr){
            m_interface->deleteLater();
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
    bool pingIP(std::string ip, const char* port) {
        auto process = new QProcess();
        process->setProgram("bash");
        std::string cmd("{ echo -n > /dev/tcp/" + ip.substr(0, ip.length() - 1) + "/" + port + "; } > /dev/null 2>&1");
        process->setArguments(QStringList() << "-c" << cmd.c_str());
        process->start();
        if(!process->waitForFinished(100)){
            process->kill();
            return false;
        }
        return !process->exitCode();
    }
    bool isConnected(){
        auto ip = exec("ip r | grep " + iface() + " | grep default | awk '{print $3}'");
        return ip != "" && (pingIP(ip, "53") || pingIP(ip, "80"));
    }
    int link(){
        QDBusPendingReply<QVariant> res = m_interface->SignalPoll();
        res.waitForFinished();
        if(!res.isError()){
            auto props = qdbus_cast<QVariantMap>(res.value());
            auto result = props["linkspeed"].toInt();
            if(result < 0){
                return 0;
            }
            return result;
        }
        O_WARNING("SignalPoll error: " << res.error());
        auto out = exec("grep " + iface() + " /proc/net/wireless | awk '{print $3}'");
        if(QString(out.c_str()).isEmpty()){
            return 0;
        }
        try {
            return std::stoi(out);
        }
        catch (const std::invalid_argument& e) {
            O_WARNING("link failed: " << out.c_str());
            return 0;
        }
        return -100;
    }
    signed int rssi(){
        QDBusPendingReply<QVariant> res = m_interface->SignalPoll();
        res.waitForFinished();
        if(!res.isError()){
            auto props = qdbus_cast<QVariantMap>(res.value());
            auto result = props["rssi"].toInt();
            if(result >= 0){
                return -100;
            }
            return result;
        }
        O_WARNING("SignalPoll error: " << res.error());
        auto out = exec("grep " + iface() + " /proc/net/wireless | awk '{print $4}'");
        if(QString(out.c_str()).isEmpty()){
            return 0;
        }
        try {
            return std::stoi(out);
        }
        catch (const std::invalid_argument& e) {
            O_WARNING("signal failed: " << out.c_str());
            return 0;
        }
        return -100;
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
