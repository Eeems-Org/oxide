#include <QFontDatabase>

#include "notification.h"
#include "notificationapi.h"
#include "appsapi.h"
#include "screenapi.h"
#include "dbusservice.h"

#include <liboxide/oxideqml.h>

Notification::Notification(
  const QString& path,
  const QString& identifier,
  const QString& owner,
  const QString& application,
  const QString& text,
  const QString& icon,
  QObject* parent
)
: QObject(parent),
  m_path(path),
  m_identifier(identifier),
  m_owner(owner),
  m_application(application),
  m_text(text),
  m_icon(icon)
{
    m_created = QDateTime::currentSecsSinceEpoch();
    if(icon.isEmpty()){
        auto app = appsAPI->getApplication(m_application);
        if(app != nullptr && !app->icon().isEmpty()){
            m_icon = app->icon();
        }
    }
}

Notification::~Notification(){
    unregisterPath();
}

QString Notification::path() { return m_path; }

QDBusObjectPath Notification::qPath(){ return QDBusObjectPath(path()); }

void Notification::registerPath(){
    auto bus = QDBusConnection::systemBus();
    bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
    if(bus.registerObject(path(), this, QDBusConnection::ExportAllContents)){
        O_INFO("Registered" << path() << OXIDE_APPLICATION_INTERFACE);
    }else{
        O_WARNING("Failed to register" << path());
    }
}

void Notification::unregisterPath(){
    auto bus = QDBusConnection::systemBus();
    if(bus.objectRegisteredAt(path()) != nullptr){
        O_DEBUG("Unregistered" << path());
        bus.unregisterObject(path());
    }
}

QString Notification::identifier(){
    if(!hasPermission("notification")){
        return "";
    }
    return m_identifier;
}

int Notification::created(){ return m_created; }

QString Notification::application(){
    if(!hasPermission("notification")){
        return "";
    }
    return m_application;
}

void Notification::setApplication(QString application){
    if(!hasPermission("notification")){
        return;
    }
    m_application = application;
    QVariantMap result;
    result.insert("application", m_application);
    emit changed(result);
}

QString Notification::text(){
    if(!hasPermission("notification")){
        return "";
    }
    return m_text;
}

void Notification::setText(QString text){
    if(!hasPermission("notification")){
        return;
    }
    m_text = text;
    QVariantMap result;
    result.insert("text", m_text);
    emit changed(result);
}

QString Notification::icon(){
    if(!hasPermission("notification")){
        return "";
    }
    return m_icon;
}

void Notification::display(){
    if(!hasPermission("notification")){
        return;
    }
    if(notificationAPI->locked()){
        O_DEBUG("Queueing notification display");
        notificationAPI->notificationDisplayQueue.append(this);
        return;
    }
    notificationAPI->lock();
    O_DEBUG("Displaying notification" << identifier());
    paintNotification();
}

void Notification::remove(){
    if(!hasPermission("notification")){
        return;
    }
    notificationAPI->remove(this);
    emit removed();
}

void Notification::click(){
    if(!hasPermission("notification")){
        return;
    }
    emit clicked();
}

void Notification::setIcon(QString icon){
    if(!hasPermission("notification")){
        return;
    }
    if(icon.isEmpty()){
        auto application = appsAPI->getApplication(m_application);
        if(application != nullptr && !application->icon().isEmpty()){
            icon = application->icon();
        }
    }
    m_icon = icon;
    QVariantMap result;
    result.insert("icon", m_icon);
    emit changed(result);
}

QString Notification::owner(){
    if(!hasPermission("notification")){
        return "";
    }
    return m_owner;
}

void Notification::setOwner(QString owner){
    if(!hasPermission("notifications")){
        return;
    }
    m_owner = owner;
    QVariantMap result;
    result.insert("owner", m_owner);
    emit changed(result);
}

bool Notification::hasPermission(QString permission, const char* sender){
    return notificationAPI->hasPermission(permission, sender);
}

void Notification::paintNotification(){
    O_DEBUG("Painting notification" << identifier());
    auto notification = notificationAPI->paintNotification(m_text, m_icon);
    O_DEBUG("Painted notification" << identifier());
    emit displayed();
    QTimer::singleShot(2000, [this, notification] {
        O_DEBUG("Finished displaying notification" << identifier());
        if(!notificationAPI->notificationDisplayQueue.isEmpty()){
            notificationAPI
                ->notificationDisplayQueue
                .takeFirst()
                ->paintNotification();
            return;
        }else{
            notification->setProperty("notificationVisible", false);
        }
        notificationAPI->unlock();
    });
}
#include "moc_notification.cpp"
