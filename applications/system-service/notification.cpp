#include <QFontDatabase>

#include "notification.h"
#include "notificationapi.h"
#include "appsapi.h"


void Notification::display(){
    if(!hasPermission("notification")){
        return;
    }
    notificationAPI->lock();
    qDebug() << "Displaying notification" << identifier();
    auto path = appsAPI->currentApplicationNoSecurityCheck();
    Application* resumeApp = nullptr;
    if(path.path() != "/"){
        resumeApp = appsAPI->getApplication(path);
        resumeApp->interruptApplication();
    }
    auto frameBuffer = EPFrameBuffer::framebuffer();
    dispatchToMainThread([=]{
        auto backup = frameBuffer->copy();
        const QRect rect = paintNotification();
        emit displayed();
        QTimer::singleShot(2000, [=]{
            QPainter painter(frameBuffer);
            painter.drawImage(rect, backup, rect);
            EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::Mono, EPFrameBuffer::FullUpdate, true);
            if(resumeApp != nullptr){
                resumeApp->uninterruptApplication();
            }
            qDebug() << "Finished displaying notification" << identifier();
            notificationAPI->unlock();
        });
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
const QRect Notification::paintNotification(){
    qDebug() << "Painting notification" << identifier();
    auto frameBuffer = EPFrameBuffer::framebuffer();
    qDebug() << "Waiting for other painting to finish...";
    while(frameBuffer->paintingActive()){
        this->thread()->yieldCurrentThread();
    }
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
    QRect rect(left, top, width, height);
    painter.fillRect(rect, Qt::black);
    painter.setPen(Qt::black);
    painter.drawRoundedRect(rect, radius, radius);
    painter.setPen(Qt::white);
    painter.drawText(rect, Qt::AlignCenter, text());
    painter.end();

    qDebug() << "Updating screen " << rect << "...";
    EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::Mono, EPFrameBuffer::PartialUpdate, true);
    EPFrameBuffer::waitForLastUpdate();
    qDebug() << "Painted notification" << identifier();
    return rect;
}

bool Notification::hasPermission(QString permission, const char* sender){ return notificationAPI->hasPermission(permission, sender); }
