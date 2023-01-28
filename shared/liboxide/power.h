/*!
 * \file power.h
 */
#ifndef POWER_H
#define POWER_H

#include "liboxide_global.h"

#include <QList>
#include <QString>

#include "sysobject.h"

/*!
 * \brief The Power API
 */
namespace Oxide::Power {
    /*!
     * \brief Get a list of battery objects
     * \return The list of battery objects
     */
    LIBOXIDE_EXPORT const QList<Oxide::SysObject>* batteries();
    /*!
     * \brief Get a list of charger objects
     * \return The list of charger objects
     */
    LIBOXIDE_EXPORT const QList<Oxide::SysObject>* chargers();
    /*!
     * \brief Get the current battery level. This is averaged across the number of batteries connected to the device.
     * \return The current battery level
     */
    LIBOXIDE_EXPORT int batteryLevel();
    /*!
     * \brief Get the current battery temperature
     * \return The current battery temperature. This returns the maximum battery temperature of the batteries connected to the device.
     */
    LIBOXIDE_EXPORT int batteryTemperature();
    /*!
     * \brief Check to see if any battery connected to the device is charging.
     * \return If any battery is charging
     */
    LIBOXIDE_EXPORT bool batteryCharging();
    /*!
     * \brief Check to see if there are any batteries connected to the device.
     * \return If there is a battery connected to the device.
     */
    LIBOXIDE_EXPORT bool batteryPresent();
    /*!
     * \brief Get the list of current warnings being emitted for the batteries connected to the device.
     * \return List of warnings
     */
    LIBOXIDE_EXPORT QList<QString> batteryWarning();
    /*!
     * \brief Get the list of current alerts being emitted for the batteries connected to the device.
     * \return List of alerts
     */
    LIBOXIDE_EXPORT QList<QString> batteryAlert();
    /*!
     * \brief Check to see if there are any battery warnings for any of the batteries connected to the device.
     * \return If there are any current battery warnings
     */
    LIBOXIDE_EXPORT bool batteryHasWarning();
    /*!
     * \brief Check to see if there are any battery alerts for any of the batteries connected to the device.
     * \return If there are any current battery alerts
     */
    LIBOXIDE_EXPORT bool batteryHasAlert();
    /*!
     * \brief Check to see if a charger is connected to the device.
     * \return If a charger is connected
     */
    LIBOXIDE_EXPORT bool chargerConnected();
}

#endif // POWER_H
