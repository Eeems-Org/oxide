#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <signal.h>
#include <liboxide.h>
#include <liboxide/eventfilter.h>

#include "controller.h"

#include "screenprovider.h"

#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

using namespace std;
using namespace Oxide;
using namespace Oxide::Sentry;

const char* qt_version = qVersion();

void sigHandler(int signal){
    ::signal(signal, SIG_DFL);
    qApp->quit();
}

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
    sentry_init("corrupt", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("corrupt");
    app.setApplicationVersion(APP_VERSION);
    auto screenProvider = new ScreenProvider(&app);
    Controller controller(&app, screenProvider);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("apps", QVariant::fromValue(controller.getApps()));
    context->setContextProperty("controller", &controller);
    engine.rootContext()->setContextProperty("screenProvider", screenProvider);
    engine.addImageProvider("screen", screenProvider);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    auto root = engine.rootObjects().first();
    root->installEventFilter(new EventFilter(&app));
    controller.setRoot(root);

    signal(SIGINT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGTERM, sigHandler);

    return app.exec();
}
