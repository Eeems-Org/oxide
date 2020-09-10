#ifndef BATTERYAPI_H
#define BATTERYAPI_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QException>
#include <QCoreApplication>

#include "dbussettings.h"
#include "sysobject.h"

class PowerAPI : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("D-Bus Interface", OXIDE_POWER_INTERFACE)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(int batteryState READ batteryState NOTIFY batteryStateChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(int batteryTemperature READ batteryTemperature NOTIFY batteryTemperatureChanged)
    Q_PROPERTY(int chargerState READ chargerState NOTIFY chargerStateChanged)
public:
    PowerAPI(QObject* parent)
    : QObject(parent),
      battery("/sys/class/power_supply/bq27441-0") {
        timer = new QTimer(this);
        timer->setSingleShot(false);
        timer->setInterval(3 * 1000); // 3 seconds
        timer->moveToThread(qApp->thread());
        connect(timer, &QTimer::timeout, this, QOverload<>::of(&PowerAPI::update));
        timer->start();
    }
    ~PowerAPI(){}

    enum State { Normal, PowerSaving };
    Q_ENUM(State)
    enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
    Q_ENUM(BatteryState)
    enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected, ChargerNotPresent };
    Q_ENUM(ChargerState)

    int state(){ return m_state; }
    void setState(int state){
        if(state < State::Normal || state > State::PowerSaving){
            throw QException{};
        }
        m_state = state;
        emit stateChanged(state);
    }

    int batteryState() { return m_batteryState; }
    void setBatteryState(int batteryState){
        m_batteryState = batteryState;
        emit batteryStateChanged(batteryState);
    }

    int batteryLevel() { return m_batteryLevel; }
    void setBatteryLevel(int batteryLevel){
        m_batteryLevel = batteryLevel;
        emit batteryLevelChanged(batteryLevel);
    }

    int batteryTemperature() { return m_batteryTemperature; }
    void setBatteryTemperature(int batteryTemperature){
        m_batteryTemperature = batteryTemperature;
        emit batteryTemperatureChanged(batteryTemperature);
    }

    int chargerState() { return m_chargerState; }
    void setChargerState(int chargerState){
        m_chargerState = chargerState;
        emit chargerStateChanged(chargerState);
    }

Q_SIGNALS:
    void stateChanged(int);
    void batteryStateChanged(int);
    void batteryLevelChanged(int);
    void batteryTemperatureChanged(int);
    void chargerStateChanged(int);
    void batteryWarning();
    void batteryAlert();
    void chargerWarning();

private:
    QTimer* timer;
    SysObject battery;
    int m_state = Normal;
    int m_batteryState = BatteryUnknown;
    int m_batteryLevel;
    int m_batteryTemperature;
    int m_chargerState = ChargerUnknown;
    bool m_batteryWarning = false;
    bool m_batteryAlert = false;

private slots:
    void update(){
        if(!battery.exists()){
            if(!m_batteryWarning){
                qWarning() << "Can't find battery information";
                m_batteryWarning = true;
                emit batteryWarning();
            }
            return;
        }
        if(!battery.intProperty("present")){
            qWarning() << "Battery is somehow not in the device?";
            if(!m_batteryWarning){
                qWarning() << "Can't find battery information";
                m_batteryWarning = true;
                emit batteryWarning();
            }
            return;
        }
        int battery_level = battery.intProperty("capacity");
        if(batteryLevel() != battery_level){
            setBatteryLevel(battery_level);
        }
        std::string status = battery.strProperty("status");
        auto charging = status == "Charging";
        if(charging && batteryState() != BatteryCharging){
            setBatteryState(BatteryCharging);
        }else if(!charging && batteryState() != BatteryDischarging){
            setBatteryState(BatteryDischarging);
        }
        std::string capacityLevel = battery.strProperty("capacity_level");
        auto alert = capacityLevel == "Critical" || capacityLevel == "";
        if(m_batteryAlert != alert){
            m_batteryAlert = alert;
            if(alert){
                emit batteryAlert();
            }
        }
        auto warning = status == "Unknown" || status == "" || capacityLevel == "Unknown";
        if(m_batteryWarning != warning){
            if(warning){
                emit batteryWarning();
            }
            m_batteryWarning = warning;
        }
        int temperature = battery.intProperty("temp") / 10;
        if(batteryTemperature() != temperature){
            setBatteryTemperature(temperature);
        }
    }
};

#endif // BATTERYAPI_H
