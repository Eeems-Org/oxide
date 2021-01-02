#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>

#include "signalhandler.h"
#include "dbussettings.h"

#include "dbusservice_interface.h"
#include "systemapi_interface.h"
#include "powerapi_interface.h"
#include "wifiapi_interface.h"

using namespace codes::eeems::oxide1;

enum State { Normal, PowerSaving };
enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };
enum WifiState { WifiUnknown, WifiOff, WifiDisconnected, WifiOffline, WifiOnline};

class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool powerOffInhibited READ powerOffInhibited NOTIFY powerOffInhibitedChanged)
    Q_PROPERTY(bool sleepInhibited READ sleepInhibited NOTIFY sleepInhibitedChanged)
public:
    Controller(QObject* parent)
    : QObject(parent) {
        SignalHandler::setup_unix_signal_handlers();
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

        connect(signalHandler, &SignalHandler::sigUsr1, this, &Controller::sentToForeground);
        connect(signalHandler, &SignalHandler::sigUsr2, this, &Controller::sentToBackground);

        qDebug() << "Requesting system API...";
        QDBusObjectPath path = api->requestAPI("system");
        if(path.path() == "/"){
            qDebug() << "Unable to get system API";
            throw "";
        }
        systemApi = new System(OXIDE_SERVICE, path.path(), bus, this);

        connect(systemApi, &System::sleepInhibitedChanged, this, &Controller::sleepInhibitedChanged);
        connect(systemApi, &System::powerOffInhibitedChanged, this, &Controller::powerOffInhibitedChanged);

        qDebug() << "Requesting power API...";
        path = api->requestAPI("power");
        if(path.path() == "/"){
            qDebug() << "Unable to get power API";
            throw "";
        }
        powerApi = new Power(OXIDE_SERVICE, path.path(), bus, this);

        connect(powerApi, &Power::batteryLevelChanged, this, &Controller::batteryLevelChanged);
        connect(powerApi, &Power::batteryStateChanged, this, &Controller::batteryStateChanged);
        connect(powerApi, &Power::chargerStateChanged, this, &Controller::chargerStateChanged);
        connect(powerApi, &Power::stateChanged, this, &Controller::powerStateChanged);
        connect(powerApi, &Power::batteryAlert, this, &Controller::batteryAlert);
        connect(powerApi, &Power::batteryWarning, this, &Controller::batteryWarning);
        connect(powerApi, &Power::chargerWarning, this, &Controller::chargerWarning);

        qDebug() << "Requesting wifi API...";
        path = api->requestAPI("wifi");
        if(path.path() == "/"){
            qDebug() << "Unable to get wifi API";
            throw "";
        }
        wifiApi = new Wifi(OXIDE_SERVICE, path.path(), bus, this);

        connect(wifiApi, &Wifi::disconnected, this, &Controller::disconnected);
        connect(wifiApi, &Wifi::networkConnected, this, &Controller::networkConnected);
        connect(wifiApi, &Wifi::stateChanged, this, &Controller::wifiStateChanged);
        connect(wifiApi, &Wifi::linkChanged, this, &Controller::wifiLinkChanged);
    }
    ~Controller(){}

    Q_INVOKABLE void startup(){
        if(!getBatteryUI()){
            QTimer::singleShot(100, this, &Controller::startup);
            return;
        }
        qDebug() << "Running controller startup";
        batteryLevelChanged(powerApi->batteryLevel());
        batteryStateChanged(powerApi->batteryState());
        chargerStateChanged(powerApi->chargerState());
        powerStateChanged(powerApi->state());
        wifiStateChanged(wifiApi->state());
        wifiLinkChanged(wifiApi->link());
        //launchOxide();
    }
    Q_INVOKABLE void launchOxide(){
        system("rot --object Application:apps/d3641f0572435f76bb5cc1468d4fe1db apps call launch");
    }
    Q_INVOKABLE void suspend(){
        if(!sleepInhibited()){
            systemApi->suspend().waitForFinished();
        }
    }
    Q_INVOKABLE void poweroff(){
        if(!powerOffInhibited()){
            systemApi->powerOff().waitForFinished();
        }
    }

    bool sleepInhibited(){ return systemApi->sleepInhibited(); }
    bool powerOffInhibited(){ return systemApi->powerOffInhibited(); }

    void setRoot(QObject* root){ this->root = root; }

signals:
    void sleepInhibitedChanged(bool);
    void powerOffInhibitedChanged(bool);
private slots:
    void disconnected(){
        wifiStateChanged(wifiApi->state());
    }
    void networkConnected(){
        wifiStateChanged(wifiApi->state());
    }
    void wifiStateChanged(int state){
        if(!getWifiUI()){
            return;
        }
        switch(state){
            case WifiOff:
                wifiUI->setProperty("state", "down");
            break;
            case WifiDisconnected:
                wifiUI->setProperty("state", "up");
                wifiUI->setProperty("connected", false);
            break;
            case WifiOffline:
                wifiUI->setProperty("state", "up");
                wifiUI->setProperty("connected", true);
            break;
            case WifiOnline:
                wifiUI->setProperty("state", "up");
                wifiUI->setProperty("connected", true);
                wifiUI->setProperty("link", wifiApi->link());
            break;
            case WifiUnknown:
            default:
                wifiUI->setProperty("state", "unkown");
        }
    }
    void wifiLinkChanged(int link){
        if(!getWifiUI()){
            return;
        }
        if(wifiApi->state() != WifiOnline){
            link = 0;
        }
        wifiUI->setProperty("link", link);
    }

    void sentToForeground(){
        qDebug() << "Got foreground signal";
        // TODO only show if not passing off to oxide
        root->setProperty("visible", true);
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
        qDebug() << "Acking SIGUSR1 to " << tarnishPid();
        kill(tarnishPid(), SIGUSR1);
        QTimer::singleShot(1000, [=]{
            launchOxide();
        });
    }
    void sentToBackground(){
        qDebug() << "Got background signal";
        if(root->property("visible").toBool()){
            root->setProperty("visible", false);
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
        }
        qDebug() << "Acking SIGUSR2 to " << tarnishPid();
        kill(tarnishPid(), SIGUSR2);
    }

    void batteryLevelChanged(int level){
        if(!getBatteryUI()){
            return;
        }
        batteryUI->setProperty("level", level);
    }
    void batteryStateChanged(int state){
        if(!getBatteryUI()){
            return;
        }
        if(state != BatteryNotPresent){
            batteryUI->setProperty("present", true);
        }
        switch(state){
            case ChargerConnected:
                batteryUI->setProperty("connected", true);
            break;
            case ChargerNotConnected:
            case ChargerNotPresent:
                batteryUI->setProperty("connected", false);
            break;
            case ChargerUnknown:
            default:
                batteryUI->setProperty("connected", false);
        }
    }
    void chargerStateChanged(int state){
        if(!getBatteryUI()){
            return;
        }
        if(state != BatteryNotPresent){
            batteryUI->setProperty("present", true);
        }
        switch(state){
            case ChargerConnected:
                batteryUI->setProperty("connected", true);
            break;
            case ChargerNotConnected:
            case ChargerNotPresent:
                batteryUI->setProperty("connected", false);
            break;
            case ChargerUnknown:
            default:
                batteryUI->setProperty("connected", false);
        }
    }
    void powerStateChanged(int state){
        Q_UNUSED(state);
        // TODO handle requested battery state
    }
    void batteryAlert(){
        if(!getBatteryUI()){
            return;
        }
        batteryUI->setProperty("alert", true);
    }
    void batteryWarning(){
        if(!getBatteryUI()){
            return;
        }
        batteryUI->setProperty("warning", true);
    }
    void chargerWarning(){
        // TODO handle charger
    }

private:
    General* api;
    System* systemApi;
    Power* powerApi;
    Wifi* wifiApi;
    QObject* root = nullptr;
    QObject* batteryUI = nullptr;
    QObject* wifiUI = nullptr;

    int tarnishPid() { return api->tarnishPid(); }
    QObject* getBatteryUI() {
        batteryUI = root->findChild<QObject*>("batteryLevel");
        return batteryUI;
    }
    QObject* getWifiUI() {
        wifiUI = root->findChild<QObject*>("wifi");
        return wifiUI;
    }
};

#endif // CONTROLLER_H
