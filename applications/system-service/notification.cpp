#include <QFontDatabase>

#include "notification.h"
#include "notificationapi.h"
#include "appsapi.h"

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
    dispatchToMainThread([=]{
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

void Notification::dispatchToMainThread(std::function<void()> callback){
    if(this->thread() == qApp->thread()){
        callback();
        return;
    }
    // any thread
    QTimer* timer = new QTimer();
    timer->moveToThread(qApp->thread());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [=](){
        // main thread
        callback();
        timer->deleteLater();
    });
    QMetaObject::invokeMethod(timer, "start", Qt::BlockingQueuedConnection, Q_ARG(int, 0));
}
void Notification::paintNotification(Application* resumeApp){
    auto frameBuffer = EPFrameBuffer::framebuffer();
    qDebug() << "Waiting for other painting to finish...";
    while(frameBuffer->paintingActive()){
        EPFrameBuffer::waitForLastUpdate();
    }
    qDebug() << "Painting notification" << identifier();
    screenBackup = frameBuffer->copy();
    qDebug() << "Painting to framebuffer...";
    QPainter painter(frameBuffer);
    auto size = frameBuffer->size();
    auto fm = painter.fontMetrics();
    auto padding = 10;
    auto radius = 10;
    QImage icon(m_icon);
    auto iconSize = icon.isNull() ? 0 : 50;
    auto width = fm.horizontalAdvance(text()) + iconSize + (padding * 3);
    auto height = max(fm.height(), iconSize) + (padding * 2);
    auto left = size.width() - width;
    auto top = size.height() - height;
    updateRect = QRect(left, top, width, height);
    painter.fillRect(updateRect, Qt::black);
    painter.setPen(Qt::black);
    painter.drawRoundedRect(updateRect, radius, radius);
    painter.setPen(Qt::white);
    QRect textRect(left + padding, top + padding, width - iconSize - (padding * 2), height - padding);
    painter.drawText(textRect, Qt::AlignCenter, text());
    painter.end();
    qDebug() << "Updating screen " << updateRect << "...";
    EPFrameBuffer::sendUpdate(updateRect, EPFrameBuffer::Mono, EPFrameBuffer::PartialUpdate, true);
    if(!icon.isNull()){
        QPainter painter2(frameBuffer);
        QRect iconRect(size.width() - iconSize - padding, top + padding, iconSize, iconSize);
        painter2.fillRect(iconRect, Qt::white);
        painter2.drawImage(iconRect, icon);
        painter2.end();
        EPFrameBuffer::sendUpdate(iconRect, EPFrameBuffer::Mono, EPFrameBuffer::PartialUpdate, true);
    }
    EPFrameBuffer::waitForLastUpdate();
    qDebug() << "Painted notification" << identifier();
    emit displayed();
    QTimer::singleShot(2000, [this, resumeApp]{
        QPainter painter(EPFrameBuffer::framebuffer());
        painter.drawImage(updateRect, screenBackup, updateRect);
        painter.end();
        EPFrameBuffer::sendUpdate(updateRect, EPFrameBuffer::Mono, EPFrameBuffer::FullUpdate, true);
        qDebug() << "Finished displaying notification" << identifier();
        EPFrameBuffer::waitForLastUpdate();
        if(!notificationAPI->notificationDisplayQueue.isEmpty()){
            dispatchToMainThread([resumeApp] {
                notificationAPI->notificationDisplayQueue.takeFirst()->paintNotification(resumeApp);
            });
            return;
        }
        if(resumeApp != nullptr){
            resumeApp->uninterruptApplication();
        }
        notificationAPI->unlock();
    });
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
