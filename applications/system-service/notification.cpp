#include <QFontDatabase>

#include "notification.h"
#include "notificationapi.h"
#include "appsapi.h"


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
    auto width = fm.width(text()) + (padding * 2);
    auto height = fm.height() + (padding * 2);
    auto left = size.width() - width;
    auto top = size.height() - height;
    updateRect = QRect(left, top, width, height);
    painter.fillRect(updateRect, Qt::black);
    painter.setPen(Qt::black);
    painter.drawRoundedRect(updateRect, radius, radius);
    painter.setPen(Qt::white);
    painter.drawText(updateRect, Qt::AlignCenter, text());
    painter.end();
    qDebug() << "Updating screen " << updateRect << "...";
    EPFrameBuffer::sendUpdate(updateRect, EPFrameBuffer::Mono, EPFrameBuffer::PartialUpdate, true);
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

bool Notification::hasPermission(QString permission, const char* sender){ return notificationAPI->hasPermission(permission, sender); }
