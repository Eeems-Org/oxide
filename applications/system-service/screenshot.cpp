#include "screenshot.h"
#include"screenapi.h"

Screenshot::Screenshot(QString path, QString filePath, QObject* parent) : QObject(parent), m_path(path), mutex() {
    m_file = new QFile(filePath);
    if(!m_file->open(QIODevice::ReadWrite)){
        O_WARNING("Unable to open screenshot file" << m_file->fileName());
    }
}

Screenshot::~Screenshot(){
    unregisterPath();
    if(m_file->isOpen()){
        m_file->close();
    }
    delete m_file;
}

QString Screenshot::path() { return m_path; }

QDBusObjectPath Screenshot::qPath(){ return QDBusObjectPath(path()); }

void Screenshot::registerPath(){
    auto bus = QDBusConnection::systemBus();
    bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
    if(bus.registerObject(path(), this, QDBusConnection::ExportAllContents)){
        O_INFO("Registered" << path() << OXIDE_APPLICATION_INTERFACE);
    }else{
        O_WARNING("Failed to register" << path());
    }
}

void Screenshot::unregisterPath(){
    auto bus = QDBusConnection::systemBus();
    if(bus.objectRegisteredAt(path()) != nullptr){
        O_INFO("Unregistered" << path());
        bus.unregisterObject(path());
    }
}

QByteArray Screenshot::blob(){
    if(!hasPermission("screen")){
        return QByteArray();
    }
    if(!m_file->exists() && !m_file->isOpen()){
        emit removed();
        return QByteArray();
    }
    mutex.lock();
    if(!m_file->isOpen() && !m_file->open(QIODevice::ReadWrite)){
        O_WARNING("Unable to open screenshot file" << m_file->fileName());
        mutex.unlock();
        return QByteArray();
    }
    m_file->seek(0);
    auto data = m_file->readAll();
    mutex.unlock();
    return data;
}

void Screenshot::setBlob(QByteArray blob){
    if(!hasPermission("screen")){
        return;
    }
    mutex.lock();
    if(!m_file->isOpen() && !m_file->open(QIODevice::ReadWrite)){
        O_WARNING("Unable to open screenshot file" << m_file->fileName());
        mutex.unlock();
        return;
    }
    m_file->seek(0);
    m_file->resize(blob.size());
    m_file->write(blob);
    m_file->flush();
    mutex.unlock();
    emit modified();
}

QString Screenshot::getPath(){
    if(!hasPermission("screen")){
        return "";
    }
    if(!m_file->exists()){
        emit removed();
        return "";
    }
    return m_file->fileName();
}

void Screenshot::remove(){
    if(!hasPermission("screen")){
        return;
    }
    mutex.lock();
    if(m_file->exists() && !m_file->remove()){
        O_INFO("Failed to remove screenshot" << path());
        mutex.unlock();
        return;
    }
    if(m_file->isOpen()){
        m_file->close();
    }
    mutex.unlock();
    O_INFO("Removed screenshot" << path());
    emit removed();
}

bool Screenshot::hasPermission(QString permission, const char* sender){ return screenAPI->hasPermission(permission, sender); }

#include "moc_screenshot.cpp"
