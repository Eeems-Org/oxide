#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <QObject>
#include <QtDBus>

#include "dbussettings.h"

class Notification : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_NOTIFICATION_INTERFACE)
    Q_PROPERTY(QString identifier READ identifier)
    Q_PROPERTY(QString application READ application WRITE setApplication)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString icon READ icon WRITE setIcon)
public:
    Notification(QString path, QString identifier, QString owner, QString application, QString text, QString icon, QObject* parent)
     : QObject(parent),
       m_path(path),
       m_identifier(identifier),
       m_owner(owner),
       m_application(application),
       m_text(text),
       m_icon(icon) {}
    ~Notification(){
        unregisterPath();
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

    QString identifier(){ return m_identifier; }
    QString application(){ return m_application; }
    void setApplication(QString application){
        m_application = application;
        QVariantMap result;
        result.insert("application", m_application);
        emit changed(result);
    }
    QString text(){ return m_text; }
    void setText(QString text){
        m_text = text;
        QVariantMap result;
        result.insert("text", m_text);
        emit changed(result);
    }
    QString icon(){ return m_icon; }
    void setIcon(QString icon){
        m_icon = icon;
        QVariantMap result;
        result.insert("icon", m_icon);
        emit changed(result);
    }

    QString owner(){  return m_owner; }
    void setOwner(QString owner){ m_owner = owner;}

    Q_INVOKABLE void display();
    Q_INVOKABLE void remove();
    Q_INVOKABLE void click(){ emit clicked(); }

signals:
    void changed(QVariantMap);
    void removed();
    void displayed();
    void clicked();

private:
    QString m_path;
    QString m_identifier;
    QString m_owner;
    QString m_application;
    QString m_text;
    QString m_icon;

    void dispatchToMainThread(std::function<void()> callback);
    const QRect paintNotification();
};

#endif // NOTIFICATION_H
