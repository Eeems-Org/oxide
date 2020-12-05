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
#include "settings.h"


#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

using namespace std;

function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

const char *qt_version = qVersion();

int main(int argc, char *argv[]){
    if (strcmp(qt_version, QT_VERSION_STR) != 0){
        qDebug() << "Version mismatch, Runtime: " << qt_version << ", Build: " << QT_VERSION_STR;
    }
#ifdef __arm__
    // Setup epaper
    qputenv("QMLSCENE_DEVICE", "epaper");
    qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
    qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", Settings::instance().getTouchEnvSetting());
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
    if(argc > 1){
        controller.protectPid = std::stoi(argv[1]);
    }
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("tasks", QVariant::fromValue(controller.getTasks()));
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
    shutdown_handler = [&controller](int signal){
        Q_UNUSED(signal)
        emit controller.reload();
    };
    signal(SIGCONT, signal_handler);
    return app.exec();
}
