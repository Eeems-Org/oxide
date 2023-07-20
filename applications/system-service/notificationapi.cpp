#include "notificationapi.h"
#include "guiapi.h"
#include "window.h"

#include <liboxide.h>

using namespace Oxide;

static Window* __window = nullptr;
Window* NotificationAPI::_window(){
    if(__window == nullptr){
        auto screenRect = deviceSettings.screenGeometry();
        int x = screenRect.width() / 2;
        int y = screenRect.height() / 8;
        __window = guiAPI->_createWindow(QRect(x, y * 7, x, y), QImage::Format_RGBA8888_Premultiplied);
        __window->setZ(std::numeric_limits<int>::max());
        __window->disableEventPipe();
        __window->setSystemWindow();
        __window->_setVisible(false);
        __window->_raise();
    }
    return __window;
}

NotificationAPI* NotificationAPI::singleton(NotificationAPI* self){
    static NotificationAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}

NotificationAPI::NotificationAPI(QObject* parent) : APIBase(parent), notificationDisplayQueue(), m_enabled(false), m_notifications(), m_lock() {
    Sentry::sentry_transaction("apps", "init", [this](Sentry::Transaction* t){
        Sentry::sentry_span(t, "singleton", "Setup singleton", [this]{
            singleton(this);
        });
    });
}

bool NotificationAPI::isEnabled(){ return m_enabled; }

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
    connect(notification, &Notification::changed, this, [this, path]{ emit notificationChanged(path); });
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

void NotificationAPI::paintNotification(const QString& text, const QString& iconPath){
    auto image = _window()->toImage();
    QPainter painter(&image);
    auto padding = 10;
    auto radius = 10;
    QImage icon(iconPath);
    auto iconSize = icon.isNull() ? 0 : 50;
    auto boundingRect = painter.fontMetrics().boundingRect(image.rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
    auto width = boundingRect.width() + iconSize + (padding * 3);
    auto height = std::max(boundingRect.height(), iconSize) + (padding * 2);
    auto left = image.width() - width;
    auto top = image.height() - height;
    painter.fillRect(image.rect(), Qt::transparent);
    QRect updateRect(left, top, width, height);
    painter.fillRect(updateRect, Qt::black);
    painter.setPen(Qt::black);
    painter.drawRoundedRect(updateRect, radius, radius);
    painter.setPen(Qt::white);
    QRect textRect(left + padding, top + padding, width - iconSize - (padding * 2), height - padding);
    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, text);
    if(!icon.isNull()){
        QRect iconRect(image.width() - iconSize - padding, top + padding, iconSize, iconSize);
        painter.fillRect(iconRect, Qt::white);
        painter.drawImage(iconRect, icon);
    }
    painter.end();
    if(_window()->_isVisible()){
        _window()->_repaint(image.rect(), EPFrameBuffer::Mono, 0, false);
    }else{
        _window()->_setVisible(true);
    }
}

void NotificationAPI::errorNotification(const QString& text){
    // TODO - clear screen?
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
