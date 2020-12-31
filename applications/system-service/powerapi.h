#ifndef BATTERYAPI_H
#define BATTERYAPI_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QException>
#include <QCoreApplication>
#include <QDir>

#include "apibase.h"
#include "sysobject.h"
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
    : APIBase(parent), batteries(), chargers(), m_chargerState(ChargerUnknown){
        singleton(this);
        QDir dir("/sys/class/power_supply");
        qDebug() << "Looking for batteries and chargers...";
        for(auto path : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)){
            qDebug() << ("  Checking " + path + "...").toStdString().c_str();
            SysObject item(dir.path() + "/" + path);
            if(!item.hasProperty("type")){
                qDebug() << "    Missing type property";
                continue;
            }
            if(item.hasProperty("present") && !item.intProperty("present")){
                qDebug() << "    Either missing present property, or battery is not present";
                continue;
            }
            auto type = item.strProperty("type");
            if(type == "Battery"){
                qDebug() << "    Found Battery!";
                batteries.append(item);
            }else if(type == "USB" || type == "USB_CDP"){
                qDebug() << "    Found Charger!";
                chargers.append(item);
            }else{
                qDebug() << "    Unknown type";
            }
        }
        update();
        timer = new QTimer(this);
        timer->setSingleShot(false);
        timer->setInterval(3 * 1000); // 3 seconds
        timer->moveToThread(qApp->thread());
        connect(timer, &QTimer::timeout, this, QOverload<>::of(&PowerAPI::update));
        timer->start();
    }
    ~PowerAPI(){
        qDebug() << "Killing timer";
        timer->stop();
        delete timer;
    }

    void setEnabled(bool enabled){
        if(enabled){
            timer->start();
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
    QList<SysObject> batteries;
    QList<SysObject> chargers;
    int m_state = Normal;
    int m_batteryState = BatteryUnknown;
    int m_batteryLevel = 0;
    int m_batteryTemperature = 0;
    int m_chargerState;
    bool m_batteryWarning = false;
    bool m_batteryAlert = false;
    bool m_chargerWarning = false;

    bool badHealth(QString match){
        static const  QSet<QString> ret {
            "Overheat",
            "Dead",
            "Over voltage",
            "Unspecified failure",
            "Cold",
            "Watchdog timer expire",
            "Safety timer expire",
            "Over current"
        };
        return ret.contains(match);
    }
    bool warnHealth(QString match){
        static const QSet<QString> ret {
            "Unknown",
            "Warm",
            "Cool",
            "Hot"
        };
        return ret.contains(match);
    }
    int batteryInt(QString property){
        int result = 0;
        for(auto battery : batteries){
            result += battery.intProperty(property.toStdString());
        }
        return result;
    }
    int batteryIntMax(QString property){
        int result = 0;
        for(auto battery : batteries){
            int value = battery.intProperty(property.toStdString());
            if(value > result){
                result = value;
            }
        }
        return result;
    }
    int calcBatteryLevel(){
        return batteryInt("capacity") / batteries.length();
    }
    int chargerInt(QString property){
        int result = 0;
        for(auto charger : chargers){
            result += charger.intProperty(property.toStdString());
        }
        return result;
    }
    std::array<bool, 3> getBatteryStates(){
        bool alert = false;
        bool warning = false;
        bool charging = false;
        for(auto battery : batteries){
            auto status = battery.strProperty("status");
            if(!charging && status == "Charging"){
                charging = true;
            }
            if(status == "Unknown" || status == ""){
                warning = true;
            }
            if(battery.hasProperty("health")){
                auto health = battery.strProperty("health");
                if(badHealth(QString(health.c_str()))){
                    alert = true;
                }else if(!warning && warnHealth(QString(health.c_str()))){
                    warning = true;
                }
            }else if(!alert && battery.hasProperty("temp")){
                auto temp = battery.intProperty("temp");
                if(battery.hasProperty("temp_alert_max") && temp > battery.intProperty("temp_alert_max")){
                    alert = true;
                }
                if(battery.hasProperty("temp_alert_min") && temp < battery.intProperty("temp_alert_min")){
                    alert = true;
                }
            }
            if(alert && warning && charging){
                break;
            }
        }
        std::array<bool, 3> state;
        state[0] = charging;
        state[1] = warning;
        state[2] = alert;
        return state;
    }
    void updateBattery(){
        if(!batteries.length()){
            if(m_batteryState != BatteryUnknown){
                setBatteryState(BatteryUnknown);
            }
            if(!m_batteryWarning){
                qWarning() << "Can't find battery information";
                m_batteryWarning = true;
                emit batteryWarning();
            }
            return;
        }
        if(!batteryInt("present")){
            if(m_batteryState != BatteryNotPresent){
                qWarning() << "Battery is somehow not in the device?";
                setBatteryState(BatteryNotPresent);
            }
            if(!m_batteryWarning){
                qWarning() << "Battery is somehow not in the device?";
                m_batteryWarning = true;
                emit batteryWarning();
            }
            return;
        }
        int battery_level = calcBatteryLevel();
        if(batteryLevel() != battery_level){
            setBatteryLevel(battery_level);
        }
        auto states = getBatteryStates();
        bool charging = states[0];
        bool warning = states[1];
        bool alert = states[2];
        if(charging && batteryState() != BatteryCharging){
            setBatteryState(BatteryCharging);
        }else if(!charging && batteryState() != BatteryDischarging){
            setBatteryState(BatteryDischarging);
        }
        if(m_batteryAlert != alert){
            m_batteryAlert = alert;
            if(alert){
                emit batteryAlert();
            }
        }
        if(m_batteryWarning != warning){
            if(warning){
                emit batteryWarning();
            }
            m_batteryWarning = warning;
        }
        int temperature = batteryIntMax("temp") / 10;
        if(batteryTemperature() != temperature){
            setBatteryTemperature(temperature);
        }
    }
    void updateCharger(){
        if(!chargers.length()){
            if(m_chargerState != ChargerUnknown){
                setChargerState(ChargerUnknown);
            }
            if(!m_chargerWarning){
                qWarning() << "Can't find charger information";
                m_chargerWarning = true;
                emit chargerWarning();
            }
            return;
        }
        bool connected = chargerInt("online");
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
