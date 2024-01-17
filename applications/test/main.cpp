#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <liboxide/eventfilter.h>
#include <liboxide/dbus.h>
#include <liboxide/meta.h>
#include <liboxide/debug.h>
#include <liboxide/sentry.h>
#include <liboxide/oxideqml.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <errno.h>

using namespace codes::eeems::blight1;
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
        auto bus = QDBusConnection::systemBus();
#else
        auto bus = QDBusConnection::sessionBus();
#endif
        if(!bus.interface()->isServiceRegistered(BLIGHT_SERVICE)){
            qFatal(BLIGHT_SERVICE " can't be found");
        }
        Compositor compositor(BLIGHT_SERVICE, "/", bus);
        auto res = compositor.open();
        if(res.isError()){
            qFatal("Failed to get connection: %s", res.error().message().toStdString().c_str());
        }
        auto qfd = res.value();
        if(!qfd.isValid()){
            qFatal("Failed to get connection: Invalid file descriptor");
        }
        {
            auto fd = qfd.fileDescriptor();
            qDebug() << fd;
        }

        QRect geometry(50, 50, 100, 100);
        auto identifier = QUuid::createUuid().toString();

        int fd = memfd_create(identifier.toStdString().c_str(), MFD_ALLOW_SEALING);
        if(fd == -1){
            O_WARNING("Unable to create memfd for framebuffer:" << strerror(errno));
            return;
        }
        QImage blankImage(geometry.size(), QImage::Format_ARGB32_Premultiplied);
        blankImage.fill(Qt::black);
        size_t size = blankImage.sizeInBytes();
        if(ftruncate(fd, size)){
            O_WARNING("Unable to truncate memfd for framebuffer:" << strerror(errno));
            if(::close(fd) == -1){
                O_WARNING("Failed to close fd:" << strerror(errno));
            }
            return;
        }
        int flags = fcntl(fd, F_GET_SEALS);
        if(fcntl(fd, F_ADD_SEALS, flags | F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW)){
            O_WARNING("Unable to seal memfd for framebuffer:" << strerror(errno));
            if(::close(fd) == -1){
                O_WARNING("Failed to close fd:" << strerror(errno));
            }
            return;
        }
        QFile file;
        if(!file.open(fd, QFile::ReadWrite, QFile::AutoCloseHandle)){
            O_WARNING("Failed to open QFile:" << file.errorString());
            if(::close(fd) == -1){
                O_WARNING("Failed to close fd:" << strerror(errno));
            }
            return;
        }
        auto data = file.map(0, size);
        if(data == nullptr){
            O_WARNING("Failed to map framebuffer:" << strerror(errno));
            file.close();
            return;
        }
        memcpy(data, blankImage.constBits(), size);
        auto bytesPerLine = blankImage.bytesPerLine();
        qDebug() << bytesPerLine;
        auto res2 = compositor.addSurface(
            QDBusUnixFileDescriptor(fd),
            geometry.x(),
            geometry.y(),
            geometry.width(),
            geometry.height(),
            blankImage.bytesPerLine(),
            (int)blankImage.format()
        );
        if(res2.isError()){
            qFatal("Failed to add surface: %s", res2.error().message().toStdString().c_str());
        }
        auto id = res2.value();
        O_DEBUG("Surface added: " << id);
        QTimer::singleShot(10000, []{ qApp->exit(); });
    });
    return app.exec();
}
