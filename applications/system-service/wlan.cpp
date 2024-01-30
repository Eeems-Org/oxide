#include "wlan.h"
#include "bss.h"
#include "wifiapi.h"

Wlan::Wlan(QString path, QObject* parent) : QObject(parent), SysObject(path), m_blobs(), m_iface(){
    m_iface = QFileInfo(path).fileName();
    m_interface = nullptr;
}

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

void Wlan::removeInterface(){
    if(m_interface != nullptr){
        m_interface->deleteLater();
        m_interface = nullptr;
    }
}

QString Wlan::iface() { return m_iface; }

bool Wlan::up() { return !system(("/sbin/ifconfig " + iface() + " up").toStdString().c_str()); }

bool Wlan::down() { return !system(("/sbin/ifconfig " + iface() + " down").toStdString().c_str()); }

bool Wlan::isUp(){ return !system(("/sbin/ip addr show " + iface() + " | /bin/grep UP > /dev/null").toStdString().c_str()); }

Interface* Wlan::interface() { return m_interface; }

QSet<QString> Wlan::blobs(){ return m_blobs; }

QString Wlan::operstate(){
    if(hasProperty("operstate")){
        return QString(strProperty("operstate").c_str());
    }
    return "";
}

bool Wlan::pingIP(std::string ip, const char* port) {
    auto process = new QProcess();
    process->setProgram("/bin/bash");
    std::string cmd("{ echo -n > /dev/tcp/" + ip.substr(0, ip.length() - 1) + "/" + port + "; } > /dev/null 2>&1");
    process->setArguments(QStringList() << "-c" << cmd.c_str());
    process->start();
    if(!process->waitForFinished(100)){
        process->kill();
        return false;
    }
    return !process->exitCode();
}

bool Wlan::isConnected(){
    auto ip = exec("/sbin/ip r | /bin/grep " + iface() + " | /bin/grep default | /usr/bin/awk '{print $3}'");
    return ip != "" && (pingIP(ip, "53") || pingIP(ip, "80"));
}

int Wlan::link(){
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
    auto out = exec("/bin/grep " + iface() + " /proc/net/wireless | /usr/bin/awk '{print $3}'");
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

signed int Wlan::rssi(){
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
    auto out = exec("/bin/grep " + iface() + " /proc/net/wireless | /usr/bin/awk '{print $4}'");
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
std::string Wlan::exec(QString cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.toStdString().c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

#include "moc_wlan.cpp"
