#include "wlan.h"
#include "bss.h"
#include "wifiapi.h"

void Wlan::setInterface(QString path){
    if(m_interface != nullptr && m_interface->path() == path){
        return;
    }
    removeInterface();
    auto bus = QDBusConnection::systemBus();
    m_interface = new Interface(WPA_SUPPLICANT_SERVICE, path, bus, this);
    auto blobs =m_interface->blobs();
    m_blobs = QSet<QString>(blobs.begin(), blobs.end());
    connect(m_interface, &Interface::BSSAdded, this, &Wlan::onBSSAdded, Qt::QueuedConnection);
    connect(m_interface, &Interface::BSSRemoved, this, &Wlan::onBSSRemoved, Qt::QueuedConnection);
    connect(m_interface, &Interface::BlobAdded, this, &Wlan::onBlobAdded, Qt::QueuedConnection);
    connect(m_interface, &Interface::BlobRemoved, this, &Wlan::onBlobRemoved, Qt::QueuedConnection);
    connect(m_interface, &Interface::NetworkAdded, this, &Wlan::onNetworkAdded, Qt::QueuedConnection);
    connect(m_interface, &Interface::NetworkRemoved, this, &Wlan::onNetworkRemoved, Qt::QueuedConnection);
    connect(m_interface, &Interface::NetworkSelected, this, &Wlan::onNetworkSelected, Qt::QueuedConnection);
    connect(m_interface, &Interface::PropertiesChanged, this, &Wlan::onPropertiesChanged, Qt::QueuedConnection);
    connect(m_interface, &Interface::ScanDone, this, &Wlan::onScanDone, Qt::QueuedConnection);
}

void Wlan::onBSSAdded(const QDBusObjectPath& path, const QVariantMap& properties){
    emit BSSAdded(this, path, properties);
}
void Wlan::onBSSRemoved(const QDBusObjectPath& path){
    emit BSSRemoved(this, path);
}
void Wlan::onBlobAdded(const QString& name){
    if(!m_blobs.contains(name)){
        m_blobs.insert(name);
    }
    emit BlobAdded(this, name);
}
void Wlan::onBlobRemoved(const QString& name){
    m_blobs.remove(name);
    emit BlobRemoved(this, name);
}
void Wlan::onNetworkAdded(const QDBusObjectPath &path, const QVariantMap &properties){
    emit NetworkAdded(this, path, properties);
}
void Wlan::onNetworkRemoved(const QDBusObjectPath &path){
    emit NetworkRemoved(this, path);
}
void Wlan::onNetworkSelected(const QDBusObjectPath &path){
    emit NetworkSelected(this, path);
}
void Wlan::onPropertiesChanged(const QVariantMap &properties){
    emit PropertiesChanged(this, properties);
}
void Wlan::onScanDone(bool success){
    emit ScanDone(this, success);
}
std::string Wlan::exec(QString cmd){
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.toStdString().c_str(), "r"), pclose);
    if(!pipe){
        throw std::runtime_error("popen() failed!");
    }
    while(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr){
        result += buffer.data();
    }
    return result;
}
