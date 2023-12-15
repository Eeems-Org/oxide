#include "screenapi.h"
#include "notificationapi.h"

QDBusObjectPath ScreenAPI::screenshot(){
    if(!hasPermission("screen")){
        return QDBusObjectPath("/");
    }
    qDebug() << "Taking screenshot";
    auto filePath = getNextPath();
#ifdef DEBUG
    qDebug() << "Using path" << filePath;
#endif
    return dispatchToMainThread<QDBusObjectPath>([this, filePath]{
        QImage screen = copy();
        QRect rect = notificationAPI->paintNotification("Taking Screenshot...", "");
        EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::Mono, EPFrameBuffer::PartialUpdate, true);
        QDBusObjectPath path("/");
        if(!screen.save(filePath)){
            qDebug() << "Failed to take screenshot";
        }else{
            path = addScreenshot(filePath)->qPath();
        }
        QPainter painter(EPFrameBuffer::framebuffer());
        painter.drawImage(rect, screen, rect);
        painter.end();
        EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::PartialUpdate, true);
        return path;
    });
}

#include "moc_screenapi.cpp"
