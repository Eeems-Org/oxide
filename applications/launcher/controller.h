#pragma once

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QGuiApplication>
#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <liboxide/sysobject.h>
#include <liboxide/tarnish.h>

#include "appitem.h"
#include "wifinetworklist.h"
#include "notificationlist.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

using namespace codes::eeems::oxide1;
using codes::eeems::oxide1::Power;
using Oxide::SysObject;
using Oxide::EventFilter;
using namespace Oxide::Sentry;

enum State { Normal, PowerSaving };
enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };
enum WifiState { WifiUnknown, WifiOff, WifiDisconnected, WifiOffline, WifiOnline};
enum SwipeDirection { None, Right, Left, Up, Down };

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool automaticSleep MEMBER m_automaticSleep WRITE setAutomaticSleep NOTIFY automaticSleepChanged)
    Q_PROPERTY(bool lockOnSuspend MEMBER m_lockOnSuspend WRITE setLockOnSuspend NOTIFY lockOnSuspendChanged)
    Q_PROPERTY(bool automaticLock MEMBER m_automaticLock WRITE setAutomaticLock NOTIFY automaticLockChanged)
    Q_PROPERTY(int columns MEMBER m_columns WRITE setColumns NOTIFY columnsChanged)
    Q_PROPERTY(int swipeLengthRight MEMBER m_swipeLengthRight WRITE setSwipeLengthRight NOTIFY swipeLengthRightChanged)
    Q_PROPERTY(int swipeLengthLeft MEMBER m_swipeLengthLeft WRITE setSwipeLengthLeft NOTIFY swipeLengthLeftChanged)
    Q_PROPERTY(int swipeLengthUp MEMBER m_swipeLengthUp WRITE setSwipeLengthUp NOTIFY swipeLengthUpChanged)
    Q_PROPERTY(int swipeLengthDown MEMBER m_swipeLengthDown WRITE setSwipeLengthDown NOTIFY swipeLengthDownChanged)
    Q_PROPERTY(bool showWifiDb MEMBER m_showWifiDb WRITE setShowWifiDb NOTIFY showWifiDbChanged)
    Q_PROPERTY(bool showBatteryPercent MEMBER m_showBatteryPercent WRITE setShowBatteryPercent NOTIFY showBatteryPercentChanged)
    Q_PROPERTY(bool showBatteryTemperature MEMBER m_showBatteryTemperature WRITE setShowBatteryTemperature NOTIFY showBatteryTemperatureChanged)
    Q_PROPERTY(int sleepAfter MEMBER m_sleepAfter WRITE setSleepAfter NOTIFY sleepAfterChanged)
    Q_PROPERTY(int lockAfter MEMBER m_lockAfter WRITE setLockAfter NOTIFY lockAfterChanged)
    Q_PROPERTY(WifiNetworkList* networks MEMBER networks READ getNetworks NOTIFY networksChanged)
    Q_PROPERTY(NotificationList* notifications MEMBER notifications READ getNotifications NOTIFY notificationsChanged)
    Q_PROPERTY(QStringList locales READ locales)
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QStringList timezones READ timezones)
    Q_PROPERTY(QString timezone READ timezone WRITE setTimezone NOTIFY timezoneChanged)
    Q_PROPERTY(bool wifiOn MEMBER m_wifion READ wifiOn NOTIFY wifiOnChanged)
    Q_PROPERTY(QString autoStartApplication READ autoStartApplication WRITE setAutoStartApplication NOTIFY autoStartApplicationChanged)
    Q_PROPERTY(bool powerOffInhibited READ powerOffInhibited NOTIFY powerOffInhibitedChanged)
    Q_PROPERTY(bool sleepInhibited READ sleepInhibited NOTIFY sleepInhibitedChanged)
    Q_PROPERTY(bool showDate MEMBER m_showDate WRITE setShowDate NOTIFY showDateChanged)
    Q_PROPERTY(bool hasNotification MEMBER m_hasNotification NOTIFY hasNotificationChanged)
    Q_PROPERTY(QString notificationText MEMBER m_notificationText NOTIFY notificationTextChanged)
    Q_PROPERTY(int maxTouchWidth READ maxTouchWidth)
    Q_PROPERTY(int maxTouchHeight READ maxTouchHeight)
public:
    static std::string exec(const char* cmd);
    EventFilter* filter;
    QObject* stateController;
    QObject* root = nullptr;
    explicit Controller(QObject* parent = 0)
    : QObject(parent),
      m_wifion(false),
      wifi("/sys/class/net/wlan0"),
      applications()
    {
        networks = new WifiNetworkList();
        notifications = new NotificationList();
        qDebug() << "Requesting APIs";
        connect(Oxide::Tarnish::getAPI(), &General::aboutToQuit, qApp, &QGuiApplication::quit);
        powerApi = Oxide::Tarnish::powerAPI();
        if(powerApi == nullptr){
            qFatal("Power API was not available");
        }
        // Connect to signals
        connect(powerApi, &Power::batteryLevelChanged, this, &Controller::batteryLevelChanged);
        connect(powerApi, &Power::batteryStateChanged, this, &Controller::batteryStateChanged);
        connect(powerApi, &Power::batteryTemperatureChanged, this, &Controller::batteryTemperatureChanged);
        connect(powerApi, &Power::chargerStateChanged, this, &Controller::chargerStateChanged);
        connect(powerApi, &Power::stateChanged, this, &Controller::powerStateChanged);
        connect(powerApi, &Power::batteryAlert, this, &Controller::batteryAlert);
        connect(powerApi, &Power::batteryWarning, this, &Controller::batteryWarning);
        connect(powerApi, &Power::chargerWarning, this, &Controller::chargerWarning);
        wifiApi = Oxide::Tarnish::wifiAPI();
        if(wifiApi == nullptr){
            qFatal("Wifi API was not available");
        }
        connect(wifiApi, &Wifi::disconnected, this, &Controller::disconnected);
        connect(wifiApi, &Wifi::networkConnected, this, &Controller::networkConnected);
        connect(wifiApi, &Wifi::stateChanged, this, &Controller::wifiStateChanged);
        connect(wifiApi, &Wifi::rssiChanged, this, &Controller::wifiRssiChanged);
        networks->setAPI(wifiApi);
        auto state = wifiApi->state();
        m_wifion = state != WifiState::WifiOff && state != WifiState::WifiUnknown;
        QTimer::singleShot(1000, [=](){
            // Get initial values when UI is ready
            batteryLevelChanged(powerApi->batteryLevel());
            batteryStateChanged(powerApi->batteryState());
            batteryTemperatureChanged(powerApi->batteryTemperature());
            chargerStateChanged(powerApi->chargerState());
            powerStateChanged(powerApi->state());

            wifiStateChanged(wifiApi->state());
            wifiRssiChanged(wifiApi->rssi());
            emit wifiOnChanged(m_wifion);
            if(stateController->property("state") == "wifi"){
                connectWifiSignals();
            }
            auto network = wifiApi->network();
            if(network.path() != "/"){
                networkConnected(network);
            }
        });
        systemApi = Oxide::Tarnish::systemAPI();
        if(systemApi == nullptr){
            qFatal("Could not request system API");
        }
        connect(systemApi, &System::powerOffInhibitedChanged, this, &Controller::powerOffInhibitedChanged);
        connect(systemApi, &System::powerOffInhibitedChanged, [=](bool value){
            qDebug() << "Power Off Inhibited:" << value;
        });
        connect(systemApi, &System::sleepInhibitedChanged, this, &Controller::sleepInhibitedChanged);
        connect(systemApi, &System::autoSleepChanged, [this](int sleepAfter){
            setAutomaticSleep(sleepAfter);
            setSleepAfter(sleepAfter);
        });
        connect(systemApi, &System::swipeLengthChanged, [this](int direction, int length){
            setSwipeLength(direction, length);
        });
        auto autoSleep = systemApi->autoSleep();
        setAutomaticSleep(autoSleep);
        setSleepAfter(autoSleep);
        for(short i = 1; i <= 4; ++i){
            setSwipeLength(i, systemApi->getSwipeLength(i));
        }
        emit powerOffInhibitedChanged(powerOffInhibited());
        emit sleepInhibitedChanged(sleepInhibited());
        appsApi = Oxide::Tarnish::appsAPI();
        if(appsApi == nullptr){
            qFatal("Apps API was not available");
        }
        connect(appsApi, &Apps::applicationUnregistered, this, &Controller::unregisterApplication);
        connect(appsApi, &Apps::applicationRegistered, this, &Controller::registerApplication);
        notificationApi = Oxide::Tarnish::notificationAPI();
        if(notificationApi == nullptr){
            qFatal("Notification API was not available");
        }
        connect(notificationApi, &Notifications::notificationAdded, this, &Controller::notificationAdded);
        connect(notificationApi, &Notifications::notificationRemoved, this, &Controller::notificationRemoved);
        connect(notificationApi, &Notifications::notificationChanged, this, &Controller::notificationChanged);
        connect(notifications, &NotificationList::updated, this, &Controller::notificationsUpdated);

        uiTimer = new QTimer(this);
        uiTimer->setSingleShot(false);
        uiTimer->setInterval(3 * 1000); // 3 seconds
        connect(uiTimer, &QTimer::timeout, this, QOverload<>::of(&Controller::updateUIElements));
        uiTimer->start();
    }
    Q_INVOKABLE void startup(){
        loadSettings();
        if(m_autoStartApplication.isEmpty()){
            qDebug() << "No auto start application";
            return;
        }
        auto app = getApplication(m_autoStartApplication);
        if(app == nullptr){
            qDebug() << "Unable to find auto start application";
            return;
        }
        app->execute();
    }
    Q_INVOKABLE bool turnOnWifi(){
        if(!wifiApi->enable()){
            return false;
        }
        m_wifion = true;
        emit wifiOnChanged(true);
        connect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
        return true;
    };
    Q_INVOKABLE void turnOffWifi(){
        wifiApi->disable();
        m_wifion = false;
        emit wifiOnChanged(false);
        disconnect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
        networks->removeUnknown();
        wifiApi->flushBSSCache(0);
    };
    bool wifiOn(){
        return m_wifion;
    }
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE QList<QObject*> getApps();
    Q_INVOKABLE void importDraftApps();
    Q_INVOKABLE void powerOff();
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void lock();
    Q_INVOKABLE void breadcrumb(QString category, QString message, QString type = "default"){
#ifdef SENTRY
        sentry_breadcrumb(category.toStdString().c_str(), message.toStdString().c_str(), type.toStdString().c_str());
#else
        Q_UNUSED(category);
        Q_UNUSED(message);
        Q_UNUSED(type);
#endif
    }
    void updateBatteryLevel();
    bool automaticSleep() const { return m_automaticSleep; }
    void setAutomaticSleep(bool);
    bool lockOnSuspend() const { return m_lockOnSuspend; }
    void setLockOnSuspend(bool);
    void setAutomaticLock(bool);
    int columns() const { return m_columns; }
    void setColumns(int);
    int swipeLengthRight() const { return m_swipeLengthRight; }
    void setSwipeLengthRight(int length) { setSwipeLength(1, length); }
    int swipeLengthLeft() const { return m_swipeLengthLeft; }
    void setSwipeLengthLeft(int length) { setSwipeLength(2, length); }
    int swipeLengthUp() const { return m_swipeLengthUp; }
    void setSwipeLengthUp(int length) { setSwipeLength(3, length); }
    int swipeLengthDown() const { return m_swipeLengthDown; }
    void setSwipeLengthDown(int length) { setSwipeLength(4, length); }
    int getSwipeLength(int direction);
    void setSwipeLength(int direction, int length);
    bool showWifiDb() const { return m_showWifiDb; }
    void setShowWifiDb(bool);
    bool showBatteryPercent() const { return m_showBatteryPercent; }
    void setShowBatteryPercent(bool);
    bool showBatteryTemperature() const { return m_showBatteryTemperature; }
    void setShowBatteryTemperature(bool);
    int sleepAfter() const { return systemApi->autoSleep(); }
    void setSleepAfter(int);
    int lockAfter() const { return systemApi->autoLock(); }
    void setLockAfter(int);
    void setShowDate(bool showDate){
        m_showDate = showDate;
        emit showDateChanged(showDate);
        if(root == nullptr){
            return;
        }
        QObject* clock = root->findChild<QObject*>("clock");
        if(clock == nullptr){
            return;
        }
        QString text = "";
        if(showDate){
            text = QDate::currentDate().toString(Qt::TextDate) + " ";
        }
        clock->setProperty("text", text + QTime::currentTime().toString("h:mm a"));
    }
    bool showDate(){ return m_showDate; }
    void setNotification(QString notificationText){
        if(m_notificationText == notificationText){
            return;
        }
        m_notificationText = notificationText;
        emit notificationTextChanged(notificationText);
        if(!m_hasNotification){
            m_hasNotification = true;
            emit hasNotificationChanged(true);
        }
    }
    void clearNotification(){
        m_notificationText = "";
        emit notificationTextChanged(m_notificationText);
        m_hasNotification = false;
        emit hasNotificationChanged(m_hasNotification);
    }
    QString autoStartApplication() { return m_autoStartApplication; }
    void setAutoStartApplication(QString autoStartApplication);
    bool getPowerConnected(){ return m_powerConnected; }
    WifiNetworkList* getNetworks(){ return networks; }
    NotificationList* getNotifications() { return notifications; }
    QStringList locales() { return deviceSettings.getLocales(); }
    QString locale() { return deviceSettings.getLocale(); }
    void setLocale(const QString& locale) {
        deviceSettings.setLocale(locale);
        emit localeChanged(locale);
    }
    QStringList timezones() { return deviceSettings.getTimezones(); }
    QString timezone() { return deviceSettings.getTimezone(); }
    void setTimezone(const QString& timezone) {
        deviceSettings.setTimezone(timezone);
        emit timezoneChanged(timezone);
    }
    bool powerOffInhibited(){ return systemApi->powerOffInhibited(); }
    bool sleepInhibited(){ return systemApi->sleepInhibited(); }
    int maxTouchWidth(){ return deviceSettings.getTouchWidth() * 0.9; }
    int maxTouchHeight(){ return deviceSettings.getTouchHeight() * 0.9; }

    Q_INVOKABLE void disconnectWifiSignals(){
        disconnect(wifiApi, &Wifi::bssFound, this, &Controller::bssFound);
        disconnect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
        disconnect(wifiApi, &Wifi::networkAdded, this, &Controller::networkAdded);
        disconnect(wifiApi, &Wifi::networkRemoved, this, &Controller::networkRemoved);
    }
    Q_INVOKABLE void connectWifiSignals(){
        networks->clear();
        QList<Network*> networksToAdd;
        for(auto path : wifiApi->networks()){
            auto network = new Network(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
            auto ssid = network->ssid();
            if(ssid.isEmpty()){
                delete network;
                continue;
            }
            networksToAdd.append(network);
        }
        networks->append(networksToAdd);
        QList<BSS*> bsssToAdd;
        for(auto path : wifiApi->bSSs()){
            auto bss = new BSS(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
            auto ssid = bss->ssid();
            if(ssid.isEmpty()){
                delete bss;
                continue;
            }
            bsssToAdd.append(bss);
        }
        networks->append(bsssToAdd);
        networks->sort();
        networks->setConnected(wifiApi->network());
        connect(wifiApi, &Wifi::bssFound, this, &Controller::bssFound);
        connect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
        connect(wifiApi, &Wifi::networkAdded, this, &Controller::networkAdded);
        connect(wifiApi, &Wifi::networkRemoved, this, &Controller::networkRemoved);
    }
    Apps* getAppsApi() { return appsApi; }
signals:
    void reload();
    void automaticSleepChanged(bool);
    void lockOnSuspendChanged(bool);
    void automaticLockChanged(bool);
    void columnsChanged(int);
    void swipeLengthRightChanged(int);
    void swipeLengthLeftChanged(int);
    void swipeLengthUpChanged(int);
    void swipeLengthDownChanged(int);
    void fontSizeChanged(int);
    void showWifiDbChanged(bool);
    void showBatteryPercentChanged(bool);
    void showBatteryTemperatureChanged(bool);
    void sleepAfterChanged(int);
    void lockAfterChanged(int);
    void networksChanged(WifiNetworkList*);
    void localeChanged(QString);
    void timezoneChanged(QString);
    void wifiOnChanged(bool);
    void autoStartApplicationChanged(QString);
    void powerOffInhibitedChanged(bool);
    void sleepInhibitedChanged(bool);
    void showDateChanged(bool);
    void hasNotificationChanged(bool);
    void notificationTextChanged(QString);
    void notificationsChanged(NotificationList*);

public slots:
    void updateUIElements();

private slots:
    void notificationsUpdated(){
        if(notifications->length() > 1){
            setNotification(QStringLiteral("%1 notifications").arg(notifications->length()));
            return;
        }else if(!notifications->empty()){
            setNotification("1 notification");
        }else{
            clearNotification();
        }
        emit notificationsChanged(notifications);
    }
    void notificationAdded(const QDBusObjectPath& path){
        auto notification = new Notification(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
        notifications->append(notification);
        // Notification UI updates will be handled by notificationsUpdated
    }
    void notificationRemoved(const QDBusObjectPath& path){
        auto notification = notifications->get(path);
        if(notification == nullptr){
            return;
        }
        notifications->removeAll(notification);
        notification->deleteLater();
        // Notification UI updates will be handled by notificationsUpdated
    }
    void notificationChanged(const QDBusObjectPath& path){
        Q_UNUSED(path);
        emit notificationsChanged(notifications);
    }
    void unregisterApplication(QDBusObjectPath path){
        auto pathString = path.path();
        for(auto app : applications){
            if(app->property("path") == pathString){
                applications.removeAll(app);
                delete app;
                emit reload();
                qDebug() << "Removed" << pathString << "application";
                return;
            }
        }
        qDebug() << "Unable to find application " << pathString << "to remove";
    }
    void registerApplication(QDBusObjectPath path){
        qDebug() << "New application detected" << path.path();
        emit reload();
    }
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
                    ui->setProperty("connected", false);
                    m_powerConnected = false;
                break;
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
        auto bss = new BSS(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
        auto ssid = bss->ssid();
        if(ssid.isEmpty()){
            delete bss;
            return;
        }
        networks->append(bss);
    }
    void bssRemoved(const QDBusObjectPath& path){
        networks->remove(path);
    }
    void disconnected(){
        wifiStateChanged(wifiApi->state());
        networks->setConnected(QDBusObjectPath("/"));
    }
    void networkAdded(const QDBusObjectPath& path){
        auto network = new Network(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
        auto ssid = network->ssid();
        if(ssid.isEmpty()){
            delete network;
            return;
        }
        networks->append(network);
    }
    void networkConnected(const QDBusObjectPath& path){
        networks->setConnected(path);
    }
    void networkRemoved(const QDBusObjectPath& path){
        networks->remove(path);
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
                    ui->setProperty("rssi", wifiApi->rssi());
                break;
                case WifiUnknown:
                default:
                    ui->setProperty("state", "unkown");
            }
        }
    }
    void wifiRssiChanged(int rssi){
        QObject* ui = root->findChild<QObject*>("wifiState");
        if(ui){
            if(wifiState != WifiOnline){
                rssi = -100;
            }
            ui->setProperty("rssi", rssi);
        }
    }
private:
    void checkUITimer();
    bool m_automaticSleep = true;
    bool m_lockOnSuspend = true;
    bool m_automaticLock = true;
    int m_columns = 4;
    int m_swipeLengthRight = 30;
    int m_swipeLengthLeft = 30;
    int m_swipeLengthUp = 30;
    int m_swipeLengthDown = 30;
    int m_fontSize = 23;
    int m_sleepAfter;
    int m_lockAfter;
    bool m_showWifiDb = false;
    bool m_showBatteryPercent = false;
    bool m_showBatteryTemperature = false;
    bool m_showDate = false;
    QString m_autoStartApplication = "";
    bool m_hasNotification = false;
    QString m_notificationText = "";

    bool m_powerConnected = false;
    int wifiState = WifiUnknown;

    bool m_wifion;
    SysObject wifi;
    QTimer* uiTimer;
    Power* powerApi = nullptr;
    Wifi* wifiApi = nullptr;
    System* systemApi = nullptr;
    Apps* appsApi = nullptr;
    Notifications* notificationApi = nullptr;
    QList<QObject*> applications;
    AppItem* getApplication(QString name);
    WifiNetworkList* networks;
    NotificationList* notifications;
};
