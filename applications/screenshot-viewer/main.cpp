#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <signal.h>
#include <liboxide.h>

#include "controller.h"

using namespace std;
using namespace Oxide;
using namespace Oxide::Sentry;

void sigHandler(int signal){
    ::signal(signal, SIG_DFL);
    qApp->quit();
}

int main(int argc, char *argv[]){
    deviceSettings.setupQtEnvironment();
    QGuiApplication app(argc, argv);
    sentry_init("anxiety", argv);
    auto filter = new EventFilter(&app);
    app.installEventFilter(filter);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("anxiety");
    app.setApplicationDisplayName("Screenshots");
    app.setApplicationVersion(APP_VERSION);
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

    signal(SIGINT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGTERM, sigHandler);

    return app.exec();
}
