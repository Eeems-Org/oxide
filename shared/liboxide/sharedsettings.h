/*!
 * \addtogroup Oxide
 * \brief The shared settings class
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include "settingsfile.h"

/*!
 * \def sharedSettings()
 * \brief Get the Oxide::SharedSettings instance
 */
#define sharedSettings Oxide::SharedSettings::instance()

namespace Oxide{
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
