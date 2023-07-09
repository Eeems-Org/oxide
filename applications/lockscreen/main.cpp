#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>

#include <cstdlib>
#include <liboxide/eventfilter.h>
#include <liboxide/dbus.h>
#include <liboxide/tarnish.h>

#include "controller.h"

using namespace codes::eeems::oxide1;
using namespace Oxide;
using namespace Oxide::Sentry;

int main(int argc, char *argv[]){
    deviceSettings.setupQtEnvironment(DeviceSettings::Oxide);
    QGuiApplication app(argc, argv);
    sentry_init("decay", argv);
    auto filter = new EventFilter(&app);
    app.installEventFilter(filter);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("decay");
    app.setApplicationVersion(APP_VERSION);
    Tarnish::registerChild();
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
    QObject::connect(signalHandler, &SignalHandler::sigInt, &app, &QGuiApplication::quit);
    QObject::connect(signalHandler, &SignalHandler::sigTerm, &app, &QGuiApplication::quit);
    return app.exec();
}
