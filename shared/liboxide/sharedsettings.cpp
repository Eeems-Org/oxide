#include "sharedsettings.h"

namespace Oxide{
    SharedSettings::~SharedSettings(){}
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, version)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, firstLaunch, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, telemetry, false)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, applicationUsage, false)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, crashReport, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, lockOnSuspend, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, autoSleep, 5)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, autoLock, 5)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, pin)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, onLogin)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, onFailedLogin)
}
