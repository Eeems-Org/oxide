/*!
 * \file liboxide.h
 */
#ifndef LIBOXIDE_H
#define LIBOXIDE_H

#include "liboxide_global.h"

#include "settingsfile.h"
#include "power.h"
#include "signalhandler.h"

#include <QDebug>
#include <QScopeGuard>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QThread>

#define WPA_SUPPLICANT_SERVICE "fi.w1.wpa_supplicant1"
#define WPA_SUPPLICANT_SERVICE_PATH "/fi/w1/wpa_supplicant1"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"
#define OXIDE_INTERFACE_VERSION "1.0.0"

#define OXIDE_GENERAL_INTERFACE OXIDE_SERVICE ".General"
#define OXIDE_POWER_INTERFACE OXIDE_SERVICE ".Power"
#define OXIDE_WIFI_INTERFACE OXIDE_SERVICE ".Wifi"
#define OXIDE_NETWORK_INTERFACE OXIDE_SERVICE ".Network"
#define OXIDE_BSS_INTERFACE OXIDE_SERVICE ".BSS"
#define OXIDE_APPS_INTERFACE OXIDE_SERVICE ".Apps"
#define OXIDE_APPLICATION_INTERFACE OXIDE_SERVICE ".Application"
#define OXIDE_SYSTEM_INTERFACE OXIDE_SERVICE ".System"
#define OXIDE_SCREEN_INTERFACE OXIDE_SERVICE ".Screen"
#define OXIDE_NOTIFICATIONS_INTERFACE OXIDE_SERVICE ".Notifications"
#define OXIDE_NOTIFICATION_INTERFACE OXIDE_SERVICE ".Notification"
#define OXIDE_SCREENSHOT_INTERFACE OXIDE_SERVICE ".Screenshot"

/*!
 * \def deviceSettings()
 * \brief Get the Oxide::DeviceSettings instance
 */
#define deviceSettings Oxide::DeviceSettings::instance()
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

#ifdef SENTRY
#include <sentry.h>
#include <sentry/src/sentry_tracing.h>
#endif

/*!
 * \brief Wifi Network definition
 */
typedef QMap<QString, QVariantMap> WifiNetworks;
Q_DECLARE_METATYPE(WifiNetworks);
/*!
 * \addtogroup Oxide
 * \brief The main Oxide namespace
 * @{
 */
/*!
 * \brief The main Oxide namespace
 */
namespace Oxide {
    /*!
     * \brief Run code on the main Qt thread
     * \param callback The code to run on the main thread
     *
     * \snippet examples/oxide.cpp dispatchToMainThread
     */
    LIBOXIDE_EXPORT void dispatchToMainThread(std::function<void()> callback);
    namespace Sentry{
        struct Transaction {
#ifdef SENTRY
            /*!
             * \brief The sentry_transaction_t instance
             */
            sentry_transaction_t* inner;
            explicit Transaction(sentry_transaction_t* t);
#else
            void* inner;
            explicit Transaction(void* t);
#endif
        };
        struct Span {
#ifdef SENTRY
            /*!
             * \brief The sentry_span_t instance
             */
            sentry_span_t* inner;
            explicit Span(sentry_span_t* s);
#else
            void* inner;
            explicit Span(void* s);
#endif
        };

        LIBOXIDE_EXPORT const char* bootId();
        LIBOXIDE_EXPORT const char* machineId();
        LIBOXIDE_EXPORT void sentry_init(const char* name, char* argv[], bool autoSessionTracking = true);
        LIBOXIDE_EXPORT void sentry_breadcrumb(const char* category, const char* message, const char* type = "default", const char* level = "info");
        LIBOXIDE_EXPORT Transaction* start_transaction(const std::string& name, const std::string& action);
        LIBOXIDE_EXPORT void stop_transaction(Transaction* transaction);
        LIBOXIDE_EXPORT void sentry_transaction(const std::string& name, const std::string& action, std::function<void(Transaction* transaction)> callback);
        LIBOXIDE_EXPORT Span* start_span(Transaction* transaction, const std::string& operation, const std::string& description);
        LIBOXIDE_EXPORT Span* start_span(Span* parent, const std::string& operation, const std::string& description);
        LIBOXIDE_EXPORT void stop_span(Span* span);
        LIBOXIDE_EXPORT void sentry_span(Transaction* transaction, const std::string& operation, const std::string& description, std::function<void()> callback);
        LIBOXIDE_EXPORT void sentry_span(Transaction* transaction, const std::string& operation, const std::string& description, std::function<void(Span* span)> callback);
        LIBOXIDE_EXPORT void sentry_span(Span* parent, const std::string& operation, const std::string& description, std::function<void()> callback);
        LIBOXIDE_EXPORT void sentry_span(Span* parent, const std::string& operation, const std::string& description, std::function<void(Span* span)> callback);
        LIBOXIDE_EXPORT void trigger_crash();
    }
    /*!
     * \brief Device specific values
     */
    class LIBOXIDE_EXPORT DeviceSettings{
    public:
        /*!
         * \brief Known device types
         */
        enum DeviceType {
            Unknown, /*!< Unknown device type >*/
            RM1, /*!< reMarkable 1 >*/
            RM2 /*!< reMarkable 2 >*/
        };
        /*!
         * \brief Get the static instance of this class. You should use the deviceSettings macro instead.
         * \return The static instance
         * \sa deviceSettings
         */
        static DeviceSettings& instance();
        /*!
         * \brief Get the path to the buttons input device
         * \return Path to the buttons device
         */
        const char* getButtonsDevicePath() const;
        /*!
         * \brief Get the path to the wacom input device
         * \return Path to the wacom device
         */
        const char* getWacomDevicePath() const;
        /*!
         * \brief Get the path to the touch input device
         * \return Path to the touch device
         */
        const char* getTouchDevicePath() const;
        /*!
         * \brief Get the Qt environment settings for the device
         * \return The Qt environment settings for the device
         */
        const char* getTouchEnvSetting() const;
        /*!
         * \brief Get the device type
         * \return The device type
         */
        DeviceType getDeviceType() const;
        /*!
         * \brief Get the human readable device name
         * \return Human readable device name
         */
        const char* getDeviceName() const;
        /*!
         * \brief Get the max width for touch input on the device
         * \return Max width for touch input
         */
        int getTouchWidth() const;
        /*!
         * \brief Get the max height for touch input on the device
         * \return Max height for touch input
         */
        int getTouchHeight() const;

    private:
        DeviceType _deviceType;

        DeviceSettings();
        ~DeviceSettings() {};
        void readDeviceType();
        bool checkBitSet(int fd, int type, int i);
        std::string buttonsPath = "";
        std::string wacomPath = "";
        std::string touchPath = "";
    };
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
        O_SETTINGS_PROPERTY(int, General, version)
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
         * \brief Enable or disable crash  reporting
         */
        /*!
         * \fn crashReportChanged
         * \brief If crash reporting has been enabled or disabled
         */
        O_SETTINGS_PROPERTY(bool, General, crashReport, true)

    private:
        ~SharedSettings();
    };
}
/*! @} */
#endif // LIBOXIDE_H
