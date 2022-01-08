#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <QObject>
#include <QImage>
#include <QtDBus>

#include <liboxide.h>

#include "application.h"

class Notification : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_NOTIFICATION_INTERFACE)
    Q_PROPERTY(QString identifier READ identifier)
    Q_PROPERTY(int created READ created)
    Q_PROPERTY(QString application READ application WRITE setApplication)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString icon READ icon WRITE setIcon)
public:
    Notification(const QString& path, const QString& identifier, const QString& owner, const QString& application, const QString& text, const QString& icon, QObject* parent);
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
    int created(){ return m_created; }
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
    void setIcon(QString icon);

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
    void paintNotification(Application* resumeApp);

signals:
    void changed(QVariantMap);
    void removed();
    void displayed();
    void clicked();

private:
    QString m_path;
    QString m_identifier;
    int m_created;
    QString m_owner;
    QString m_application;
    QString m_text;
    QString m_icon;
    QImage screenBackup;
    QRect updateRect;

    void dispatchToMainThread(std::function<void()> callback);
    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
};

#endif // NOTIFICATION_H
