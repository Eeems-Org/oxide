#include <QPainterPath>

#include "notificationapi.h"
#include "systemapi.h"
#include "dbusservice.h"

#include <liboxide/oxideqml.h>
#include <libblight.h>

NotificationAPI* NotificationAPI::singleton(NotificationAPI* self){
    static NotificationAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}

NotificationAPI::NotificationAPI(QObject* parent)
: APIBase(parent),
  notificationDisplayQueue(),
  m_enabled(false),
  m_notifications(),
  m_lock()
{
    Oxide::Sentry::sentry_transaction("apps", "init", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::sentry_span(t, "singleton", "Setup singleton", [this]{
            singleton(this);
        });
    });
}

NotificationAPI::~NotificationAPI(){}

bool NotificationAPI::enabled(){ return m_enabled; }

void NotificationAPI::setEnabled(bool enabled){
    m_enabled = enabled;
    O_INFO("Notification API" << enabled);
    for(auto notification : m_notifications.values()){
        if(enabled){
            notification->registerPath();
        }else{
            notification->unregisterPath();
        }
    }
}

void NotificationAPI::startup(){
    auto engine = dbusService->engine();
    engine->load("qrc:/notification.qml");
    m_window = static_cast<QQuickWindow*>(engine->rootObjects().last());
    m_window->setProperty("WA_WAVEFORM", Blight::WaveformMode::Mono);
    auto buffer = Oxide::QML::getSurfaceForWindow(m_window);
    getCompositorDBus()->setFlags(
        QString("connection/%1/surface/%2")
            .arg(getpid())
            .arg(buffer->surface),
        QStringList() << "system"
    );
    m_window->setProperty("notificationVisible", false);
    m_window->lower();
}

QDBusObjectPath NotificationAPI::get(QString identifier){
    if(!hasPermission("notification")){
        return QDBusObjectPath("/");
    }
    auto notification = getByIdentifier(identifier);
    if(notification == nullptr){
        return QDBusObjectPath("/");
    }
    return notification->qPath();
}

QList<QDBusObjectPath> NotificationAPI::getAllNotifications(){
    QList<QDBusObjectPath> result;
    if(!hasPermission("notification")){
        return result;
    }
    for(auto notification : m_notifications.values()){
        result.append(notification->qPath());
    }
    return result;
}

QList<QDBusObjectPath> NotificationAPI::getUnownedNotifications(){
    QList<QDBusObjectPath> result;
    if(!hasPermission("notification")){
        return result;
    }
    QStringList names = QDBusConnection::systemBus().interface()->registeredServiceNames();
    for(auto notification : m_notifications.values()){
        if(!names.contains(notification->owner())){
            result.append(notification->qPath());
        }
    }
    return result;
}

Notification* NotificationAPI::add(const QString& identifier, const QString& owner, const QString& application, const QString& text, const QString& icon){
    if(m_notifications.contains(identifier)){
        return nullptr;
    }
    auto notification = new Notification(getPath(identifier), identifier, owner, application, text, icon, this);
    m_notifications.insert(identifier, notification);
    auto path = notification->qPath();
    connect(notification, &Notification::changed, this, [this, path]{
        emit notificationChanged(path);
    });
    if(m_enabled){
        notification->registerPath();
    }
    emit notificationAdded(path);
    return notification;
}

Notification* NotificationAPI::getByIdentifier(const QString& identifier){
    if(!m_notifications.contains(identifier)){
        return nullptr;
    }
    return m_notifications.value(identifier);
}

QQuickWindow* NotificationAPI::paintNotification(const QString& text, const QString& iconPath){
    m_window->setProperty("text", text);
    if(!iconPath.isEmpty() && QFileInfo(iconPath).exists()){
        m_window->setProperty("image", QUrl::fromLocalFile(iconPath));
    }else{
        m_window->setProperty("image", "");
    }
    m_window->show();
    m_window->raise();
    m_window->setProperty("notificationVisible", true);
    return m_window;
}

void NotificationAPI::errorNotification(const QString &text){
    O_DEBUG("Displaying error text");
    notificationAPI->paintNotification(text, "");
}

QDBusObjectPath NotificationAPI::add(const QString& identifier, const QString& application, const QString& text, const QString& icon, QDBusMessage message){
    if(!hasPermission("notification")){
        return QDBusObjectPath("/");
    }
    auto notification = add(identifier, message.service(), application, text, icon);
    if(notification == nullptr){
        return QDBusObjectPath("/");
    }
    return notification->qPath();
}

bool NotificationAPI::take(QString identifier, QDBusMessage message){
    if(!hasPermission("notification")){
        return false;
    }
    if(!m_notifications.contains(identifier)){
        return false;
    }
    m_notifications.value(identifier)->setOwner(message.service());
    return true;
}

QList<QDBusObjectPath> NotificationAPI::notifications(QDBusMessage message){
    QList<QDBusObjectPath> result;
    if(!hasPermission("notification")){
        return result;
    }
    for(auto notification : m_notifications.values()){
        if(notification->owner() == message.service()){
            result.append(notification->qPath());
        }
    }
    return result;
}

void NotificationAPI::remove(Notification* notification){
    if(!hasPermission("notification")){
        return;
    }
    if(!m_notifications.contains(notification->identifier())){
        return;
    }
    m_notifications.remove(notification->identifier());
    emit notificationRemoved(notification->qPath());
}

bool NotificationAPI::locked(){
    if(!m_lock.tryLock(1)){
        return true;
    }
    m_lock.unlock();
    return false;
}

void NotificationAPI::lock() { m_lock.tryLock(1); }

void NotificationAPI::unlock() { m_lock.unlock(); }

QString NotificationAPI::getPath(QString id){
    static const QUuid NS = QUuid::fromString(QLatin1String("{66acfa80-020f-11eb-adc1-0242ac120002}"));
    id= QUuid::createUuidV5(NS, id).toString(QUuid::Id128);
    if(id.isEmpty()){
        id = QUuid::createUuid().toString(QUuid::Id128);
    }
    return QString(OXIDE_SERVICE_PATH "/notifications/") + id;
}
