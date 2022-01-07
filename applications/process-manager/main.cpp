#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>
#include <QtPlugin>

#include <linux/input.h>
#include <signal.h>
#include <ostream>
#include <fcntl.h>

#include "controller.h"
#include "eventfilter.h"
#include "dbussettings.h"
#include "devicesettings.h"


#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

using namespace std;

const char *qt_version = qVersion();

int main(int argc, char *argv[]){
    initSentry("erode", argv);
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
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("tarnish");
    app.setApplicationDisplayName("Process Monitor");
    app.setApplicationVersion(OXIDE_INTERFACE_VERSION);
    EventFilter filter;
    app.installEventFilter(&filter);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    Controller controller(&engine);
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QObject* root = engine.rootObjects().first();
    filter.root = (QQuickItem*)root;
    QQuickItem* tasksView = root->findChild<QQuickItem*>("tasksView");
    if(!tasksView){
        qDebug() << "Can't find tasksView";
        return -1;
    }
    return app.exec();
}
