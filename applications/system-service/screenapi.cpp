#include "screenapi.h"
#include "notificationapi.h"
#include "systemapi.h"

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
        bool saved = (
            systemAPI->landscape()
                ? screen.transformed(QTransform().rotate(270.0))
                : screen
        ).save(filePath);
        if(!saved){
            qDebug() << "Failed to take screenshot";
        }else{
            path = addScreenshot(filePath)->qPath();
        }
        auto frameBuffer = EPFrameBuffer::framebuffer();
        QPainter painter(frameBuffer);
        painter.drawImage(rect, screen, rect);
        painter.end();
        EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::PartialUpdate, true);
        notificationAPI->add(
            QUuid::createUuid().toString(),
            "codes.eeems.tarnish",
            "codes.eeems.tarnish",
            saved ? "Screenshot taken" : "Failed to take screenshot",
            saved ? filePath : ""
        )->display();
        return path;
    });
}

bool ScreenAPI::drawFullscreenImage(QString path, float rotate) {
    if (!hasPermission("screen")) {
        return false;
    }
    if (!QFile(path).exists()) {
        qDebug() << "Can't find image" << path;
        return false;
    }
    QImage img(path);
    if (img.isNull()) {
        qDebug() << "Image data invalid" << path;
        return false;
    }
    if(rotate){
        img = img.transformed(QTransform().rotate(rotate));
    }
    Oxide::Sentry::sentry_transaction(
        "screen", "drawFullscrenImage",
        [img, path](Oxide::Sentry::Transaction *t) {
            Q_UNUSED(t);
            Oxide::dispatchToMainThread([img] {
                auto frameBuffer = EPFrameBuffer::framebuffer();
                QRect rect = frameBuffer->rect();
                QPainter painter(frameBuffer);
                painter.drawImage(rect, img);
                painter.end();
                EPFrameBuffer::sendUpdate(
                    rect,
                    EPFrameBuffer::HighQualityGrayscale,
                    EPFrameBuffer::FullUpdate,
                    true
                );
                EPFrameBuffer::waitForLastUpdate();
            });
        });
    return true;
}
#include "moc_screenapi.cpp"
