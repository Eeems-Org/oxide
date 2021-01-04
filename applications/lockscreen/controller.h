#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QQuickItem>

#include "dbussettings.h"

#include "dbusservice_interface.h"
#include "systemapi_interface.h"
#include "powerapi_interface.h"
#include "wifiapi_interface.h"
#include "appsapi_interface.h"
#include "application_interface.h"

using namespace codes::eeems::oxide1;

#define DECAY_SETTINGS_VERSION 1

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
    : QObject(parent), confirmPin(), settings(this) {
        clockTimer = new QTimer(root);
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

        qDebug() << "Requesting system API...";
        QDBusObjectPath path = api->requestAPI("system");
        if(path.path() == "/"){
            qDebug() << "Unable to get system API";
            throw "";
        }
        systemApi = new System(OXIDE_SERVICE, path.path(), bus, this);

        connect(systemApi, &System::sleepInhibitedChanged, this, &Controller::sleepInhibitedChanged);
        connect(systemApi, &System::powerOffInhibitedChanged, this, &Controller::powerOffInhibitedChanged);
        connect(systemApi, &System::deviceSuspending, this, &Controller::deviceSuspending);

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

        qDebug() << "Requesting apps API...";
        path = api->requestAPI("apps");
        if(path.path() == "/"){
            qDebug() << "Unable to get apps API";
            throw "";
        }
        appsApi = new Apps(OXIDE_SERVICE, path.path(), bus, this);

        settings.sync();
        auto version = settings.value("version", 0).toInt();
        if(version < DECAY_SETTINGS_VERSION){
            migrate(&settings, version);
        }
    }
    ~Controller(){
        if(clockTimer->isActive()){
            clockTimer->stop();
        }
    }

    Q_INVOKABLE void startup(){
        if(!getBatteryUI() || !getWifiUI() || !getClockUI() || !getStateControllerUI()){
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

        clockUI->setProperty("text", QTime::currentTime().toString("h:mm a"));

        auto currentTime = QTime::currentTime();
        QTime nextTime = currentTime.addSecs(60 - currentTime.second());
        clockTimer->setInterval(currentTime.msecsTo(nextTime)); // nearest minute
        QObject::connect(clockTimer , &QTimer::timeout, this, &Controller::updateClock);
        clockTimer ->start();

        if(!settings.contains("pin")){
            QTimer::singleShot(100, [this]{
                stateControllerUI->setProperty("state", xochitlPin().isEmpty() ? "firstLaunch" : "import");
            });
            return;
        }

        if(!storedPin().length()){
            launchStartupApp();
            return;
        }

        QTimer::singleShot(100, [this]{
            stateControllerUI->setProperty("state", "loaded");
        });
    }
    Q_INVOKABLE void launchStartupApp(){
        QDBusObjectPath path = appsApi->startupApplication();
        if(path.path() == "/"){
            path = appsApi->getApplicationPath("codes.eeems.oxide");
        }
        if(path.path() == "/"){
            return;
        }
        Application app(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus());
        app.launch();
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
    Q_INVOKABLE bool submitPin(QString pin){
        if(pin.length() != 4){
            return false;
        }
        qDebug() << "Checking PIN";
        auto state = this->state();
        if(state == "loaded" && pin == storedPin()){
            qDebug() << "PIN matches!";
            QTimer::singleShot(200, [this]{
                qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
                launchStartupApp();
            });
            return true;

        }else if(state == "loaded"){
            qDebug() << "PIN doesn't match!";
            return false;
        }
        if(state == "prompt"){
            confirmPin = pin;
            setState("confirmPin");
            return true;
        }
        if(state != "confirmPin"){
            return false;
        }
        if(pin != confirmPin){
            qDebug() << "PIN doesn't match!";
            setState("prompt");
            return false;
        }
        qDebug() << "PIN matches!";
        setStoredPin(pin);
        QTimer::singleShot(200, [this]{
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents, 100);
            setState("loaded");
            launchStartupApp();
        });
        return true;
    }
    Q_INVOKABLE void importPin(){
        setStoredPin(xochitlPin());
        if(!storedPin().isEmpty()){
            removeXochitlPin();
        }
        setState("loaded");
    }
    Q_INVOKABLE void clearPin(){
        setStoredPin("");
        setState("loading");
    }

    bool sleepInhibited(){ return systemApi->sleepInhibited(); }
    bool powerOffInhibited(){ return systemApi->powerOffInhibited(); }
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
    QString storedPin() { return settings.value("pin", "").toString(); }
    void setStoredPin(QString pin) {
        settings.setValue("pin", pin);
    }

    void setRoot(QObject* root){ this->root = root; }

signals:
    void sleepInhibitedChanged(bool);
    void powerOffInhibitedChanged(bool);

private slots:
    void deviceSuspending(){
        if(getPinEntryUI()){
            pinEntryUI->setProperty("message", "");
            pinEntryUI->setProperty("pin", "");
        }
        if(state() == "confirmPin"){
            confirmPin = "";
            setState("firstLaunch");
        }else{
            setState("loading");
        }
    }
    void updateClock(){
        if(!getClockUI()){
            return;
        }
        clockUI->setProperty("text", QTime::currentTime().toString("h:mm a"));
        if(clockTimer->interval() != 60 * 1000){
            clockTimer->setInterval(60 * 1000); // 1 minute
        }
    }
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
    QString confirmPin;
    QSettings settings;
    General* api;
    System* systemApi;
    Power* powerApi;
    Wifi* wifiApi;
    Apps* appsApi;
    QTimer* clockTimer = nullptr;
    QObject* root = nullptr;
    QObject* batteryUI = nullptr;
    QObject* wifiUI = nullptr;
    QObject* clockUI = nullptr;
    QObject* stateControllerUI = nullptr;
    QObject* pinEntryUI = nullptr;

    int tarnishPid() { return api->tarnishPid(); }
    QObject* getBatteryUI() {
        batteryUI = root->findChild<QObject*>("batteryLevel");
        return batteryUI;
    }
    QObject* getWifiUI() {
        wifiUI = root->findChild<QObject*>("wifiState");
        return wifiUI;
    }
    QObject* getClockUI() {
        clockUI = root->findChild<QObject*>("clock");
        return clockUI;
    }
    QObject* getStateControllerUI(){
        stateControllerUI = root->findChild<QObject*>("stateController");
        return stateControllerUI;
    }
    QObject* getPinEntryUI(){
        pinEntryUI = root->findChild<QObject*>("pinEntry");
        return pinEntryUI;
    }

    static void migrate(QSettings* settings, int fromVersion){
        if(fromVersion != 0){
            throw "Unknown settings version";
        }
        // In the future migrate changes to settings between versions
        settings->setValue("version", DECAY_SETTINGS_VERSION);
        settings->sync();
    }

    static QString xochitlPin(){
        QSettings xochitlSettings("/home/root/.config/remarkable/xochitl.conf", QSettings::IniFormat);
        xochitlSettings.sync();
        return xochitlSettings.value("Passcode", "").toString();
    }
    static void removeXochitlPin(){
        QSettings xochitlSettings("/home/root/.config/remarkable/xochitl.conf", QSettings::IniFormat);
        xochitlSettings.sync();
        xochitlSettings.remove("Passcode");
        xochitlSettings.sync();
    }
};

#endif // CONTROLLER_H
