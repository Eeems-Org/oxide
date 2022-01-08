#ifndef WLAN_H
#define WLAN_H

#include <QFileInfo>

#include <liboxide.h>

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
        auto out = exec("grep " + iface() + " /proc/net/wireless | awk '{print $3}'");
        if(QString(out.c_str()).isEmpty()){
            return 0;
        }
        try {
            return std::stoi(out);
        }
        catch (const std::invalid_argument& e) {
            qDebug() << "link failed: " << out.c_str();
            return 0;
        }
    }
    signed int rssi(){
        QDBusMessage message = m_interface->call("SignalPoll");
        if (message.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "SignalPoll error: " << message.errorMessage();
            return -100;
        }
        auto props = qdbus_cast<QVariantMap>(message.arguments().at(0).value<QDBusVariant>().variant().value<QDBusArgument>());
        auto result = props["rssi"].toInt();
        if(result >= 0){
            return -100;
        }
        return result;
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
