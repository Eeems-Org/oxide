#ifndef BATTERYAPI_H
#define BATTERYAPI_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QException>
#include <QCoreApplication>
#include <QDir>

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
    : QObject(parent), batteries(), chargers(){
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
                break;
            }
            if(item.strProperty("type") == "Battery"){
                qDebug() << "    Found Battery!";
                batteries.append(item);
            }else if(item.strProperty("type") == "USB"){
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
    QList<SysObject> batteries;
    QList<SysObject> chargers;
    int m_state = Normal;
    int m_batteryState = BatteryUnknown;
    int m_batteryLevel;
    int m_batteryTemperature;
    int m_chargerState = ChargerUnknown;
    bool m_batteryWarning = false;
    bool m_batteryAlert = false;

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
    std::array<bool, 3> getBatteryStates(){
        bool alert = false;
        bool warning = false;
        bool charging = false;
        for(auto battery : batteries){
            auto capacityLevel = battery.strProperty("capacity_level");
            auto status = battery.strProperty("status");
            if(!charging && status == "Charging"){
                charging = true;
            }
            if(capacityLevel == "Critical" || capacityLevel == ""){
                alert = true;
            }
            if(status == "Unknown" || status == "" || capacityLevel == "Unknown"){
                warning = true;
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

private slots:
    void update(){
        if(!batteries.length()){
            if(!m_batteryWarning){
                qWarning() << "Can't find battery information";
                m_batteryWarning = true;
                emit batteryWarning();
            }
            return;
        }
        if(!batteryInt("present")){
            qWarning() << "Battery is somehow not in the device?";
            if(!m_batteryWarning){
                qWarning() << "Can't find battery information";
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
};

#endif // BATTERYAPI_H
