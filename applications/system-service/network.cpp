#include <QSet>

#include "network.h"
#include "wifiapi.h"
#include <liboxide/debug.h>

QSet<QString> none{
    "NONE",
    "WPA-NONE"
};
QSet<QString> psk{
    "WPA-PSK",
    "FT-PSK",
    "WPA-PSK-SHA256"
};
QSet<QString> eap{
    "WPA-EAP",
    "FT-EAP",
    "FT-EAP-SHA384",
    "WPA-EAP-SHA256",
    "WPA-EAP-SUITE-B",
    "WPA-EAP-SUITE-B-192"
};
QSet<QString> sae{
    "SAE"
};

Network::Network(QString path, QString ssid, QVariantMap properties, QObject* parent)
: QObject(parent), m_path(path), networks(), m_properties(properties), m_ssid(ssid),
  m_enabled(false){
    auto proto = properties["key_mgmt"].toString();
    if(psk.contains(proto)){
        m_protocol = "psk";
    }else if(eap.contains(proto)){
        m_protocol = "eap";
    }else if(none.contains(proto)){
        m_protocol = "open";
    }else if(sae.contains(proto)){
        m_protocol = "sae";
    }else{
        m_protocol = "psk";
    }
}

Network::Network(QString path, QVariantMap properties, QObject* parent)
    : Network(path, properties["ssid"].toString().mid(1, properties["ssid"].toString().length() - 2), properties, parent){}

Network::~Network(){ unregisterPath(); }

QString Network::path() { return m_path; }

void Network::registerPath(){
    auto bus = QDBusConnection::systemBus();
    bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
    if(bus.registerObject(path(), this, QDBusConnection::ExportAllContents)){
        O_INFO("Registered" << path() << OXIDE_NETWORK_INTERFACE);
    }else{
        O_WARNING("Failed to register" << path());
    }
}

void Network::unregisterPath(){
    auto bus = QDBusConnection::systemBus();
    if(bus.objectRegisteredAt(path()) != nullptr){
        O_DEBUG("Unregistered" << path());
        bus.unregisterObject(path());
    }
}

bool Network::enabled(){ return m_enabled; }

void Network::setEnabled(bool enabled){
    m_enabled = enabled;
    for(auto network : networks){
        if(network->enabled() != enabled){
            network->setEnabled(enabled);
        }
    }
    emit stateChanged(enabled);
}

QString Network::ssid(){
    if(!hasPermission("wifi")){
        return "";
    }
    return m_ssid;
}
void Network::registerNetwork(){
    for(auto interface : wifiAPI->getInterfaces()){
        bool found = false;
        for(auto network : networks){
            if(interface->path() == (reinterpret_cast<Interface*>(network->parent()))->path()){
                found = true;
                break;
            }
        }
        if(!found){
            O_DEBUG(realProps());
            QDBusObjectPath path = interface->AddNetwork(realProps());
            auto network = new INetwork(WPA_SUPPLICANT_SERVICE, path.path(), QDBusConnection::systemBus(), interface);
            networks.append(network);
            network->setEnabled(m_enabled);
        }
    }
}

void Network::connect(){
    if(!hasPermission("wifi")){
        return;
    }
    QList<QDBusPendingReply<void>> replies;
    for(auto network : networks){
        auto interface = (Interface*)network->parent();
        replies.append(interface->SelectNetwork(QDBusObjectPath(network->path())));
    }
    for(auto reply : replies){
        reply.waitForFinished();
    }
}

void Network::remove(){
    if(!hasPermission("wifi")){
        return;
    }
    QList<QDBusPendingReply<void>> replies;
    QMap<QString,Interface*> todo;
    for(auto network : networks){
        todo.insert(network->path(), (Interface*)network->parent());
    }
    for(auto path : todo.keys()){
        auto interface = todo[path];
        replies.append(interface->RemoveNetwork(QDBusObjectPath(path)));
    }
    for(auto reply : replies){
        reply.waitForFinished();
    }
}

void Network::PropertiesChanged(const QVariantMap& properties){
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

QList<QDBusObjectPath> Network::bSSs(){
    if(!hasPermission("wifi")){
        return QList<QDBusObjectPath>();
    }
    QVariantMap args;
    args.insert("ssid", ssid());
    return wifiAPI->getBSS(args);
}

QString Network::getNull(){ return ""; }

QString Network::password() {
    if(!hasPermission("wifi")){
        return "";
    }
    if(passwordField() == ""){
        return "";
    }
    auto password = m_properties[passwordField()].toString();
    if(password != ""){
        return password;
    }
    for(auto network : networks){
        password = network->properties()[passwordField()].toString();
        if(password != ""){
            return password;
        }
    }
    return "";
}

void Network::setPassword(QString password) {
    if(!hasPermission("wifi")){
        return;
    }
    if(passwordField() != ""){
        m_properties[passwordField()] = password;
        emit propertiesChanged(m_properties);
    }
}

QString Network::protocol() {
    if(!hasPermission("wifi")){
        return "";
    }
    return m_protocol;
}

void Network::setProtocol(QString protocol) {
    if(!hasPermission("wifi")){
        return;
    }
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

QVariantMap Network::properties() {
    if(!hasPermission("wifi")){
        return QVariantMap();
    }
    return m_properties;
}

void Network::setProperties(QVariantMap properties){
    if(!hasPermission("wifi")){
        return;
    }
    m_properties = properties;
    auto props = realProps();
    for(auto network : networks){
        network->setProperties(props);
    }
    emit propertiesChanged(properties);
}

QList<QString> Network::paths(){
    QList<QString> result;
    if(!hasPermission("wifi")){
        return result;
    }
    for(auto network : networks){
        result.append(network->path());
    }

    return result;
}

void Network::addNetwork(const QString& path, Interface* interface){
    if(paths().contains(path)){
        return;
    }
    auto network = new INetwork(WPA_SUPPLICANT_SERVICE, path, QDBusConnection::systemBus(), interface);
    networks.append(network);
    if(network->enabled() != m_enabled){
        network->setEnabled(m_enabled);
    }
    QObject::connect(network, &INetwork::PropertiesChanged, this, &Network::PropertiesChanged, Qt::QueuedConnection);
}

void Network::removeNetwork(const QString& path){
    QMutableListIterator<INetwork*> i(networks);
    while(i.hasNext()){
        auto network = i.next();
        if(!network->isValid() || network->path() == path){
            i.remove();
            network->deleteLater();
        }
    }
}

void Network::removeInterface(Interface* interface){
    QMutableListIterator<INetwork*> i(networks);
    while(i.hasNext()){
        auto network = i.next();
        if(!network->isValid() || (Interface*)network->parent() == interface){
            i.remove();
            network->deleteLater();
        }
    }
}

bool Network::hasPermission(QString permission, const char* sender){ return wifiAPI->hasPermission(permission, sender); }

QVariantMap Network::realProps(){
    QVariantMap props(m_properties);
    props["ssid"] = m_ssid;
    return props;
}

QString Network::passwordField(){
    if(protocol() == "psk"){
        return "psk";
    }else if(protocol() == "eap"){
        return "password";
    }else if(protocol() == "sae"){
        return "sae_password";
    }
    return "";
}

#include "moc_network.cpp"
