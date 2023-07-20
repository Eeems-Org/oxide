#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <QObject>
#include <QImage>
#include <QtDBus>

#include "apibase.h"
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
    ~Notification();
    QString path();
    QDBusObjectPath qPath();
    void registerPath();
    void unregisterPath();

    QString identifier();
    int created(){ return m_created; }
    QString application();
    void setApplication(QString application);
    QString text();
    void setText(QString text);
    QString icon();
    void setIcon(QString icon);

    QString owner();
    void setOwner(QString owner);

    Q_INVOKABLE void display();
    Q_INVOKABLE void remove();
    Q_INVOKABLE void click();
    void paintNotification();

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

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
};

#endif // NOTIFICATION_H
