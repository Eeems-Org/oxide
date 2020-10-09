#include <QFontDatabase>

#include "notification.h"
#include "notificationapi.h"
#include "appsapi.h"


void Notification::display(){
    qDebug() << "Displaying notification" << identifier();
    auto path= appsAPI->currentApplication();
    Application* resumeApp = nullptr;
    if(path.path() != "/"){
        resumeApp = appsAPI->getApplication(path);
        resumeApp->pause(false);
    }
    auto size = EPFrameBuffer::framebuffer()->size();
    QRect rect(0, 0, size.width(), size.height());
    QPainter painter(EPFrameBuffer::framebuffer());
    if(QFontDatabase::supportsThreadedFontRendering()){
        paintNotification();
    }else{
        dispatchToMainThread([=]{
            paintNotification();
        });
    }
    emit displayed();
    if(resumeApp != nullptr){
        QTimer::singleShot(2000, [=]{
            resumeApp->resume();
        });
    }
    qDebug() << "Finished displaying notification" << identifier();
}

void Notification::remove(){
    notificationAPI->remove(this);
    emit removed();
}

void Notification::dispatchToMainThread(std::function<void()> callback){
    // any thread
    QTimer* timer = new QTimer();
    timer->moveToThread(qApp->thread());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [=]()
    {
        // main thread
        callback();
        timer->deleteLater();
    });
    QMetaObject::invokeMethod(timer, "start", Qt::BlockingQueuedConnection, Q_ARG(int, 0));
}
void Notification::paintNotification(){
    qDebug() << "Painting notification" << identifier();

    qDebug() << "Creating image...";
    QImage image(QSize(400, 300), EPFrameBuffer::framebuffer()->format());

    QRect rect(0, 0, 100, 100);

    qDebug() << "Painting image...";
    QPainter painter(EPFrameBuffer::framebuffer());
//    painter.fillRect(0, 0, image.width(), image.height(), QBrush(Qt::white));
    painter.fillRect(rect, QBrush(Qt::white));
    painter.setBrush(QBrush(Qt::black));
    painter.drawText(QPointF(0, 0), text());

//    qDebug() << "Painting to frame buffer...";
//    QRect rect(0, 0, image.width(), image.height());
//    QPainter painter2(EPFrameBuffer::framebuffer());
//    painter2.drawImage(rect, image);

    qDebug() << "Updating screen...";
    EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::PartialUpdate, true);
    EPFrameBuffer::waitForLastUpdate();
    qDebug() << "Painted notification" << identifier();
}
