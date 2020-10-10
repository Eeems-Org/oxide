#ifndef NOTIFICATIONAPI_H
#define NOTIFICATIONAPI_H

#include <QDebug>
#include <QtDBus>

#include "dbussettings.h"
#include "apibase.h"
#include "notification.h"

#define notificationAPI NotificationAPI::singleton()

class NotificationAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_NOTIFICATIONS_INTERFACE)
    Q_PROPERTY(bool enabled READ enabled)
    Q_PROPERTY(QList<QDBusObjectPath> allNotifications READ getAllNotifications)
    Q_PROPERTY(QList<QDBusObjectPath> unownedNotifications READ getUnownedNotifications)
public:
    static NotificationAPI* singleton(NotificationAPI* self = nullptr){
        static NotificationAPI* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    NotificationAPI(QObject* parent) : APIBase(parent), m_enabled(false), m_notifications() {
        singleton(this);
    }
    ~NotificationAPI(){}
    bool enabled(){ return m_enabled; }
    void setEnabled(bool enabled){
        m_enabled = enabled;
        qDebug() << "Notification API" << enabled;
        for(auto notification : m_notifications.values()){
            if(enabled){
                notification->registerPath();
            }else{
                notification->unregisterPath();
            }
        }
    }

    Q_INVOKABLE QDBusObjectPath get(QString identifier){
        if(!m_notifications.contains(identifier)){
            return QDBusObjectPath("/");
        }
        return m_notifications.value(identifier)->qPath();
    }

    QList<QDBusObjectPath> getAllNotifications(){
        QList<QDBusObjectPath> result;
        for(auto notification : m_notifications.values()){
            result.append(notification->qPath());
        }
        return result;
    }
    QList<QDBusObjectPath> getUnownedNotifications(){
        QList<QDBusObjectPath> result;
        QStringList names = QDBusConnection::systemBus().interface()->registeredServiceNames();
        for(auto notification : m_notifications.values()){
            if(!names.contains(notification->owner())){
                result.append(notification->qPath());
            }
        }
        return result;
    }

public slots:
    QDBusObjectPath add(QString identifier, QString application, QString text, QString icon, QDBusMessage message){
        if(m_notifications.contains(identifier)){
            return QDBusObjectPath("/");
        }
        auto notification = new Notification(getPath(identifier), identifier, message.service(), application, text, icon, this);
        m_notifications.insert(identifier, notification);
        auto path = notification->qPath();
        connect(notification, &Notification::changed, this, [=]{
           emit notificationChanged(path);
        });
        if(m_enabled){
            notification->registerPath();
        }
        emit notificationAdded(path);
        return path;
    }
    bool take(QString identifier, QDBusMessage message){
        if(!m_notifications.contains(identifier)){
            return false;
        }
        m_notifications.value(identifier)->setOwner(message.service());
        return true;
    }
    QList<QDBusObjectPath> notifications(QDBusMessage message){
        QList<QDBusObjectPath> result;
        for(auto notification : m_notifications.values()){
            if(notification->owner() == message.service()){
                result.append(notification->qPath());
            }
        }
        return result;
    }

    void remove(Notification* notification){
        if(m_notifications.contains(notification->identifier())){
            m_notifications.remove(notification->identifier());
            emit notificationRemoved(notification->qPath());
        }
    }

signals:
    void notificationAdded(QDBusObjectPath);
    void notificationRemoved(QDBusObjectPath);
    void notificationChanged(QDBusObjectPath);


private:
    bool m_enabled;
    QMap<QString, Notification*> m_notifications;

    QString getPath(QString id){
        static const QUuid NS = QUuid::fromString(QLatin1String("{66acfa80-020f-11eb-adc1-0242ac120002}"));
        id= QUuid::createUuidV5(NS, id).toString(QUuid::Id128);
        if(id.isEmpty()){
            id = QUuid::createUuid().toString(QUuid::Id128);
        }
        return QString(OXIDE_SERVICE_PATH "/notifications/") + id;
    }
};


#endif // NOTIFICATIONAPI_H
