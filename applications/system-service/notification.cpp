#include <QFontDatabase>

#include "notification.h"
#include "notificationapi.h"
#include "appsapi.h"
#include "systemapi.h"

Notification::Notification(const QString& path, const QString& identifier, const QString& owner, const QString& application, const QString& text, const QString& icon, QObject* parent)
 : QObject(parent),
   m_path(path),
   m_identifier(identifier),
   m_owner(owner),
   m_application(application),
   m_text(text),
   m_icon(icon) {
    m_created = QDateTime::currentSecsSinceEpoch();
    if(!icon.isEmpty()){
        return;
    }
    auto app = appsAPI->getApplication(m_application);
    if(app != nullptr && !app->icon().isEmpty()){
        m_icon = app->icon();
    }
}

void Notification::display(){
    if(!hasPermission("notification")){
        return;
    }
    if(notificationAPI->locked()){
        qDebug() << "Queueing notification display";
        notificationAPI->notificationDisplayQueue.append(this);
        return;
    }
    notificationAPI->lock();
    Oxide::dispatchToMainThread([=]{
        qDebug() << "Displaying notification" << identifier();
        auto path = appsAPI->currentApplicationNoSecurityCheck();
        Application* resumeApp = nullptr;
        if(path.path() != "/"){
            resumeApp = appsAPI->getApplication(path);
            resumeApp->interruptApplication();
        }
        paintNotification(resumeApp);
    });
}

void Notification::remove(){
    if(!hasPermission("notification")){
        return;
    }
    notificationAPI->remove(this);
    emit removed();
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

bool Notification::hasPermission(QString permission, const char* sender){ return notificationAPI->hasPermission(permission, sender); }

void Notification::paintNotification(Application *resumeApp) {
    qDebug() << "Painting notification" << identifier();
    dispatchToMainThread([this] { screenBackup = screenAPI->copy(); });
    updateRect = notificationAPI->paintNotification(text(), m_icon);
    qDebug() << "Painted notification" << identifier();
    emit displayed();
    QTimer::singleShot(2000, [this, resumeApp] {
        dispatchToMainThread([this] {
            auto frameBuffer = EPFrameBuffer::framebuffer();
            QPainter painter(frameBuffer);
            painter.drawImage(updateRect, screenBackup, updateRect);
            painter.end();
            EPFrameBuffer::sendUpdate(
                updateRect,
                EPFrameBuffer::Mono,
                EPFrameBuffer::FullUpdate,
                true
            );
            qDebug() << "Finished displaying notification" << identifier();
            EPFrameBuffer::waitForLastUpdate();
        });
        if (!notificationAPI->notificationDisplayQueue.isEmpty()) {
            Oxide::dispatchToMainThread([resumeApp] {
                notificationAPI
                    ->notificationDisplayQueue
                    .takeFirst()
                    ->paintNotification(resumeApp);
            });
            return;
        }
        if (resumeApp != nullptr) {
            resumeApp->uninterruptApplication();
        }
        notificationAPI->unlock();
    });
}
#include "moc_notification.cpp"
