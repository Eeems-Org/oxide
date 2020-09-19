#pragma once

#include <QObject>
#include <QTimer>
#include <QJsonObject>

#include "appitem.h"
#include "eventfilter.h"
#include "sysobject.h"
#include "inputmanager.h"
#include "dbusservice_interface.h"
#include "powerapi_interface.h"
#include "wifiapi_interface.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

using namespace codes::eeems::oxide1;

enum State { Normal, PowerSaving };
enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };
enum WifiState { WifiUnknown, WifiOff, WifiDisconnected, WifiOffline, WifiOnline};

class Controller : public QObject
{
    Q_OBJECT
public:
    static std::string exec(const char* cmd);
    EventFilter* filter;
    QObject* stateController;
    QObject* root = nullptr;
    explicit Controller(QObject* parent = 0) : QObject(parent), wifi("/sys/class/net/wlan0"), inputManager(), applications(){
        uiTimer = new QTimer(this);
        uiTimer->setSingleShot(false);
        uiTimer->setInterval(3 * 1000); // 3 seconds
        connect(uiTimer, &QTimer::timeout, this, QOverload<>::of(&Controller::updateUIElements));
        uiTimer->start();
    }
    Q_INVOKABLE bool turnOnWifi(){ return wifiApi->enable(); };
    Q_INVOKABLE void turnOffWifi(){ wifiApi->disable(); };
    Q_INVOKABLE bool wifiOn(){
        auto state = wifiApi->state();
        return state != WifiUnknown && state != WifiOff;
    };
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE QList<QObject*> getApps();
    Q_INVOKABLE void importDraftApps();
    Q_INVOKABLE void powerOff();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void killXochitl();
    void updateBatteryLevel();
    Q_INVOKABLE void resetInactiveTimer();
    Q_PROPERTY(bool automaticSleep MEMBER m_automaticSleep WRITE setAutomaticSleep NOTIFY automaticSleepChanged);
    bool automaticSleep() const {
        return m_automaticSleep;
    };
    void setAutomaticSleep(bool);
    Q_PROPERTY(int columns MEMBER m_columns WRITE setColumns NOTIFY columnsChanged);
    int columns() const {
        return m_columns;
    };
    void setColumns(int);
    Q_PROPERTY(int fontSize MEMBER m_fontSize WRITE setFontSize NOTIFY fontSizeChanged);
    int fontSize() const {
        return m_fontSize;
    };
    void setFontSize(int);
    Q_PROPERTY(bool showWifiDb MEMBER m_showWifiDb WRITE setShowWifiDb NOTIFY showWifiDbChanged);
    bool showWifiDb() const {
        return m_showWifiDb;
    };
    void setShowWifiDb(bool);
    Q_PROPERTY(bool showBatteryPercent MEMBER m_showBatteryPercent WRITE setShowBatteryPercent NOTIFY showBatteryPercentChanged);
    bool showBatteryPercent() const {
        return m_showBatteryPercent;
    };
    void setShowBatteryPercent(bool);
    Q_PROPERTY(bool showBatteryTemperature MEMBER m_showBatteryTemperature WRITE setShowBatteryTemperature NOTIFY showBatteryTemperatureChanged);
    bool showBatteryTemperature() const {
        return m_showBatteryTemperature;
    };
    void setShowBatteryTemperature(bool);
    Q_PROPERTY(int sleepAfter MEMBER m_sleepAfter WRITE setSleepAfter NOTIFY sleepAfterChanged);
    int sleepAfter() const {
        return m_sleepAfter;
    };
    void setSleepAfter(int);
    bool getPowerConnected(){ return m_powerConnected; }
signals:
    void reload();
    void automaticSleepChanged(bool);
    void columnsChanged(int);
    void fontSizeChanged(int);
    void showWifiDbChanged(bool);
    void showBatteryPercentChanged(bool);
    void showBatteryTemperatureChanged(bool);
    void sleepAfterChanged(int);

public slots:
    void updateUIElements();
    void reconnectToAPI(){
        auto bus = QDBusConnection::systemBus();
        qDebug() << "Waiting for tarnish to start up";
        while(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            usleep(1000);
        }
        qDebug() << "Requesting APIs";
        General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
        auto reply = api.requestAPI("power");
        reply.waitForFinished();
        if(reply.isError()){
            qDebug() << reply.error();
            qFatal("Could not request power API");
        }
        auto path = ((QDBusObjectPath)reply).path();
        if(path == "/"){
            qDebug() << "API not available";
            qFatal("Power API was not available");
        }
        if(powerApi != nullptr){
            delete powerApi;
        }
        powerApi = new Power(OXIDE_SERVICE, path, bus);
        // Connect to signals
        connect(powerApi, &Power::batteryLevelChanged, this, &Controller::batteryLevelChanged);
        connect(powerApi, &Power::batteryStateChanged, this, &Controller::batteryStateChanged);
        connect(powerApi, &Power::batteryTemperatureChanged, this, &Controller::batteryTemperatureChanged);
        connect(powerApi, &Power::chargerStateChanged, this, &Controller::chargerStateChanged);
        connect(powerApi, &Power::stateChanged, this, &Controller::powerStateChanged);
        connect(powerApi, &Power::batteryAlert, this, &Controller::batteryAlert);
        connect(powerApi, &Power::batteryWarning, this, &Controller::batteryWarning);
        connect(powerApi, &Power::chargerWarning, this, &Controller::chargerWarning);
        reply = api.requestAPI("wifi");
        reply.waitForFinished();
        if(reply.isError()){
            qDebug() << reply.error();
            qFatal("Could not request power API");
        }
        path = ((QDBusObjectPath)reply).path();
        if(path == "/"){
            qDebug() << "API not available";
            qFatal("Wifi API was not available");
        }
        if(wifiApi != nullptr){
            delete wifiApi;
        }
        wifiApi = new Wifi(OXIDE_SERVICE, path, bus);
        connect(wifiApi, &Wifi::bssFound, this, &Controller::bssFound);
        connect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
        connect(wifiApi, &Wifi::disconnected, this, &Controller::disconnected);
        connect(wifiApi, &Wifi::networkAdded, this, &Controller::networkAdded);
        connect(wifiApi, &Wifi::networkConnected, this, &Controller::networkConnected);
        connect(wifiApi, &Wifi::networkRemoved, this, &Controller::networkRemoved);
        connect(wifiApi, &Wifi::stateChanged, this, &Controller::wifiStateChanged);
        connect(wifiApi, &Wifi::linkChanged, this, &Controller::wifiLinkChanged);

        QTimer::singleShot(1000, [=](){
            // Get initial values when UI is ready
            batteryLevelChanged(powerApi->batteryLevel());
            batteryStateChanged(powerApi->batteryState());
            batteryTemperatureChanged(powerApi->batteryTemperature());
            chargerStateChanged(powerApi->chargerState());
            powerStateChanged(powerApi->state());

            wifiStateChanged(wifiApi->state());
            wifiLinkChanged(wifiApi->link());
            // for(auto network : wifiApi->networks()){
            //     networkAdded(network);
            // }
            // networkConnected(wifiApi->network());
        });
    }
private slots:
    void batteryAlert(){
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            ui->setProperty("alert", true);
        }
    }
    void batteryLevelChanged(int level){
        qDebug() << "Battery level: " << level;
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            ui->setProperty("level", level);
        }
    }
    void batteryStateChanged(int state){
        switch(state){
            case BatteryCharging:
                qDebug() << "Battery state: Charging";
            break;
            case BatteryNotPresent:
                qDebug() << "Battery state: Not Present";
            break;
            case BatteryDischarging:
                qDebug() << "Battery state: Discharging";
            break;
            case BatteryUnknown:
            default:
                qDebug() << "Battery state: Unknown";
        }
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            if(state != BatteryNotPresent){
                ui->setProperty("present", true);
            }
            switch(state){
                case BatteryCharging:
                    ui->setProperty("charging", true);
                    m_powerConnected = true;
                break;
                case BatteryNotPresent:
                    ui->setProperty("present", false);
                break;
                case BatteryDischarging:
                    ui->setProperty("charging", false);
                break;
                case BatteryUnknown:
                default:
                    ui->setProperty("charging", false);
            }
        }
    }
    void batteryTemperatureChanged(int temperature){
        qDebug() << "Battery temperature: " << temperature;
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            ui->setProperty("temperature", temperature);
        }
    }
    void batteryWarning(){
        qDebug() << "Battery Warning!";
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            ui->setProperty("warning", true);
        }
    }
    void chargerStateChanged(int state){
        switch(state){
            case ChargerConnected:
                qDebug() << "Charger state: Connected";
            break;
            case ChargerNotPresent:
                qDebug() << "Charger state: Not Present";
            break;
            case ChargerNotConnected:
                qDebug() << "Charger state: Not Connected";
            break;
            case ChargerUnknown:
            default:
                qDebug() << "Charger state: Unknown";
        }
        QObject* ui = root->findChild<QObject*>("batteryLevel");
        if(ui){
            if(state != BatteryNotPresent){
                ui->setProperty("present", true);
            }
            switch(state){
                case ChargerConnected:
                    ui->setProperty("connected", true);
                    m_powerConnected = true;
                break;
                case ChargerNotConnected:
                case ChargerNotPresent:
                    m_powerConnected = false;
                    // Fall through on purpose
                case ChargerUnknown:
                default:
                    ui->setProperty("connected", false);
            }
        }
    }
    void chargerWarning(){
        // TODO handle charger
    }
    void powerStateChanged(int state){
        Q_UNUSED(state);
        // TODO handle requested battery state
    }
    void bssFound(const QDBusObjectPath& path){
        Q_UNUSED(path);
    }
    void bssRemoved(const QDBusObjectPath& path){
        Q_UNUSED(path);
    }
    void disconnected(){
        wifiStateChanged(wifiApi->state());
    }
    void networkAdded(const QDBusObjectPath& path){
        Q_UNUSED(path);
    }
    void networkConnected(const QDBusObjectPath& path){
        Q_UNUSED(path);
    }
    void networkRemoved(const QDBusObjectPath& path){
        Q_UNUSED(path);
    }
    void wifiStateChanged(int state){
        if(state == wifiState){
            return;
        }
        wifiState = state;
        switch(state){
            case WifiOff:
                qDebug() << "Wifi state: Off";
            break;
            case WifiDisconnected:
                qDebug() << "Wifi state: On+Disconnected";
            break;
            case WifiOffline:
                qDebug() << "Wifi state: On+Connected+Offline";
            break;
            case WifiOnline:
                qDebug() << "Wifi state: On+Connected+Online";
            break;
            case WifiUnknown:
            default:
                qDebug() << "Wifi state: Unknown";
        }
        QObject* ui = root->findChild<QObject*>("wifiState");
        if(ui){
            switch(state){
                case WifiOff:
                    ui->setProperty("state", "down");
                break;
                case WifiDisconnected:
                    ui->setProperty("state", "up");
                    ui->setProperty("connected", false);
                break;
                case WifiOffline:
                    ui->setProperty("state", "up");
                    ui->setProperty("connected", true);
                break;
                case WifiOnline:
                    ui->setProperty("state", "up");
                    ui->setProperty("connected", true);
                    ui->setProperty("link", wifiLink);
                break;
                case WifiUnknown:
                default:
                    ui->setProperty("state", "unkown");
            }
        }
    }
    void wifiLinkChanged(int link){
        wifiLink = link;
        QObject* ui = root->findChild<QObject*>("wifiState");
        if(ui){
            if(wifiState != WifiOnline){
                link = 0;
            }
            ui->setProperty("link", link);
        }
    }
private:
    void checkUITimer();
    bool m_automaticSleep = true;
    int m_columns = 6;
    int m_fontSize = 23;
    bool m_showWifiDb = false;
    bool m_showBatteryPercent = false;
    bool m_showBatteryTemperature = false;
    int m_sleepAfter = 5;
    bool m_powerConnected = false;

    int wifiState = WifiUnknown;
    int wifiLink = 0;
    int wifiLevel = 0;
    bool wifiConnected = false;

    SysObject wifi;
    QTimer* uiTimer;
    InputManager inputManager;
    Power* powerApi = nullptr;
    Wifi* wifiApi = nullptr;
    QList<QObject*> applications;
    AppItem* getApplication(QString name);
};
