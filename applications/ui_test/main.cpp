#include <QGuiApplication>
#include <QPainter>

#include <liboxide.h>
#include <liboxide/tarnish.h>

using namespace Oxide::Sentry;
using namespace Oxide;

int qExit(int ret){
    QTimer::singleShot(0, [ret](){
        qApp->exit(ret);
    });
    return qApp->exec();
}

int main(int argc, char *argv[]){
    QGuiApplication app(argc, argv);
    sentry_init("ui_test", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("ui_test");
    app.setApplicationVersion(APP_VERSION);
    qDebug() << "registerChild()";
    Tarnish::registerChild();
    qDebug() << "createFrameBuffer()";
    if(Tarnish::createFrameBuffer(500, 500) == -1){
        qDebug() << "Failed to create framebuffer";
        return EXIT_FAILURE;
    }
    qDebug() << "frameBufferImage()";
    Tarnish::lockFrameBuffer();
    auto image = Tarnish::frameBufferImage();
    if(image == nullptr){
        qDebug() << "Failed to get frame buffer image";
        return EXIT_FAILURE;
    }
    qDebug() << "width:" << image->width();
    qDebug() << "height:" << image->height();
    qDebug() << "bytes:" << image->sizeInBytes();
    qDebug() << "QPainter()";
    QPainter painter(image);
    qDebug() << "QPainter::setPen()";
    painter.setPen(Qt::black);
    qDebug() << "QPainter::drawText()";
    painter.drawText(50, 50, "Hello world!");
    qDebug() << "QPainter::end()";
    painter.end();
    qDebug() << "QImage::save()";
    if(!image->save("/tmp/ui_test.bmp", "BMP", 100)){
        qDebug() << "Failed to save image";
    }
    qDebug() << "screenUpdate()";
    Tarnish::screenUpdate();
    return EXIT_SUCCESS;
}
