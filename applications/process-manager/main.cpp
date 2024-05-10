#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuick>
#include <QtPlugin>

#include <linux/input.h>
#include <signal.h>
#include <ostream>
#include <fcntl.h>
#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/oxideqml.h>

#include "controller.h"

using namespace std;
using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

int main(int argc, char *argv[]){
    deviceSettings.setupQtEnvironment();
    QGuiApplication app(argc, argv);
    sentry_init("erode", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("tarnish");
    app.setApplicationDisplayName("Process Monitor");
    app.setApplicationVersion(APP_VERSION);
    QQmlApplicationEngine engine;
    registerQML(&engine);
    QQmlContext* context = engine.rootContext();
    Controller controller(&engine);
    context->setContextProperty("controller", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QObject* root = engine.rootObjects().first();
    root->installEventFilter(new EventFilter(&app));
    QQuickItem* tasksView = root->findChild<QQuickItem*>("tasksView");
    if(!tasksView){
        qDebug() << "Can't find tasksView";
        return -1;
    }
    return app.exec();
}
