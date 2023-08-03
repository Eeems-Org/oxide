#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <signal.h>
#include <liboxide.h>
#include <liboxide/tarnish.h>

#include "controller.h"

using namespace std;
using namespace Oxide;
using namespace Oxide::Sentry;

void sigHandler(int signal){
    ::signal(signal, SIG_DFL);
    qApp->quit();
}

int main(int argc, char *argv[]){
    deviceSettings.setupQtEnvironment(DeviceSettings::Oxide);
    QGuiApplication app(argc, argv);
    app.setAttribute(Qt::AA_SynthesizeMouseForUnhandledTabletEvents);
    sentry_init("corrupt", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("corrupt");
    app.setApplicationVersion(APP_VERSION);
    Tarnish::getSocketFd();
    auto window = Tarnish::topWindow();
    if(window == nullptr){
        qDebug() << "No window";
        return EXIT_FAILURE;
    }
    auto geometry = app.primaryScreen()->geometry();
    geometry.setY(geometry.height() - 150);
    geometry.setHeight(150);
    window->setGeometry(geometry);
    Controller controller(&app);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    context->setContextProperty("screenGeometry", geometry);
    context->setContextProperty("apps", QVariant::fromValue(controller.getApps()));
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    auto root = engine.rootObjects().first();
    controller.setRoot(root);

    signal(SIGINT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGTERM, sigHandler);

    return app.exec();
}
