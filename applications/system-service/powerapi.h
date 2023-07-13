#ifndef BATTERYAPI_H
#define BATTERYAPI_H

#include <QTimer>

#include "apibase.h"

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
    static PowerAPI* singleton(PowerAPI* self = nullptr);
    PowerAPI(QObject* parent);
    ~PowerAPI();

    void setEnabled(bool enabled) override;
    bool isEnabled();

    enum State { Normal, PowerSaving };
    Q_ENUM(State)
    enum BatteryState { BatteryUnknown, BatteryCharging, BatteryDischarging, BatteryNotPresent };
    Q_ENUM(BatteryState)
    enum ChargerState { ChargerUnknown, ChargerConnected, ChargerNotConnected };
    Q_ENUM(ChargerState)

    int state();
    void setState(int state);

    int batteryState();
    void setBatteryState(int batteryState);

    int batteryLevel();
    void setBatteryLevel(int batteryLevel);

    int batteryTemperature();
    void setBatteryTemperature(int batteryTemperature);

    int chargerState();
    void setChargerState(int chargerState);

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
    QTimer* timer;
    int m_state = Normal;
    int m_batteryState = BatteryUnknown;
    int m_batteryLevel = 0;
    int m_batteryTemperature = 0;
    int m_chargerState;
    bool m_batteryWarning = false;
    bool m_batteryAlert = false;
    bool m_chargerWarning = false;
    bool m_enabled;

    void updateBattery();
    void updateCharger();

private slots:
    void update();
};

#endif // BATTERYAPI_H
