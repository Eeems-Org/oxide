#include <QPainterPath>

#include "notificationapi.h"
#include "systemapi.h"

#include <liboxide/oxideqml.h>

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
    qDebug() << "Notification API" << enabled;
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

QRect NotificationAPI::paintNotification(const QString &text, const QString &iconPath){
    QImage notification = notificationImage(text, iconPath);
    return dispatchToMainThread<QRect>([&notification]{
        qDebug() << "Painting to framebuffer...";
        auto frameBuffer = getFrameBuffer();
        QPainter painter(frameBuffer);
        QPoint pos(0, frameBuffer->height() - notification.height());
        if(systemAPI->landscape()){
            notification = notification.transformed(QTransform().rotate(90.0));
            pos.setX(0);
            pos.setY(frameBuffer->height() - notification.height());
        }
        auto updateRect = notification.rect().translated(pos);
        painter.drawImage(updateRect, notification);
        painter.end();
        qDebug() << "Updating screen " << updateRect << "...";
        Oxide::QML::repaint(getFrameBufferWindow(), frameBuffer->rect(), Blight::Grayscale, true);
        return updateRect;
    });
}
void NotificationAPI::errorNotification(const QString &text) {
    dispatchToMainThread([] {
        auto frameBuffer = getFrameBuffer();
        qDebug() << "Waiting for other painting to finish...";
        while (frameBuffer->paintingActive()) {
            // TODO - don't spinlock
        }
        qDebug() << "Displaying error text";
        QPainter painter(frameBuffer);
        painter.fillRect(frameBuffer->rect(), Qt::white);
        painter.end();
        Oxide::QML::repaint(getFrameBufferWindow(), frameBuffer->rect(), Blight::Mono, true);
    });
    notificationAPI->paintNotification(text, "");
}
QImage NotificationAPI::notificationImage(const QString& text, const QString& iconPath){
    auto padding = 10;
    auto radius = 10;
    auto frameBuffer = getFrameBuffer();
    auto size = frameBuffer->size();
    auto boundingRect = QPainter(frameBuffer).fontMetrics().boundingRect(
        QRect(0, 0, size.width() / 2, size.height() / 8),
        Qt::AlignCenter | Qt::TextWordWrap, text
    );
    QImage icon(iconPath);
    auto iconSize = icon.isNull() ? 0 : 50;
    auto width = boundingRect.width() + iconSize + (padding * 3);
    auto height = max(boundingRect.height(), iconSize) + (padding * 2);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(image.rect(), radius, radius);
    QPen pen(Qt::white, 1);
    painter.setPen(pen);
    painter.fillPath(path, Qt::black);
    painter.drawPath(path);
    painter.setPen(Qt::white);
    QRect textRect(
        padding,
        padding,
        image.width() - iconSize - (padding * 2),
        image.height() - padding
    );
    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, text);
    if(!icon.isNull()){
        QRect iconRect(
            image.width() - iconSize - padding,
            padding,
            iconSize,
            iconSize
        );
        painter.fillRect(iconRect, Qt::white);
        painter.drawImage(iconRect, icon);
    }
    painter.end();
    return image;
}

void NotificationAPI::drawNotificationText(const QString& text, QColor color, QColor background){
    auto frameBuffer = getFrameBuffer();
    dispatchToMainThread([frameBuffer, text, color, background]{
        QPainter painter(frameBuffer);
        int padding = 10;
        auto size = frameBuffer->size();
        auto fm = painter.fontMetrics();
        int textHeight = fm.height() + padding;
        QImage textImage(
            size.width() - padding * 2,
            textHeight,
            background == Qt::transparent
                ? QImage::Format_ARGB32_Premultiplied
                : QImage::Format_RGB16
        );
        textImage.fill(Qt::transparent);
        QPainter painter2(&textImage);
        painter2.setPen(color);
        painter2.drawText(
            textImage.rect(),
            Qt::AlignVCenter | Qt::AlignRight,
            text
        );
        painter2.end();
        QPoint textPos(0 + padding, size.height() - textHeight);
        if(systemAPI->landscape()){
            textImage = textImage.transformed(QTransform().rotate(90.0));
            textPos.setX(0);
            textPos.setY(size.height() - textImage.height() - padding);
        }
        auto textRect = textImage.rect().translated(textPos);
        painter.drawImage(textRect, textImage);
        Oxide::QML::repaint(getFrameBufferWindow(), textRect, Blight::Grayscale);
        painter.end();
    });
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
