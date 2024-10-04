#include "powerapi.h"

#include <liboxide/udev.h>

PowerAPI* PowerAPI::singleton(PowerAPI* self){
    static PowerAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}

PowerAPI::PowerAPI(QObject* parent)
: APIBase(parent), m_chargerState(ChargerUnknown){
    Oxide::Sentry::sentry_transaction("Power API Init", "init", [this](Oxide::Sentry::Transaction* t){
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
                Oxide::UDev::singleton()->addMonitor("platform", NULL);
                Oxide::UDev::singleton()->subsystem("power_supply", [this]{
                    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
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

PowerAPI::~PowerAPI(){
    if(timer != nullptr){
            qDebug() << "Killing timer";
            timer->stop();
            delete timer;
        }else{
            qDebug() << "Killing UDev monitor";
            Oxide::UDev::singleton()->stop();
        }
}

void PowerAPI::setEnabled(bool enabled){
    if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM1){
        if(enabled){
            Oxide::UDev::singleton()->start();
        }else{
            Oxide::UDev::singleton()->stop();
        }
        return;
    }
    if(enabled){
        timer->start();
    }else{
        timer->stop();
    }
}

int PowerAPI::state(){
    if(!hasPermission("power")){
        return 0;
    }
    return m_state;
}

void PowerAPI::setState(int state){
    if(!hasPermission("power")){
        return;
    }
    if(state < State::Normal || state > State::PowerSaving){
        throw QException{};
    }
    m_state = state;
    emit stateChanged(state);
}

int PowerAPI::batteryState() {
    if(!hasPermission("power")){
        return BatteryUnknown;
    }
    return m_batteryState;
}

void PowerAPI::setBatteryState(int batteryState){
    if(!hasPermission("power")){
        return;
    }
    m_batteryState = batteryState;
    emit batteryStateChanged(batteryState);
}

int PowerAPI::batteryLevel() {
    if(!hasPermission("power")){
        return 0;
    }
    return m_batteryLevel;
}

void PowerAPI::setBatteryLevel(int batteryLevel){
    m_batteryLevel = batteryLevel;
    emit batteryLevelChanged(batteryLevel);
}

int PowerAPI::batteryTemperature() {
    if(!hasPermission("power")){
        return 0;
    }
    return m_batteryTemperature;
}

void PowerAPI::setBatteryTemperature(int batteryTemperature){
    m_batteryTemperature = batteryTemperature;
    emit batteryTemperatureChanged(batteryTemperature);
}

int PowerAPI::chargerState() {
    if(!hasPermission("power")){
        return ChargerUnknown;
    }
    return m_chargerState;
}

void PowerAPI::setChargerState(int chargerState){
    m_chargerState = chargerState;
    emit chargerStateChanged(chargerState);
}

void PowerAPI::updateBattery(){
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

void PowerAPI::updateCharger(){
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

void PowerAPI::update(){
    updateBattery();
    updateCharger();
}
