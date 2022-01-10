#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>

#include "controller.h"
#include "eventfilter.h"

#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

#include "dbusservice_interface.h"

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;

const char* qt_version = qVersion();

int main(int argc, char *argv[]){
    if (strcmp(qt_version, QT_VERSION_STR) != 0){
        qDebug() << "Version mismatch, Runtime: " << qt_version << ", Build: " << QT_VERSION_STR;
    }
#ifdef __arm__
    // Setup epaper
    qputenv("QMLSCENE_DEVICE", "epaper");
    qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
    qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", deviceSettings.getTouchEnvSetting());
    qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet");
//    qputenv("QT_DEBUG_BACKINGSTORE", "1");
#endif
    QGuiApplication app(argc, argv);
    sentry_init("decay", argv);
    auto filter = new EventFilter(&app);
    app.installEventFilter(filter);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("decay");
    app.setApplicationVersion(OXIDE_INTERFACE_VERSION);
    Controller controller(&app);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    auto root = engine.rootObjects().first();
    filter->root = (QQuickItem*)root;
    controller.setRoot(root);
    return app.exec();
}
