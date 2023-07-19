/*!
 * \addtogroup Oxide
 * \brief The main Oxide module
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include "meta.h"
#include "dbus.h"
#include "applications.h"
#include "settingsfile.h"
#include "power.h"
#include "json.h"
#include "signalhandler.h"
#include "slothandler.h"
#include "sysobject.h"
#include "debug.h"
#include "devicesettings.h"
#include "oxide_sentry.h"
#include "threading.h"

#include <QDebug>
#include <QScopeGuard>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QThread>

#include <sys/types.h>
#ifndef VERSION
#ifdef APP_VERSION
#define VERSION APP_VERSION
#else
#define VERSION "2.7"
#endif
#endif
#ifndef APP_VERSION
#define APP_VERSION VERSION
#endif
/*!
 * \def xochitlSettings()
 * \brief Get the Oxide::XochitlSettings instance
 */
#define xochitlSettings Oxide::XochitlSettings::instance()
/*!
 * \def sharedSettings()
 * \brief Get the Oxide::SharedSettings instance
 */
#define sharedSettings Oxide::SharedSettings::instance()

/*!
 * \brief Wifi Network definition
 */
typedef QMap<QString, QVariantMap> WifiNetworks;
Q_DECLARE_METATYPE(WifiNetworks);
/*!
 * \brief The main Oxide namespace
 */
namespace Oxide {
    /*!
     * \brief Try to get a lock
     * \param lockName Path to the lock file
     * \return File descriptor of the lock file
     * \retval -1 Unable to get lock
     */
    LIBOXIDE_EXPORT int tryGetLock(char const *lockName);
    /*!
     * \brief Release a lock file
     * \param fd File descriptor of the lock file
     * \param lockName Path to the lock file
     */
    LIBOXIDE_EXPORT void releaseLock(int fd, char const* lockName);
    /*!
     * \brief Checks to see if a process exists
     * \return If the process exists
     */
    LIBOXIDE_EXPORT bool processExists(pid_t pid);
    /*!
     * \brief Get list of pids that have a file open
     * \param path File to check
     * \return list of pids that have the file open
     */
    LIBOXIDE_EXPORT QList<int> lsof(const QString& path);
    /*!
     * \brief Get the UID for a username
     * \param name Username to search for
     * \throws std::runtime_error Failed to get the UID for the username
     * \return The UID for the username
     * \snippet examples/oxide.cpp getUID
     */
    LIBOXIDE_EXPORT uid_t getUID(const QString& name);
    /*!
     * \brief Get the GID for a groupname
     * \param name Groupname to search for
     * \throws std::runtime_error Failed to get the GID for the groupname
     * \return The GID for the groupname
     * \snippet examples/oxide.cpp getGID
     */
    LIBOXIDE_EXPORT gid_t getGID(const QString& name);

    /*!
     * \brief Manage Xochitl settings
     */
    class LIBOXIDE_EXPORT XochitlSettings : public SettingsFile {
        Q_OBJECT
        /*!
         * \fn instance
         * \brief Get the static instance of this class. You should use the xochitlSettings macro instead.
         * \return The static instance
         * \sa xochitlSettings
         */
        // cppcheck-suppress uninitMemberVarPrivate
        // cppcheck-suppress unusedFunction
        O_SETTINGS(XochitlSettings, "/home/root/.config/remarkable/xochitl.conf")
        /*!
         * \property passcode
         * \brief The passcode used to unlock the device
         * \sa set_passcode, passcodeChanged
         */
        /*!
         * \fn set_passcode
         * \param _arg_passcode The passcode used to unlock the device
         * \brief Set the passcode used to unlock the device
         */
        /*!
         * \fn passcodeChanged
         * \brief The passcode used to unlock the device has changed
         */
        O_SETTINGS_PROPERTY(QString, General, passcode)
        /*!
         * \property wifion
         * \brief If wifi is on or off
         * \sa set_wifion, wifionChanged
         */
        /*!
         * \fn set_wifion
         * \param _arg_wifion If wifi should be on or off
         * \brief Turn wifi on or off
         */
        /*!
         * \fn wifionChanged
         * \brief Wifi has been turned on or off
         */
        O_SETTINGS_PROPERTY(bool, General, wifion)
        /*!
         * \property XochitlSettings::wifinetworks
         * \brief List of wifi networks
         * \sa setWifinetworks, wifinetworksChanged
         */
        Q_PROPERTY(WifiNetworks wifinetworks MEMBER m_wifinetworks READ wifinetworks WRITE setWifinetworks RESET resetWifinetworks NOTIFY wifinetworksChanged)

    public:
        WifiNetworks wifinetworks();
        /*!
         * \brief Set the list of wifi networks
         * \param wifinetworks List of wifi networks to replace with
         */
        void setWifinetworks(const WifiNetworks& wifinetworks);
        /*!
         * \brief Get a specific wifi network
         * \param name SSID of the wifi network
         * \return The wifi network properties
         */
        QVariantMap getWifiNetwork(const QString& name);
        /*!
         * \brief Set the properties for a specific wifi network
         * \param name SSID of the wifi network
         * \param properties The wifi network properties
         */
        void setWifiNetwork(const QString& name, QVariantMap properties);
        void resetWifinetworks();

    signals:
        /*!
         * \brief The contents of the wifi network list has changed
         */
        void wifinetworksChanged(WifiNetworks);

    private:
        ~XochitlSettings();
        WifiNetworks m_wifinetworks;
    };
    /*!
     * \brief Shared settings for Oxide
     */
    class LIBOXIDE_EXPORT SharedSettings : public SettingsFile {
        Q_OBJECT
        /*!
         * \fn instance
         * \brief Get the static instance of this class. You should use the sharedSettings macro instead.
         * \return The static instance
         * \sa sharedSettings
         */
        // cppcheck-suppress uninitMemberVarPrivate
        O_SETTINGS(SharedSettings, "/home/root/.config/Eeems/shared.conf")
        /*!
         * \property version
         * \brief Current version of the settings file
         * \sa set_version, versionChanged
         */
        /*!
         * \fn versionChanged
         * \brief If the version number has changed
         */
        O_SETTINGS_PROPERTY(int, General, version)
        /*!
         * \property firstLaunch
         * \brief If this is the first time that things have been run
         * \sa set_firstLaunch, firstLaunchChanged
         */
        /*!
         * \fn set_firstLaunch
         * \param _arg_firstLaunch
         * \brief Change the state of firstLaunch
         */
        /*!
         * \fn firstLaunchChanged
         * \brief If firstLaunch has changed
         */
        O_SETTINGS_PROPERTY(bool, General, firstLaunch, true)
        /*!
         * \property telemetry
         * \brief If telemetry reporting is enabled or not
         * \sa set_telemetry, telemetryChanged
         */
        /*!
         * \fn set_telemetry
         * \param _arg_telemetry
         * \brief Enable or disable telemetry reporting
         */
        /*!
         * \fn telemetryChanged
         * \brief If telemetry reporting has been enabled or disabled
         */
        O_SETTINGS_PROPERTY(bool, General, telemetry, false)
        /*!
         * \property applicationUsage
         * \brief If application usage reporting is enabled or not
         * \sa set_applicationUsage, applicationUsageChanged
         */
        /*!
         * \fn set_applicationUsage
         * \param _arg_applicationUsage
         * \brief Enable or disable application usage reporting
         */
        /*!
         * \fn applicationUsageChanged
         * \brief If application usage reporting has been enabled or disabled
         */
        O_SETTINGS_PROPERTY(bool, General, applicationUsage, false)
        /*!
         * \property crashReport
         * \brief If crash reporting is enabled or not
         * \sa set_crashReport, crashReportChanged
         */
        /*!
         * \fn set_crashReport
         * \param _arg_crashReport
         * \brief Enable or disable crash reporting
         */
        /*!
         * \fn crashReportChanged
         * \brief If crash reporting has been enabled or disabled
         */
        O_SETTINGS_PROPERTY(bool, General, crashReport, true)
        /*!
         * \property lockOnSuspend
         * \brief If the device should lock on suspend or not
         * \sa set_lockOnSuspend, lockOnSuspendChanged
         */
        /*!
         * \fn set_lockOnSuspend
         * \param _arg_lockOnSuspend
         * \brief Enable or disable locking on suspend
         */
        /*!
         * \fn lockOnSuspendChanged
         * \brief If lock on suspend has been enabled or disabled
         */
        O_SETTINGS_PROPERTY(bool, General, lockOnSuspend, true)
        /*!
         * \property autoSleep
         * \brief How long without activity before the device should suspend
         * \sa set_autoSleep, autoSleepChanged
         */
        /*!
         * \fn set_autoSleep
         * \param _arg_autoSleep
         * \brief Change autoSleep
         */
        /*!
         * \fn autoSleepChanged
         * \brief If autoSleep has been changed
         */
        O_SETTINGS_PROPERTY(int, General, autoSleep, 5)
        /*!
         * \property autoLock
         * \brief How long without activity before the device should suspend
         * \sa set_autoLock, autoLockChanged
         */
        /*!
         * \fn set_autoLock
         * \param _arg_autoLock
         * \brief Change autoLock
         */
        /*!
         * \fn autoLockChanged
         * \brief If autoLock has been changed
         */
        O_SETTINGS_PROPERTY(int, General, autoLock, 5)
        /*!
         * \property pin
         * \brief The lockscreen pin
         * \sa set_pin, pinChanged
         */
        /*!
         * \fn set_pin
         * \param _arg_pin
         * \brief Change lockscreen pin
         */
        /*!
         * \fn has_pin
         * \brief Change lockscreen pin
         * \return If the lockscreen pin is set
         */
        /*!
         * \fn pinChanged
         * \brief If the lockscreen pin has been changed
         */
        O_SETTINGS_PROPERTY(QString, Lockscreen, pin)
        /*!
         * \property onLogin
         * \brief The lockscreen onLogin
         * \sa set_onLogin, onLoginChanged
         */
        /*!
         * \fn set_onLogin
         * \param _arg_onLogin
         * \brief Change lockscreen onLogin
         */
        /*!
         * \fn has_onLogin
         * \brief If lockscreen onLogin has been set
         * \return If the lockscreen onLogin is set
         */
        /*!
         * \fn onLoginChanged
         * \brief If the lockscreen onLogin has been changed
         */
        O_SETTINGS_PROPERTY(QString, Lockscreen, onLogin)
        /*!
         * \property onFailedLogin
         * \brief The lockscreen onFailedLogin
         * \sa set_onFailedLogin, onFailedLoginChanged
         */
        /*!
         * \fn set_onFailedLogin
         * \param _arg_onFailedLogin
         * \brief Change lockscreen onFailedLogin
         */
        /*!
         * \fn has_onFailedLogin
         * \brief If lockscreen onFailedLogin has been set
         * \return If the lockscreen onFailedLogin is set
         */
        /*!
         * \fn onFailedLoginChanged
         * \brief If the lockscreen onFailedLogin has been changed
         */
        O_SETTINGS_PROPERTY(QString, Lockscreen, onFailedLogin)

    private:
        ~SharedSettings();
    };
}
/*! @} */
