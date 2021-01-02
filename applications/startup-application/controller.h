#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>

#include "signalhandler.h"
#include "dbussettings.h"

#include "dbusservice_interface.h"
#include "systemapi_interface.h"
#include "powerapi_interface.h"

using namespace codes::eeems::oxide1;

enum State { Normal, PowerSaving };
enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };

class Controller : public QObject {
    Q_OBJECT

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
    }
    ~Controller(){}

    Q_INVOKABLE void startup(){
        if(!getUI()){
            QTimer::singleShot(100, this, &Controller::startup);
            return;
        }
        qDebug() << "Running controller startup";
        batteryLevelChanged(powerApi->batteryLevel());
        batteryStateChanged(powerApi->batteryState());
        chargerStateChanged(powerApi->chargerState());
        powerStateChanged(powerApi->state());
        launchOxide();
    }
    Q_INVOKABLE void launchOxide(){
        system("rot --object Application:apps/d3641f0572435f76bb5cc1468d4fe1db apps call launch");
    }

    void setRoot(QObject* root){ this->root = root; }

private slots:
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
        if(!getUI()){
            return;
        }
        ui->setProperty("level", level);
    }
    void batteryStateChanged(int state){
        if(!getUI()){
            return;
        }
        if(state != BatteryNotPresent){
            ui->setProperty("present", true);
        }
        switch(state){
            case ChargerConnected:
                ui->setProperty("connected", true);
            break;
            case ChargerNotConnected:
            case ChargerNotPresent:
                ui->setProperty("connected", false);
            break;
            case ChargerUnknown:
            default:
                ui->setProperty("connected", false);
        }
    }
    void chargerStateChanged(int state){
        if(!getUI()){
            return;
        }
        if(state != BatteryNotPresent){
            ui->setProperty("present", true);
        }
        switch(state){
            case ChargerConnected:
                ui->setProperty("connected", true);
            break;
            case ChargerNotConnected:
            case ChargerNotPresent:
                ui->setProperty("connected", false);
            break;
            case ChargerUnknown:
            default:
                ui->setProperty("connected", false);
        }
    }
    void powerStateChanged(int state){
        Q_UNUSED(state);
        // TODO handle requested battery state
    }
    void batteryAlert(){
        if(!getUI()){
            return;
        }
        ui->setProperty("alert", true);
    }
    void batteryWarning(){
        if(!getUI()){
            return;
        }
        ui->setProperty("warning", true);
    }
    void chargerWarning(){
        // TODO handle charger
    }

private:
    General* api;
    System* systemApi;
    Power* powerApi;
    QObject* root = nullptr;
    QObject* ui = nullptr;

    int tarnishPid() { return api->tarnishPid(); }
    QObject* getUI() {
        ui = root->findChild<QObject*>("batteryLevel");
        return ui;
    }
};

#endif // CONTROLLER_H
