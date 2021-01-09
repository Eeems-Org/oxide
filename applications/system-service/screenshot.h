#ifndef SCREENSHOT_H
#define SCREENSHOT_H
#include <QObject>
#include <QtDBus>
#include <QMutex>

#include <sys/stat.h>
#include <sys/types.h>


#include "dbussettings.h"

class Screenshot : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_SCREENSHOT_INTERFACE)
    Q_PROPERTY(QByteArray blob READ blob WRITE setBlob)
    Q_PROPERTY(QString path READ getPath)
public:
    Screenshot(QString path, QString filePath, QObject* parent) : QObject(parent), m_path(path), mutex() {
        m_file = new QFile(filePath);
        if(!m_file->open(QIODevice::ReadWrite)){
            qDebug() << "Unable to open screenshot file" << m_file->fileName();
        }
    }
    ~Screenshot(){
        unregisterPath();
        if(m_file->isOpen()){
            m_file->close();
        }
        delete m_file;
    }
    QString path() { return m_path; }
    QDBusObjectPath qPath(){ return QDBusObjectPath(path()); }
    void registerPath(){
        auto bus = QDBusConnection::systemBus();
        bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
        if(bus.registerObject(path(), this, QDBusConnection::ExportAllContents)){
            qDebug() << "Registered" << path() << OXIDE_APPLICATION_INTERFACE;
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
    QByteArray blob(){
        if(!hasPermission("screen")){
            return QByteArray();
        }
        mutex.lock();
        if(!m_file->isOpen() && !m_file->open(QIODevice::ReadWrite)){
            qDebug() << "Unable to open screenshot file" << m_file->fileName();
            mutex.unlock();
            return QByteArray();
        }
        m_file->seek(0);
        auto data = m_file->readAll();
        mutex.unlock();
        return data;
    }
    void setBlob(QByteArray blob){
        if(!hasPermission("screen")){
            return;
        }
        mutex.lock();
        if(!m_file->isOpen() && !m_file->open(QIODevice::ReadWrite)){
            qDebug() << "Unable to open screenshot file" << m_file->fileName();
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
    QString getPath(){
        if(!hasPermission("screen")){
            return "";
        }
        return m_file->fileName();
    }

signals:
    void modified();
    void removed();

public slots:
    void remove(){
        if(!hasPermission("screen")){
            return;
        }
        mutex.lock();
        m_file->remove();
        m_file->close();
        mutex.unlock();
        emit removed();
    }

private:
    QString m_path;
    QFile* m_file;
    QMutex mutex;

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
};

#endif // SCREENSHOT_H
