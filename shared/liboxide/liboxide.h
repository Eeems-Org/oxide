#ifndef LIBOXIDE_H
#define LIBOXIDE_H

#include "liboxide_global.h"

#include "settingsfile.h"
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

#define deviceSettings Oxide::DeviceSettings::instance()
#define xochitlSettings Oxide::XochitlSettings::instance()
#define sharedSettings Oxide::SharedSettings::instance()

#ifdef SENTRY
#include <sentry.h>
#include <sentry/src/sentry_tracing.h>
#endif

typedef QMap<QString, QVariantMap> WifiNetworks;
Q_DECLARE_METATYPE(WifiNetworks);

namespace Oxide {
    LIBOXIDE_EXPORT void dispatchToMainThread(std::function<void()> callback);
    namespace Sentry{
        struct Transaction {
#ifdef SENTRY
            sentry_transaction_t* inner;
            explicit Transaction(sentry_transaction_t* t);
#else
            void* inner;
            explicit Transaction(void* t);
#endif
        };
        struct Span {
#ifdef SENTRY
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
        LIBOXIDE_EXPORT Transaction* start_transaction(std::string name, std::string action);
        LIBOXIDE_EXPORT void stop_transaction(Transaction* transaction);
        LIBOXIDE_EXPORT void sentry_transaction(std::string name, std::string action, std::function<void(Transaction* transaction)> callback);
        LIBOXIDE_EXPORT Span* start_span(Transaction* transaction, std::string operation, std::string description);
        LIBOXIDE_EXPORT Span* start_span(Span* parent, std::string operation, std::string description);
        LIBOXIDE_EXPORT void stop_span(Span* span);
        LIBOXIDE_EXPORT void sentry_span(Transaction* transaction, std::string operation, std::string description, std::function<void()> callback);
        LIBOXIDE_EXPORT void sentry_span(Transaction* transaction, std::string operation, std::string description, std::function<void(Span* span)> callback);
        LIBOXIDE_EXPORT void sentry_span(Span* parent, std::string operation, std::string description, std::function<void()> callback);
        LIBOXIDE_EXPORT void sentry_span(Span* parent, std::string operation, std::string description, std::function<void(Span* span)> callback);
        LIBOXIDE_EXPORT void trigger_crash();
    }
    class LIBOXIDE_EXPORT DeviceSettings{
    public:
        enum DeviceType { Unknown, RM1, RM2 };
        static DeviceSettings& instance();
        const char* getButtonsDevicePath() const;
        const char* getWacomDevicePath() const;
        const char* getTouchDevicePath() const;
        const char* getTouchEnvSetting() const;
        DeviceType getDeviceType() const;
        int getTouchWidth() const;
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

    class LIBOXIDE_EXPORT XochitlSettings : public SettingsFile {
        Q_OBJECT
        // cppcheck-suppress uninitMemberVarPrivate
        // cppcheck-suppress unusedFunction
        O_SETTINGS(XochitlSettings, "/home/root/.config/remarkable/xochitl.conf")
        O_SETTINGS_PROPERTY(QString, General, passcode)
        O_SETTINGS_PROPERTY(bool, General, wifion)
        Q_PROPERTY(WifiNetworks wifinetworks MEMBER m_wifinetworks READ wifinetworks WRITE setWifinetworks RESET resetWifinetworks NOTIFY wifinetworksChanged)
    public:
        WifiNetworks wifinetworks();
        void setWifinetworks(const WifiNetworks& wifinetworks);
        QVariantMap getWifiNetwork(const QString& name);
        void setWifiNetwork(const QString& name, QVariantMap properties);
        void resetWifinetworks();
    signals:
        void wifinetworksChanged(WifiNetworks);
    private:
        ~XochitlSettings();
        WifiNetworks m_wifinetworks;
    };

    class LIBOXIDE_EXPORT SharedSettings : public SettingsFile {
        Q_OBJECT
        // cppcheck-suppress uninitMemberVarPrivate
        O_SETTINGS(SharedSettings, "/home/root/.config/Eeems/shared.conf")
        O_SETTINGS_PROPERTY(int, General, version)
        O_SETTINGS_PROPERTY(bool, General, firstLaunch, true)
        O_SETTINGS_PROPERTY(bool, General, telemetry, false)
        O_SETTINGS_PROPERTY(bool, General, applicationUsage, false)
        O_SETTINGS_PROPERTY(bool, General, crashReport, true)
    private:
        ~SharedSettings();
    };
}

#endif // LIBOXIDE_H
