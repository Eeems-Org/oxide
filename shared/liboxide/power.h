#ifndef POWER_H
#define POWER_H

#include "liboxide_global.h"

#include <string>

#include "sysobject.h"

namespace Oxide::Power {
    LIBOXIDE_EXPORT const QList<SysObject>* batteries();
    LIBOXIDE_EXPORT const QList<SysObject>* chargers();
    LIBOXIDE_EXPORT int batteryLevel();
    LIBOXIDE_EXPORT int batteryTemperature();
    LIBOXIDE_EXPORT bool batteryCharging();
    LIBOXIDE_EXPORT bool batteryPresent();
    LIBOXIDE_EXPORT std::string batteryWarning();
    LIBOXIDE_EXPORT std::string batteryAlert();
    LIBOXIDE_EXPORT bool batteryHasWarning();
    LIBOXIDE_EXPORT bool batteryHasAlert();
    LIBOXIDE_EXPORT bool chargerConnected();
}

#endif // POWER_H
