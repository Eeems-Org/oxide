#ifndef BATTERYAPI_H
#define BATTERYAPI_H

#include <QObject>
#include <QException>

#include "dbussettings.h"

class BatteryAPI : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_BATTERY_INTERFACE)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY stateChanged)
public:
    BatteryAPI(QObject* parent) : QObject(parent){}
    ~BatteryAPI(){}

    enum BatteryState { Normal, PowerSaving };
    Q_ENUM(BatteryState)

    int state(){ return m_state; }
    void setState(int newState){
        if(newState < BatteryState::Normal || newState > BatteryState::PowerSaving){
            throw QException{};
        }
        m_state = newState;
        emit stateChanged(newState);
    }

Q_SIGNALS:
    void stateChanged(int);

private:
    int m_state = (int)BatteryState::Normal;
};

#endif // BATTERYAPI_H
