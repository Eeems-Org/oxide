#ifndef NETWORK_H
#define NETWORK_H

#include <QDBusConnection>
#include <QMutableListIterator>

#include "supplicant.h"
#include "dbussettings.h"

class Network : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_NETWORK_INTERFACE)
    Q_PROPERTY(bool enabled READ enabled  WRITE setEnabled  NOTIFY stateChanged)
    Q_PROPERTY(QString ssid READ ssid)
    Q_PROPERTY(QString password READ password WRITE setPassword)
    Q_PROPERTY(QString protocol READ protocol WRITE setProtocol)
    Q_PROPERTY(QVariantMap properties READ properties WRITE setProperties NOTIFY propertiesChanged)
public:
    Network(QString path, QString ssid, QVariantMap properties, QObject* parent);
    Network(QString path, QVariantMap properties, QObject* parent)
    : Network(path, properties["ssid"].toString().mid(1, properties["ssid"].toString().length() - 2), properties, parent){}

    ~Network(){ unregisterPath(); }
    QString path() { return m_path; }
    void registerPath(){
        auto bus = QDBusConnection::systemBus();
        bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
        if(bus.registerObject(path(), this, QDBusConnection::ExportAllContents)){
            qDebug() << "Registered" << path() << OXIDE_NETWORK_INTERFACE;
        }else{
            qDebug() << "Failed to register" << path();
        }
    }
    void unregisterPath(){
        auto bus = QDBusConnection::systemBus();
        if(bus.objectRegisteredAt(path()) != nullptr){
            qDebug() << "Unregistered" << path();
            bus.unregisterObject(path());
        }
    }

    bool enabled(){ return m_enabled; }
    void setEnabled(bool enabled){
        m_enabled = enabled;
        for(auto network : networks){
            network->setEnabled(true);
        }
        emit stateChanged(enabled);
    }

    QString ssid(){ return m_ssid; }

    QString password() {
        if(passwordField() == ""){
            return "";
        }
        return m_properties[passwordField()].toString();
    }
    void setPassword(QString password) {
        if(passwordField() != ""){
            m_properties[passwordField()] = password;
            emit propertiesChanged(m_properties);
        }
    }

    QString protocol() { return m_protocol; }
    void setProtocol(QString protocol) {
        auto oldPassword = password();
        m_properties.remove(passwordField());
        m_protocol = protocol;
        if(passwordField() != ""){
            m_properties[passwordField()] = oldPassword;
        }
        if(protocol == "open"){
            m_properties["key_mgmt"] = "NONE";
        }else{
            m_properties.remove("key_mgmt");
        }
        setProperties(m_properties);
    }

    QVariantMap properties() { return m_properties; }
    void setProperties(QVariantMap properties){
        m_properties = properties;
        auto props = realProps();
        for(auto network : networks){
            network->setProperties(props);
        }
        emit propertiesChanged(properties);
    }
    QList<QString> paths(){
        QList<QString> result;
        for(auto network : networks){
            result.append(network->path());
        }
        return result;
    }
    void addNetwork(QString path, Interface* interface){
        if(paths().contains(path)){
            return;
        }
        auto network = new INetwork(WPA_SUPPLICANT_SERVICE, path, QDBusConnection::systemBus(), interface);
        networks.append(network);
        network->setEnabled(m_enabled);
        QObject::connect(network, &INetwork::PropertiesChanged, this, &Network::PropertiesChanged, Qt::QueuedConnection);
    }
    void addNetwork(INetwork* network){
        if(paths().contains(network->path())){
            return;
        }
        networks.append(network);
        network->setEnabled(m_enabled);
        QObject::connect(network, &INetwork::PropertiesChanged, this, &Network::PropertiesChanged, Qt::QueuedConnection);
    }
    void removeNetwork(QString path){
        QMutableListIterator<INetwork*> i(networks);
        while(i.hasNext()){
            auto network = i.next();
            if(network->path() == path){
                i.remove();
            }
        }
    }
    void registerNetwork();
    Q_INVOKABLE void connect(){
        QList<QDBusPendingReply<void>> replies;
        for(auto network : networks){
            replies.append(((Interface*)network->parent())->SelectNetwork(QDBusObjectPath(network->path())));
        }
        for(auto reply : replies){
            reply.waitForFinished();
        }
    }

Q_SIGNALS:
    void stateChanged(bool);
    void propertiesChanged(QVariantMap);
    void connected();
    void disconnected();


private slots:
    void PropertiesChanged(const QVariantMap& properties){
        if(properties.contains("Enabled")){
            bool enabled = false;
            for(auto network : networks){
                if(network->enabled()){
                    enabled = true;
                    break;
                }
            }
            if(enabled != m_enabled){
                setEnabled(enabled);
            }
        }
    }

private:
    QString m_path;
    QList<INetwork*> networks;
    QVariantMap m_properties;
    QString m_ssid;
    QString m_protocol;
    bool m_enabled = false;

    QVariantMap realProps(){
        QVariantMap props(m_properties);
        props["ssid"] = m_ssid;
        return props;
    }
    QString passwordField(){
        if(protocol() == "psk"){
            return "psk";
        }else if(protocol() == "eap"){
            return "password";
        }else if(protocol() == "sae"){
            return "sae_password";
        }
        return "";
    }
};

#endif // NETWORK_H
