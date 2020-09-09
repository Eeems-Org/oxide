#ifndef BATTERYAPI_H
#define BATTERYAPI_H

#include <QObject>
#include <QException>
#include <QCoreApplication>

#include "dbussettings.h"

class PowerAPI : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("D-Bus Interface", OXIDE_BATTERY_INTERFACE)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(int batteryState READ batteryState NOTIFY batteryStateChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
    Q_PROPERTY(int chargerState READ chargerState NOTIFY chargerStateChanged)
public:
    PowerAPI(QObject* parent) : QObject(parent){}
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
        this->moveToThread(qApp->thread());
        emit stateChanged(state);
    }

    int batteryState() { return m_batteryState; }
    void setBatteryState(int batteryState){
        m_batteryState = batteryState;
        this->moveToThread(qApp->thread());
        emit batteryStateChanged(batteryState);
    }

    int batteryLevel() { return m_batteryLevel; }
    void setBatteryLevel(int batteryLevel){
        m_batteryLevel = batteryLevel;
        this->moveToThread(qApp->thread());
        emit batteryLevelChanged(batteryLevel);
    }

    int chargerState() { return m_chargerState; }
    void setChargerState(int chargerState){
        m_chargerState = chargerState;
        this->moveToThread(qApp->thread());
        emit chargerStateChanged(chargerState);
    }

Q_SIGNALS:
    void stateChanged(int);
    void batteryStateChanged(int);
    void batteryLevelChanged(int);
    void chargerStateChanged(int);
    void batteryWarning();
    void chargerWarning();

private:
    int m_state = Normal;
    int m_batteryState = BatteryUnknown;
    int m_batteryLevel;
    int m_chargerState = ChargerUnknown;

};

#endif // BATTERYAPI_H
