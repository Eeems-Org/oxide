#ifndef LIBOXIDE_H
#define LIBOXIDE_H

#include "liboxide_global.h"

#include <sentry.h>
#include <QDebug>
#include <QScopeGuard>
#include <QSettings>
#include <QFileSystemWatcher>

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

namespace Oxide {
    namespace Sentry{
        LIBOXIDE_EXPORT void sentry_init(const char* name, char* argv[]);
        LIBOXIDE_EXPORT void sentry_breadcrumb(const char* category, const char* message, const char* type = "default", const char* level = "info");
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

    class LIBOXIDE_EXPORT XochitlSettings : public QSettings {
        Q_OBJECT
        Q_PROPERTY(QString passcode MEMBER m_passcode READ passcode WRITE setPasscode NOTIFY passcodeChanged)
        Q_PROPERTY(QMap<QString, QVariantMap> wifinetworks MEMBER m_wifinetworks READ wifinetworks WRITE setWifinetworks NOTIFY wifinetworksChanged)
        Q_PROPERTY(bool wifion MEMBER m_wifion READ wifion WRITE setWifion NOTIFY wifionChanged)
    public:
        static XochitlSettings& instance();
        QString passcode();
        void setPasscode(const QString&);
        QMap<QString, QVariantMap> wifinetworks();
        void setWifinetworks(const QMap<QString, QVariantMap>&);
        QVariantMap getWifiNetwork(const QString& name);
        QVariantMap setWifiNetwork(const QString& name, QVariantMap properties);
        bool wifion();
        void setWifion(bool);
    signals:
        void passcodeChanged(const QString&);
        void wifinetworksChanged(const QMap<QString, QVariantMap>&);
        void wifionChanged(bool);
    private:
        XochitlSettings();
        ~XochitlSettings();
        QFileSystemWatcher fileWatcher;
        QString m_passcode;
        QMap<QString, QVariantMap> m_wifinetworks;
        bool m_wifion;
    };
}

#endif // LIBOXIDE_H
