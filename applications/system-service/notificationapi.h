#ifndef NOTIFICATIONAPI_H
#define NOTIFICATIONAPI_H

#include <QDebug>
#include <QtDBus>
#include <QMutex>

#include <liboxide.h>

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
    static NotificationAPI* singleton(NotificationAPI* self = nullptr);
    NotificationAPI(QObject* parent);
    ~NotificationAPI();
    bool enabled();
    void setEnabled(bool enabled);

    Q_INVOKABLE QDBusObjectPath get(QString identifier);

    QList<QDBusObjectPath> getAllNotifications();
    QList<QDBusObjectPath> getUnownedNotifications();
    QList<Notification*> notificationDisplayQueue;

    Notification* add(const QString& identifier, const QString& owner, const QString& application, const QString& text, const QString& icon);
    Notification* getByIdentifier(const QString& identifier);
    QRect paintNotification(const QString& text, const QString& iconPath);
    void errorNotification(const QString& text);
    QImage notificationImage(const QString& text, const QString& iconPath);
    void drawNotificationText(const QString& text, QColor color = Qt::black, QColor background = Qt::transparent);

public slots:
    QDBusObjectPath add(const QString& identifier, const QString& application, const QString& text, const QString& icon, QDBusMessage message);
    bool take(QString identifier, QDBusMessage message);
    QList<QDBusObjectPath> notifications(QDBusMessage message);
    void remove(Notification* notification);
    bool locked();
    void lock();
    void unlock();

signals:
    void notificationAdded(QDBusObjectPath);
    void notificationRemoved(QDBusObjectPath);
    void notificationChanged(QDBusObjectPath);


private:
    bool m_enabled;
    QMap<QString, Notification*> m_notifications;
    QMutex m_lock;

    QString getPath(QString id);
};


#endif // NOTIFICATIONAPI_H
