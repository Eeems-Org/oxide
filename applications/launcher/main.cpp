#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include <QQuickItem>
#include <QObject>
#include <QMap>
#include <QProcess>
#include <QStringList>
#include <QtDBus>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>
#include <liboxide.h>
#include <liboxide/oxideqml.h>
#include <liboxide/eventfilter.h>

#include "controller.h"

using namespace std;
using namespace Oxide;
using namespace Oxide::QML;
using namespace Oxide::Sentry;

function<void(int)> shutdown_handler;
void signalHandler2(int signal) { shutdown_handler(signal); }

int main(int argc, char* argv[]){
    deviceSettings.setupQtEnvironment();
    QGuiApplication app(argc, argv);
    sentry_init("oxide", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("oxide");
    app.setApplicationDisplayName("Launcher");
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    Controller* controller = new Controller();
    qmlRegisterAnonymousType<AppItem>("codes.eeems.oxide", 2);
    qmlRegisterAnonymousType<Controller>("codes.eeems.oxide", 2);
    registerQML(&engine);
    context->setContextProperty("apps", QVariant::fromValue(controller->getApps()));
    context->setContextProperty("controller", controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QObject* root = engine.rootObjects().first();
    controller->root = root;
    root->installEventFilter(new EventFilter(&app));
    QObject* stateController = root->findChild<QObject*>("stateController");
    if(!stateController){
        qDebug() << "Can't find stateController";
        return -1;
    }
    controller->stateController = stateController;
    QObject* clock = root->findChild<QObject*>("clock");
    if(!clock){
        qDebug() << "Can't find clock";
        return -1;
    }
    // Update UI
    clock->setProperty("text", QTime::currentTime().toString("h:mm a"));
    controller->updateUIElements();
    // Setup clock
    QTimer* clockTimer = new QTimer(root);
    auto currentTime = QTime::currentTime();
    QTime nextTime = currentTime.addSecs(60 - currentTime.second());
    clockTimer->setInterval(currentTime.msecsTo(nextTime)); // nearest minute
    QObject::connect(clockTimer , &QTimer::timeout, [clock, &clockTimer, controller](){
        QString text = "";
        if(controller->showDate()){
            text = QDate::currentDate().toString(Qt::TextDate) + " ";
        }
        clock->setProperty("text", text + QTime::currentTime().toString("h:mm a"));
        if(clockTimer->interval() != 60 * 1000){
            clockTimer->setInterval(60 * 1000); // 1 minute
        }
    });
    clockTimer ->start();
    shutdown_handler = [&controller](int signum) {
        Q_UNUSED(signum)
        QTimer::singleShot(300, [=](){
            emit controller->reload();
        });
    };
    signal(SIGCONT, signalHandler2);
    return app.exec();
}
