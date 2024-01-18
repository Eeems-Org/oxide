#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <liboxide/eventfilter.h>
#include <liboxide/meta.h>
#include <liboxide/debug.h>
#include <liboxide/sentry.h>
#include <liboxide/oxideqml.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <errno.h>
#include <cstring>
#include <libblight.h>
#include <libblight/connection.h>

using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

#define DEBUG_EVENTS

int main(int argc, char *argv[]){
    qputenv("QMLSCENE_DEVICE", "software");
    qputenv("QT_QUICK_BACKEND","software");
    qputenv("QT_QPA_PLATFORM", "minimal");
    QGuiApplication app(argc, argv);
    sentry_init("decay", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("test");
    app.setApplicationVersion(APP_VERSION);
    QQmlApplicationEngine engine;
    registerQML(&engine);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    engine.rootObjects().first()->installEventFilter(new EventFilter(&app));
    QTimer::singleShot(0, []{
#ifdef EPAPER
        Blight::connect(true);
#else
        Blight::connect(false);
#endif
        if(!Blight::exists()){
            O_WARNING("Service not found!");
            qApp->exit(EXIT_FAILURE);
            return;
        }
        int bfd = Blight::open();
        if(bfd < 0){
            O_WARNING(std::strerror(errno));
            qApp->exit(EXIT_FAILURE);
            return;
        }
        O_DEBUG("Connection file descriptor:" << bfd);
        QRect geometry(50, 50, 100, 100);
        QImage blankImage(geometry.size(), QImage::Format_ARGB32_Premultiplied);
        blankImage.fill(Qt::black);
        size_t size = blankImage.sizeInBytes();
        auto buffer = Blight::createBuffer(
            geometry.x(),
            geometry.y(),
            geometry.width(),
            geometry.height(),
            blankImage.bytesPerLine(),
            Blight::Format::Format_ARGB32_Premultiplied
        );
        if(buffer->fd == -1 || buffer->data == nullptr){
            O_WARNING("Failed to create buffer:" << strerror(errno));
            qApp->exit(EXIT_FAILURE);
            return;
        }
        memcpy(buffer->data, blankImage.constBits(), size);
        auto image = new QImage(
            buffer->data,
            blankImage.width(),
            blankImage.height(),
            blankImage.bytesPerLine(),
            blankImage.format()
        );
        if(image->isNull()){
            O_WARNING("Null image");
        }
        if(image->size() != geometry.size()){
            O_WARNING("Invalid size" << image->size());
        }
        std::string id = Blight::addSurface(
            buffer->fd,
            geometry.x(),
            geometry.y(),
            image->width(),
            image->height(),
            image->bytesPerLine(),
            (Blight::Format)image->format()
        );
        if(id.empty()){
            O_WARNING("No identifier provided");
        }
        O_DEBUG("Surface added:" << id.c_str());
        auto connection = new Blight::Connection(bfd);
        connection->onDisconnect([](int res){
            if(res){
                qApp->exit(res);
            }
        });
        sleep(1);
        O_DEBUG("Switching to gray");
        image->fill(Qt::gray);
        O_DEBUG("Repainting" << id.c_str());
        connection->repaint(
            id,
            0,
            0,
            geometry.width(),
            geometry.height()
        );
        O_DEBUG("Done!");
        delete buffer;
        delete connection;
        QTimer::singleShot(1000, []{ qApp->exit(); });
    });
    return app.exec();
}
