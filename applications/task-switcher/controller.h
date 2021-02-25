#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QImage>
#include <QQuickItem>

#include <epframebuffer.h>

#include "dbussettings.h"

#include "dbusservice_interface.h"
#include "screenapi_interface.h"
#include "appsapi_interface.h"
#include "application_interface.h"

#include "screenprovider.h"
#include "signalhandler.h"

using namespace codes::eeems::oxide1;

#define CORRUPT_SETTINGS_VERSION 1

enum State { Normal, PowerSaving };
enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };
enum WifiState { WifiUnknown, WifiOff, WifiDisconnected, WifiOffline, WifiOnline};

class Controller : public QObject {
    Q_OBJECT
public:
    Controller(QObject* parent, ScreenProvider* screenProvider)
    : QObject(parent), confirmPin(), settings(this) {
        this->screenProvider = screenProvider;
        auto bus = QDBusConnection::systemBus();
        qDebug() << "Waiting for tarnish to start up...";
        while(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            struct timespec args{
                .tv_sec = 1,
                .tv_nsec = 0,
            }, res;
            nanosleep(&args, &res);
        }
        api = new General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus, this);

        SignalHandler::setup_unix_signal_handlers();
        connect(signalHandler, &SignalHandler::sigUsr1, this, &Controller::sigUsr1);
        connect(signalHandler, &SignalHandler::sigUsr2, this, &Controller::sigUsr2);

        qDebug() << "Requesting screen API...";
        QDBusObjectPath path = api->requestAPI("screen");
        if(path.path() == "/"){
            qDebug() << "Unable to get screen API";
            throw "";
        }
        screenApi = new Screen(OXIDE_SERVICE, path.path(), bus, this);

        qDebug() << "Requesting apps API...";
        path = api->requestAPI("apps");
        if(path.path() == "/"){
            qDebug() << "Unable to get apps API";
            throw "";
        }
        appsApi = new Apps(OXIDE_SERVICE, path.path(), bus, this);

        settings.sync();
        auto version = settings.value("version", 0).toInt();
        if(version < CORRUPT_SETTINGS_VERSION){
            migrate(&settings, version);
        }
        saveScreen();
    }
    ~Controller(){}

    Q_INVOKABLE void startup(){
        qDebug() << "Running controller startup";
        QTimer::singleShot(10, [this]{
            setState("loaded");
        });
    }
    QString state() {
        if(!getStateControllerUI()){
            return "loading";
        }
        return stateControllerUI->property("state").toString();
    }
    void setState(QString state){
        if(!getStateControllerUI()){
            throw "Unable to find state controller";
        }
        stateControllerUI->setProperty("state", state);
    }
    void saveScreen(){
        qDebug() << "Saving screen for background...";
        screenProvider->updateImage(EPFrameBuffer::framebuffer());
    }

    void setRoot(QObject* root){ this->root = root; }

private slots:
    void sigUsr1(){
        ::kill(tarnishPid(), SIGUSR1);
        qDebug() << "Sent to the foreground...";
        saveScreen();
        setState("loading");
    }
    void sigUsr2(){
        qDebug() << "Sent to the background...";
        setState("hidden");
        QTimer::singleShot(10, [this]{
            ::kill(tarnishPid(), SIGUSR2);
        });
    }

private:
    QString confirmPin;
    QSettings settings;
    General* api;
    Screen* screenApi;
    Apps* appsApi;
    QObject* root = nullptr;
    QObject* stateControllerUI = nullptr;
    QObject* backgroundUI = nullptr;
    ScreenProvider* screenProvider;

    int tarnishPid() { return api->tarnishPid(); }
    QObject* getStateControllerUI(){
        stateControllerUI = root->findChild<QObject*>("stateController");
        return stateControllerUI;
    }
    QObject* getBackgroundUI(){
        backgroundUI = root->findChild<QObject*>("background");
        return backgroundUI;
    }

    static void migrate(QSettings* settings, int fromVersion){
        if(fromVersion != 0){
            throw "Unknown settings version";
        }
        // In the future migrate changes to settings between versions
        settings->setValue("version", CORRUPT_SETTINGS_VERSION);
        settings->sync();
    }
};

#endif // CONTROLLER_H
