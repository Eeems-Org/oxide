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
    QTimer::singleShot(0, &app, []{
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
        {
            auto maybe = Blight::clipboard();
            if(!maybe.has_value()){
                O_WARNING("Could not read clipboard!");
                qApp->exit(EXIT_FAILURE);
                return;
            }
            auto clipboard = maybe.value();
            O_DEBUG(clipboard.name.c_str() << QString::fromStdString(clipboard.to_string()));
            O_DEBUG("Updating clipboard");
            if(!clipboard.set("Test")){
                O_WARNING("Could not update clipboard!");
                qApp->exit(EXIT_FAILURE);
                return;
            }
            O_DEBUG(clipboard.name.c_str() << QString::fromStdString(clipboard.to_string()));
        }
        int bfd = Blight::open();
        if(bfd < 0){
            O_WARNING(std::strerror(errno));
            qApp->exit(EXIT_FAILURE);
            return;
        }
        O_DEBUG("Connection file descriptor:" << bfd);
        QRect geometry(0, 0, 100, 100);
        QImage blankImage(geometry.size(), QImage::Format_RGB16);
        blankImage.fill(Qt::black);
        auto maybe = Blight::createBuffer(
            geometry.x(),
            geometry.y(),
            geometry.width(),
            geometry.height(),
            blankImage.bytesPerLine(),
            Blight::Format::Format_RGB16
        );
        if(!maybe.has_value()){
            O_WARNING("Failed to create buffer:" << strerror(errno));
            qApp->exit(EXIT_FAILURE);
            return;
        }
        auto buffer = maybe.value();
        if(buffer->fd == -1 || buffer->data == nullptr){
            O_WARNING("Failed to create buffer:" << strerror(errno));
            qApp->exit(EXIT_FAILURE);
            return;
        }
        memcpy(buffer->data, blankImage.constBits(), buffer->size());
        QImage image(
            buffer->data,
            blankImage.width(),
            blankImage.height(),
            blankImage.bytesPerLine(),
            blankImage.format()
        );
        if(image.isNull()){
            O_WARNING("Null image");
        }
        if(image.size() != geometry.size()){
            O_WARNING("Invalid size" << image.size());
        }
        Blight::addSurface(buffer);
        if(buffer->surface.empty()){
            O_WARNING("No identifier provided");
            qApp->exit(EXIT_FAILURE);
            return;
        }
        O_DEBUG("Surface added:" << buffer->surface.c_str());
        auto connection = new Blight::Connection(bfd);
        connection->onDisconnect([](int res){
            if(res){
                qApp->exit(res);
            }
        });
        sleep(1);
        O_DEBUG("Switching to gray");
        image.fill(Qt::gray);
        O_DEBUG("Repainting" << buffer->surface.c_str());
        auto ack = connection->repaint(
            buffer,
            0,
            0,
            geometry.width(),
            geometry.height()
        );
        if(ack.has_value()){
            ack.value()->wait();
        }
        sleep(1);
        O_DEBUG("Moving " << buffer->surface.c_str());
        geometry.setTopLeft(QPoint(100, 100));
        connection->move(buffer, geometry.x(), geometry.y());
        O_DEBUG("Move done" << buffer->surface.c_str());
        sleep(1);
        O_DEBUG("Switching to black");
        image.fill(Qt::black);
        O_DEBUG("Repainting" << buffer->surface.c_str());
        ack = connection->repaint(
            buffer,
            0,
            0,
            geometry.width(),
            geometry.height()
        );
        if(ack.has_value()){
            ack.value()->wait();
        }
        sleep(1);
        O_DEBUG("Resizing" << buffer->surface.c_str());
        geometry.setSize(QSize(300, 300));
        auto resizedImage = image.scaled(geometry.size());
        auto res = connection->resize(
            buffer,
            geometry.width(),
            geometry.height(),
            resizedImage.bytesPerLine(),
            (Blight::data_t)resizedImage.constBits()
        );
        if(res.has_value()){
            buffer = res.value();
        }
        O_DEBUG("Resize done" << buffer->surface.c_str());
        sleep(1);
        O_DEBUG("Validating surface count");
        auto surfaces = connection->surfaces();
        auto buffers = connection->buffers();
        delete connection;
        if(surfaces.size() != 1){
            O_WARNING("There should only be one surface!");
            qApp->exit(1);
            return;
        }
        if(surfaces.front() != buffer->surface){
            O_WARNING("Surface identifier does not match!");
            qApp->exit(1);
            return;
        }
        if(buffers.size() != 1){
            O_WARNING("There should only be one buffer!");
            qApp->exit(1);
            return;
        }
        if(buffers.front()->surface != buffer->surface){
            O_WARNING("Buffer surface identifier does not match!");
            qApp->exit(1);
            return;
        }
        O_DEBUG("Test passes");
        qApp->exit();
    });
    return app.exec();
}
