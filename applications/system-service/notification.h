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

    QString identifier(){
        if(!hasPermission("notification")){
            return "";
        }
        return m_identifier;
    }
    QString application(){
        if(!hasPermission("notification")){
            return "";
        }
        return m_application;
    }
    void setApplication(QString application){
        if(!hasPermission("notification")){
            return;
        }
        m_application = application;
        QVariantMap result;
        result.insert("application", m_application);
        emit changed(result);
    }
    QString text(){
        if(!hasPermission("notification")){
            return "";
        }
        return m_text;
    }
    void setText(QString text){
        if(!hasPermission("notification")){
            return;
        }
        m_text = text;
        QVariantMap result;
        result.insert("text", m_text);
        emit changed(result);
    }
    QString icon(){
        if(!hasPermission("notification")){
            return "";
        }
        return m_icon;
    }
    void setIcon(QString icon){
        if(!hasPermission("notification")){
            return;
        }
        m_icon = icon;
        QVariantMap result;
        result.insert("icon", m_icon);
        emit changed(result);
    }

    QString owner(){
        if(!hasPermission("notification")){
            return "";
        }
        return m_owner;
    }
    void setOwner(QString owner){
        if(!hasPermission("notifications")){
            return;
        }
        m_owner = owner;
        QVariantMap result;
        result.insert("owner", m_owner);
        emit changed(result);
    }

    Q_INVOKABLE void display();
    Q_INVOKABLE void remove();
    Q_INVOKABLE void click(){
        if(!hasPermission("notification")){
            return;
        }
        emit clicked();
    }

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
    bool hasPermission(QString permission);
};

#endif // NOTIFICATION_H
