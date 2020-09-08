#pragma once

#include <QObject>
#include <QJsonObject>

#include "appitem.h"
#include "eventfilter.h"
#include "sysobject.h"
#include "inputmanager.h"
#include "wifimanager.h"
#include "dbusservice.h"

class Controller : public QObject
{
    Q_OBJECT
public:
    EventFilter* filter;
    WifiManager* wifiManager;
    DBusService* dbusService;
    QObject* root = nullptr;
    explicit Controller(QObject* parent = 0) : QObject(parent), battery("/sys/class/power_supply/bq27441-0"), wifi("/sys/class/net/wlan0"), inputManager(){
        uiTimer = new QTimer(this);
        uiTimer->setSingleShot(false);
        uiTimer->setInterval(3 * 1000); // 3 seconds
        connect(uiTimer, &QTimer::timeout, this, QOverload<>::of(&Controller::updateUIElements));
        uiTimer->start();
    }
    Q_INVOKABLE bool turnOnWifi(){
        system("ifconfig wlan0 up");
        if(wifiManager == nullptr){
            if(!WifiManager::ensureService()){
                return false;
            }
            wifiManager = WifiManager::singleton();
            wifiState = "up";
            wifiConnected = false;
            QObject* ui = root->findChild<QObject*>("wifiState");
            if(ui){
                ui->setProperty("state", wifiState);
                ui->setProperty("connected", wifiConnected);
            }
        }else{
            wifiManager->loadNetworks();
        }
        return true;
    };
    Q_INVOKABLE bool turnOffWifi(){
        if(wifiManager != nullptr){
            delete wifiManager;
            wifiManager = nullptr;
            system("killall wpa_supplicant");
        }
        system("ifconfig wlan0 down");
        wifiState = "down";
        wifiConnected = false;
        QObject* ui = root->findChild<QObject*>("wifiState");
        if(ui){
            ui->setProperty("state", wifiState);
            ui->setProperty("connected", wifiConnected);
        }
        return true;
    };
    Q_INVOKABLE bool wifiOn(){
        return wifiManager != nullptr;
    };
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE QList<QObject*> getApps();
    Q_INVOKABLE void powerOff();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void killXochitl();
    void updateBatteryLevel();
    void updateWifiState();
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
    int getBatteryCharging(){
        return batteryCharging;
    }
signals:
    void automaticSleepChanged(bool);
    void columnsChanged(int);
    void fontSizeChanged(int);
    void showWifiDbChanged(bool);
    void showBatteryPercentChanged(bool);
    void showBatteryTemperatureChanged(bool);
    void sleepAfterChanged(int);
private slots:
    void updateUIElements();
private:
    void updateHiddenUIElements();
    void checkUITimer();
    bool m_automaticSleep = true;
    int m_columns = 6;
    int m_fontSize = 23;
    bool m_showWifiDb = false;
    bool m_showBatteryPercent = false;
    bool m_showBatteryTemperature = false;
    int m_sleepAfter = 5;

    bool batteryAlert = false;
    bool batteryWarning = true;
    bool batteryCharging = false;
    int batteryLevel = 0;
    int batteryTemperature = 0;

    QString wifiState = "Unknown";
    int wifiLink = 0;
    int wifiLevel = 0;
    bool wifiConnected = false;

    SysObject battery;
    SysObject wifi;
    QTimer* uiTimer;
    InputManager inputManager;
};
