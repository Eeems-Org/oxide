#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <signal.h>
#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>

#include "controller.h"

#include "screenprovider.h"

using namespace std;
using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

void sigHandler(int signal){
    ::signal(signal, SIG_DFL);
    qApp->quit();
}

int main(int argc, char *argv[]){
    deviceSettings.setupQtEnvironment();
    QGuiApplication app(argc, argv);
    sentry_init("corrupt", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("corrupt");
    app.setApplicationVersion(APP_VERSION);
    auto screenProvider = new ScreenProvider(&app);
    Controller controller(&app, screenProvider);
    QQmlApplicationEngine engine;
    registerQML(&engine);
    QQmlContext* context = engine.rootContext();
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
