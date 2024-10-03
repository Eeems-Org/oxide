#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <liboxide/eventfilter.h>
#include <liboxide/dbus.h>
#include <liboxide/oxideqml.h>

#include "controller.h"

using namespace codes::eeems::oxide1;
using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

#define DEBUG_EVENTS

int main(int argc, char *argv[]){
    deviceSettings.setupQtEnvironment();
    QGuiApplication app(argc, argv);
    sentry_init("decay", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("decay");
    app.setApplicationVersion(APP_VERSION);
    Controller controller(&app);
    QQmlApplicationEngine engine;
    registerQML(&engine);
    QQmlContext* context = engine.rootContext();
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    auto root = engine.rootObjects().first();
    root->installEventFilter(new EventFilter(&app));
    controller.setRoot(root);
    qDebug() << root;
    return app.exec();
}
