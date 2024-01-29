#include "canvas.h"

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
#include <libblight/debug.h>

using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

#define DEBUG_EVENTS

int main(int argc, char *argv[]){
    qputenv("QMLSCENE_DEVICE", "software");
    qputenv("QT_QUICK_BACKEND","software");
    qputenv("QT_QPA_PLATFORM", "oxide:enable_fonts");
    QCoreApplication::addLibraryPath("/opt/usr/lib/plugins");
    QGuiApplication app(argc, argv);
    sentry_init("test_blight", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("test_blight");
    app.setApplicationVersion(APP_VERSION);
    if(Oxide::debugEnabled()){
        BLIGHT_DEBUG_LOGGING = 7;
    }
    if(!qEnvironmentVariableIsSet("SKIP_TEST")){
#ifdef EPAPER
        Blight::connect(true);
#else
        Blight::connect(false);
#endif
        if(!Blight::exists()){
            O_WARNING("Service not found!");
            return EXIT_FAILURE;
        }
        {
            auto maybe = Blight::clipboard();
            if(!maybe.has_value()){
                O_WARNING("Could not read clipboard!");
                return EXIT_FAILURE;
            }
            auto clipboard = maybe.value();
            O_DEBUG(clipboard.name.c_str() << QString::fromStdString(clipboard.to_string()));
            O_DEBUG("Updating clipboard");
            if(!clipboard.set("Test")){
                O_WARNING("Could not update clipboard!");
                return EXIT_FAILURE;
            }
            O_DEBUG(clipboard.name.c_str() << QString::fromStdString(clipboard.to_string()));
        }
        auto connection = Blight::connection();
        if(connection == nullptr){
            O_WARNING(std::strerror(errno));
            return EXIT_FAILURE;
        }
        connection->onDisconnect([](int res){
            O_DEBUG("Connection closed:" << res);
            if(res){
                std::exit(res);
            }
        });
        O_DEBUG("Connection socket descriptor:" << connection->handle());
        O_DEBUG("Input socket descriptor:" << connection->input_handle());
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
            return EXIT_FAILURE;
        }
        auto buffer = maybe.value();
        if(buffer->fd == -1 || buffer->data == nullptr){
            O_WARNING("Failed to create buffer:" << strerror(errno));
            return EXIT_FAILURE;
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
        if(!buffer->surface){
            O_WARNING("No identifier provided");
            return EXIT_FAILURE;
        }
        O_DEBUG("Surface added:" << buffer->surface);
        sleep(1);
        O_DEBUG("Switching to gray");
        image.fill(Qt::gray);
        O_DEBUG("Repainting" << buffer->surface);
        {
            auto ack = connection->repaint(
                buffer,
                0,
                0,
                geometry.width(),
                geometry.height()
                );
            if(!ack.has_value()){
                O_WARNING("Failed to repaint");
                return EXIT_FAILURE;
            }
            ack.value()->wait();
        }
        O_DEBUG("Repainting done");
        sleep(1);
        O_DEBUG("Moving " << buffer->surface);
        geometry.setTopLeft(QPoint(100, 100));
        connection->move(buffer, geometry.x(), geometry.y());
        O_DEBUG("Move done" << buffer->surface);
        sleep(1);
        O_DEBUG("Switching to black");
        image.fill(Qt::black);
        O_DEBUG("Repainting" << buffer->surface);
        {
            auto ack = connection->repaint(
                buffer,
                0,
                0,
                geometry.width(),
                geometry.height()
                );
            if(!ack.has_value()){
                O_WARNING("Failed to repaint");
                return EXIT_FAILURE;
            }
            ack.value()->wait();
        }
        O_DEBUG("Repainting done");
        sleep(1);
        O_DEBUG("Resizing" << buffer->surface);
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
        O_DEBUG("Resize done" << buffer->surface);
        sleep(1);
        O_DEBUG("Lowering" << buffer->surface);
        {
            auto ack = connection->lower(buffer);
            if(!ack.has_value()){
                O_WARNING("Could not lower surface!");
                return EXIT_FAILURE;
            }
            ack.value()->wait();
        }
        O_DEBUG("Lowering done" << buffer->surface);
        sleep(1);
        O_DEBUG("Raising" << buffer->surface);
        {
            auto ack = connection->raise(buffer);
            if(!ack.has_value()){
                O_WARNING("Could not raise surface!");
                return EXIT_FAILURE;
            }
            ack.value()->wait();
        }
        O_DEBUG("Raising done" << buffer->surface);
        sleep(1);
        O_DEBUG("Validating surface count");
        auto surfaces = connection->surfaces();
        if(surfaces.size() != 1){
            O_WARNING("There should only be one surface!" << surfaces.size());
            return EXIT_FAILURE;
        }
        if(surfaces.front() != buffer->surface){
            O_WARNING("Surface identifier does not match!" << surfaces.front());
            return EXIT_FAILURE;
        }
        auto buffers = connection->buffers();
        if(buffers.size() != 1){
            O_WARNING("There should only be one buffer!" << buffers.size());
            return EXIT_FAILURE;
        }
        if(buffers.front()->surface != buffer->surface){
            O_WARNING("Buffer surface identifier does not match!" << buffers.front()->surface);
            return EXIT_FAILURE;
        }
        O_DEBUG("Removing surface");
        {
            auto ack = connection->remove(buffer);
            if(!ack.has_value()){
                O_WARNING("Failed to remove buffer:" << std::strerror(errno));
                return EXIT_FAILURE;
            }
            ack.value()->wait();
        }
        O_DEBUG("Surface removed");
        if(!connection->surfaces().empty()){
            O_WARNING("There are still surfaces even though it was removed!");
            return EXIT_FAILURE;
        }
        O_DEBUG("Test passes");
    }
    QQmlApplicationEngine engine;
    registerQML(&engine);
    qmlRegisterType<Canvas>("codes.eeems.test", 1, 0, "Canvas");
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        O_WARNING("Nothing to display");
        return EXIT_FAILURE;
    }
    return app.exec();
}
