#include <QFontDatabase>

#include "notification.h"
#include "notificationapi.h"
#include "appsapi.h"


void Notification::display(){
    auto path= appsAPI->currentApplication();
    Application* resumeApp = nullptr;
    if(path.path() != "/"){
        resumeApp = appsAPI->getApplication(path);
        resumeApp->pause(false);
    }
    auto size = EPFrameBuffer::framebuffer()->size();
    QRect rect(0, 0, size.width(), size.height());
    QPainter painter(EPFrameBuffer::framebuffer());
    // TODO draw notification on screen
    // Using drawText crashes on fonts
    // painter.drawText(QPointF(0, 0), text());
    EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::FullUpdate, true);
    emit displayed();
    if(resumeApp != nullptr){
        QTimer::singleShot(2000, [=]{
            resumeApp->resume();
        });
    }

}

void Notification::remove(){
    notificationAPI->remove(this);
    emit removed();
}
