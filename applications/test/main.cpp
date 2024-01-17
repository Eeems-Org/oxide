#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <liboxide/eventfilter.h>
#include <liboxide/dbus.h>
#include <liboxide/meta.h>
#include <liboxide/sentry.h>
#include <liboxide/oxideqml.h>

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
    QTimer::singleShot(0, [&app]{
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
        auto fd = qfd.fileDescriptor();
        qDebug() << fd;
        app.exit();
    });
    return app.exec();
}
