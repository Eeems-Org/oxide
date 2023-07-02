#ifndef BATTERYAPI_H
#define BATTERYAPI_H

#include <liboxide.h>
#include <liboxide/udev.h>

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QException>
#include <QCoreApplication>
#include <QDir>

#include "apibase.h"
#include "systemapi.h"

#define powerAPI PowerAPI::singleton()

class PowerAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_POWER_INTERFACE)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(int batteryState READ batteryState NOTIFY batteryStateChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(int batteryTemperature READ batteryTemperature NOTIFY batteryTemperatureChanged)
    Q_PROPERTY(int chargerState READ chargerState NOTIFY chargerStateChanged)

public:
    static PowerAPI* singleton(PowerAPI* self = nullptr){
        static PowerAPI* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    PowerAPI(QObject* parent)
    : APIBase(parent), m_chargerState(ChargerUnknown){
        Oxide::Sentry::sentry_transaction("power", "init", [this](Oxide::Sentry::Transaction* t){
            Oxide::Sentry::sentry_span(t, "singleton", "Setup singleton", [this]{
                singleton(this);
            });
            Oxide::Sentry::sentry_span(t, "sysfs", "Determine power devices from sysfs", [this](){
                Oxide::Power::batteries();
                Oxide::Power::chargers();
            });
            Oxide::Sentry::sentry_span(t, "update", "Update current state", [this]{
                update();
            });
            Oxide::Sentry::sentry_span(t, "monitor", "Setup monitor", [this]{
                if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM1){
                    UDev::singleton()->subsystem("power_supply", [this]{
                        update();
                    });
                }else{
                    timer = new QTimer(this);
                    timer->setSingleShot(false);
                    timer->setInterval(3 * 1000); // 3 seconds
                    timer->moveToThread(qApp->thread());
                    connect(timer, &QTimer::timeout, this, QOverload<>::of(&PowerAPI::update));
                    timer->start();
                }
            });
        });
    }
    ~PowerAPI(){
        if(timer != nullptr){
            qDebug() << "Killing timer";
            timer->stop();
            delete timer;
        }else{
            qDebug() << "Killing UDev monitor";
            UDev::singleton()->stop();
        }
    }

    void setEnabled(bool enabled) override {
        if(enabled){
            if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM1){
                UDev::singleton()->start();
            }else{
                timer->start();
            }
        }else if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM1){
            UDev::singleton()->stop();
        }else{
            timer->stop();
        }
    }

    enum State { Normal, PowerSaving };
    Q_ENUM(State)
    enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
    Q_ENUM(BatteryState)
    enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected };
    Q_ENUM(ChargerState)

    int state(){
        if(!hasPermission("power")){
            return 0;
        }
        return m_state;
    }
    void setState(int state){
        if(!hasPermission("power")){
            return;
        }
        if(state < State::Normal || state > State::PowerSaving){
            throw QException{};
        }
        m_state = state;
        emit stateChanged(state);
    }

    int batteryState() {
        if(!hasPermission("power")){
            return BatteryUnknown;
        }
        return m_batteryState;
    }
    void setBatteryState(int batteryState){
        if(!hasPermission("power")){
            return;
        }
        m_batteryState = batteryState;
        emit batteryStateChanged(batteryState);
    }

    int batteryLevel() {
        if(!hasPermission("power")){
            return 0;
        }
        return m_batteryLevel;
    }
    void setBatteryLevel(int batteryLevel){
        m_batteryLevel = batteryLevel;
        emit batteryLevelChanged(batteryLevel);
    }

    int batteryTemperature() {
        if(!hasPermission("power")){
            return 0;
        }
        return m_batteryTemperature;
    }
    void setBatteryTemperature(int batteryTemperature){
        m_batteryTemperature = batteryTemperature;
        emit batteryTemperatureChanged(batteryTemperature);
    }

    int chargerState() {
        if(!hasPermission("power")){
            return ChargerUnknown;
        }
        return m_chargerState;
    }
    void setChargerState(int chargerState){
        m_chargerState = chargerState;
        emit chargerStateChanged(chargerState);
    }

signals:
    void stateChanged(int);
    void batteryStateChanged(int);
    void batteryLevelChanged(int);
    void batteryTemperatureChanged(int);
    void chargerStateChanged(int);
    void batteryWarning();
    void batteryAlert();
    void chargerWarning();

private:
    QTimer* timer = nullptr;
    int m_state = Normal;
    int m_batteryState = BatteryUnknown;
    int m_batteryLevel = 0;
    int m_batteryTemperature = 0;
    int m_chargerState;
    bool m_batteryWarning = false;
    bool m_batteryAlert = false;
    bool m_chargerWarning = false;

    void updateBattery(){
        if(!Oxide::Power::batteries()->length()){
            if(m_batteryState != BatteryUnknown){
                setBatteryState(BatteryUnknown);
            }
            if(!m_batteryWarning){
                O_WARNING("Can't find battery information");
                m_batteryWarning = true;
                emit batteryWarning();
            }
            return;
        }
        if(!Oxide::Power::batteryPresent()){
            if(m_batteryState != BatteryNotPresent){
                O_WARNING("Battery is somehow not in the device?");
                setBatteryState(BatteryNotPresent);
            }
            if(!m_batteryWarning){
                O_WARNING("Battery is somehow not in the device?");
                m_batteryWarning = true;
                emit batteryWarning();
            }
            return;
        }
        int battery_level = Oxide::Power::batteryLevel();
        if(m_batteryLevel != battery_level){
            setBatteryLevel(battery_level);
        }
        bool charging = Oxide::Power::batteryCharging();
        if(charging && m_batteryState != BatteryCharging){
            setBatteryState(BatteryCharging);
        }else if(!charging && m_batteryState != BatteryDischarging){
            setBatteryState(BatteryDischarging);
        }
        bool alert = Oxide::Power::batteryHasAlert();
        if(m_batteryAlert != alert){
            m_batteryAlert = alert;
            if(alert){
                emit batteryAlert();
            }
        }
        bool warning = Oxide::Power::batteryHasWarning();
        if(m_batteryWarning != warning){
            if(warning){
                emit batteryWarning();
            }
            m_batteryWarning = warning;
        }
        int temperature = Oxide::Power::batteryTemperature();
        if(m_batteryTemperature != temperature){
            setBatteryTemperature(temperature);
        }
    }
    void updateCharger(){
        if(!Oxide::Power::chargers()->length()){
            if(m_chargerState != ChargerUnknown){
                setChargerState(ChargerUnknown);
            }
            if(!m_chargerWarning){
                O_WARNING("Can't find charger information");
                m_chargerWarning = true;
                emit chargerWarning();
            }
            return;
        }
        bool connected = Oxide::Power::chargerConnected();
        if(connected && m_chargerState != ChargerConnected){
            setChargerState(ChargerConnected);
        }else if(!connected && m_chargerState != ChargerNotConnected){
            setChargerState(ChargerNotConnected);
        }
    }

private slots:
    void update(){
        updateBattery();
        updateCharger();
    }
};

#endif // BATTERYAPI_H
