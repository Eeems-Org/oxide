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

#include "controller.h"
#include "eventfilter.h"
#include "wifimanager.h"

#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif

const char *qt_version = qVersion();

bool exists(const std::string& path) {
    QFile file(path.c_str());
    return file.exists();
}

int main(int argc, char *argv[]){

//    QSettings xochitlSettings("/home/root/.config/remarkable/xochitl.conf", QSettings::IniFormat);
//    xochitlSettings.sync();
//    qDebug() << xochitlSettings.value("Password").toString();

    if (strcmp(qt_version, QT_VERSION_STR) != 0){
        qDebug() << "Version mismatch, Runtime: " << qt_version << ", Build: " << QT_VERSION_STR;
    }
#ifdef __arm__
    // Setup epaper
    qputenv("QMLSCENE_DEVICE", "epaper");
    qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
    qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "rotate=180");
    qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet");
//    qputenv("QT_DEBUG_BACKINGSTORE", "1");
#endif
    QProcess::execute("killall", QStringList() << "button-capture" << "erode");
    auto buttonCaptureProc = new QProcess();
    buttonCaptureProc->setArguments(QStringList() << std::to_string(getpid()).c_str());
    qDebug() << "Looking for button-capture";
    for(auto buttonCapture : QList<QString> { "/opt/bin/button-capture", "/bin/button-capture", "/home/root/.local/button-capture"}){
        qDebug() << "  " << buttonCapture.toStdString().c_str();
        if(exists(buttonCapture.toStdString())){
            qDebug() << "   Found";
            buttonCaptureProc->setProgram(buttonCapture);
            buttonCaptureProc->start();
            buttonCaptureProc->waitForStarted();
            break;
        }
    }
    if(!buttonCaptureProc->processId()){
        qDebug() << "button-capture not found or is running";
    }
    QGuiApplication app(argc, argv);
    EventFilter filter;
    app.installEventFilter(&filter);
    QQmlApplicationEngine engine;
    QQmlContext* context = engine.rootContext();
    Controller* controller = new Controller();
    controller->killXochitl();
    controller->filter = &filter;
    controller->wifiManager = WifiManager::singleton();
    qmlRegisterType<AppItem>();
    qmlRegisterType<Controller>();
    controller->loadSettings();
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
    filter.root = (QQuickItem*)root;
    QObject* stateController = root->findChild<QObject*>("stateController");
    if(!stateController){
        qDebug() << "Can't find stateController";
        return -1;
    }
    QObject* clock = root->findChild<QObject*>("clock");
    if(!clock){
        qDebug() << "Can't find clock";
        return -1;
    }
    // Update UI
    clock->setProperty("text", QTime::currentTime().toString("h:mm a"));
    controller->updateBatteryLevel();
    controller->updateWifiState();
    // Setup suspend timer
    filter.timer = new QTimer(root);
    filter.timer->setInterval(5 * 60 * 1000); // 5 minutes
    QObject::connect(filter.timer, &QTimer::timeout, [stateController, controller](){
        if(!controller->getBatteryCharging()){
            qDebug() << "Suspending due to inactivity...";
            stateController->setProperty("state", QString("suspended"));
        }
    });
    // Setup clock
    QTimer* clockTimer = new QTimer(root);
    auto currentTime = QTime::currentTime();
    QTime nextTime = currentTime.addSecs(60 - currentTime.second());
    clockTimer->setInterval(currentTime.msecsTo(nextTime)); // nearest minute
    QObject::connect(clockTimer , &QTimer::timeout, [clock, &clockTimer](){
        clock->setProperty("text", QTime::currentTime().toString("h:mm a"));
        if(clockTimer->interval() != 60 * 1000){
            clockTimer->setInterval(60 * 1000); // 1 minute
        }
    });
    clockTimer ->start();
    if(controller->automaticSleep()){
        filter.timer->start();
    }
    return app.exec();
}
