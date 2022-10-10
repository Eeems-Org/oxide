#ifndef POWER_H
#define POWER_H

#include "liboxide_global.h"
#include "sysobject.h"

namespace Oxide::Power {
    LIBOXIDE_EXPORT const QList<SysObject>* batteries();
    LIBOXIDE_EXPORT const QList<SysObject>* chargers();
}

#endif // POWER_H
