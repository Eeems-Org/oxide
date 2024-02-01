/*!
 * \addtogroup Oxide
 * \brief The DeviceSettings class
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include "event_device.h"
#include <QFileSystemWatcher>

/*!
 * \def deviceSettings()
 * \brief Get the Oxide::DeviceSettings instance
 */
#define deviceSettings Oxide::DeviceSettings::instance()

namespace Oxide{
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
        /*!
         * \brief Get the list of possible locales on the device
         * \return The list of possible locales on the device
         */
        const QStringList getLocales();
        /*!
         * \brief Get the current set locale
         * \return The current locale
         */
        QString getLocale();
        /*!
         * \brief Set the current locale
         * \param locale Locale to set
         */
        void setLocale(const QString& locale);
        /*!
         * \brief Get the list of possible timezones on the device
         * \return The list of possible timezones on the device
         */
        const QStringList getTimezones();
        /*!
         * \brief Get the current set timezone
         * \return The current timezone
         */
        QString getTimezone();
        /*!
         * \brief Set the current timezone
         * \param locale Timezone to set
         */
        void setTimezone(const QString& timezone);
        /*!
         * \brief Setup the Qt environment
         * \snippet examples/oxide.cpp setupQtEnvironment
         */
        void setupQtEnvironment(bool touch = true);
        /*!
         * \brief Check if a keyboard is attached
         * \return If a keyboard is attached
         */
        bool keyboardAttached();
        /*!
         * \brief Run a callback when keyboardAttached changes
         * \param callback Callback to run
         */
        void onKeyboardAttachedChanged(std::function<void()> callback);
        /*!
         * \brief Get the list of all evdev devices
         * \return All input devices
         */
        QList<event_device> inputDevices();
        /*!
         * \brief Run a callback when inputDevices changes
         * \param callback callback to run
         */
        void onInputDevicesChanged(std::function<void()> callback);
        /*!
         * \brief Get the list of all keyboard evdev devices
         * \return All keyboard devices
         */
        QList<event_device> keyboards();
        /*!
         * \brief Get the list of all physical keyboard evdev devices
         * \return All physical keyboard devices
         */
        QList<event_device> physicalKeyboards();
        /*!
         * \brief Get the list of all virtual keyboard evdev devices
         * \return All virtual keyboard devices
         */
        QList<event_device> virtualKeyboards();

    private:
        DeviceType _deviceType;

        DeviceSettings();
        ~DeviceSettings();
        void readDeviceType();
        bool checkBitSet(int fd, int type, int i);
        std::string buttonsPath = "";
        std::string wacomPath = "";
        std::string touchPath = "";
        QFileSystemWatcher* watcher = nullptr;
    };
}
/*! @} */
