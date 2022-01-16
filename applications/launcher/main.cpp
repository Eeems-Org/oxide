#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtPlugin>
#include <QtQuick>
#include <QQuickItem>
#include <QObject>
#include <QMap>
#include <QSettings>
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
#include <liboxide/eventfilter.h>

#include "controller.h"

#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

using namespace std;
using namespace Oxide;
using namespace Oxide::Sentry;

const char *qt_version = qVersion();

function<void(int)> shutdown_handler;
void signalHandler2(int signal) { shutdown_handler(signal); }

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
    sentry_init("oxide", argv);
    auto filter = new EventFilter(&app);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("oxide");
    app.setApplicationDisplayName("Launcher");
    app.installEventFilter(filter);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    Controller* controller = new Controller();
    controller->filter = filter;
    qmlRegisterAnonymousType<AppItem>("codes.eeems.oxide", 2);
    qmlRegisterAnonymousType<Controller>("codes.eeems.oxide", 2);
    context->setContextProperty("screenGeometry", app.primaryScreen()->geometry());
    context->setContextProperty("apps", QVariant::fromValue(controller->getApps()));
    context->setContextProperty("controller", controller);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()){
        qDebug() << "Nothing to display";
        return -1;
    }
    QObject* root = engine.rootObjects().first();
    controller->root = root;
    filter->root = (QQuickItem*)root;
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
