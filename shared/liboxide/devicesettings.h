/*!
 * \addtogroup Oxide
 * \brief Device settings
 * @{
 * \file
 */
#pragma once

/*!
 * \def deviceSettings()
 * \brief Get the Oxide::DeviceSettings instance
 */
#define deviceSettings Oxide::DeviceSettings::instance()

namespace Oxide{
    /*!
     * \brief Execute a program and return it's output
     * \param program Program to run
     * \param args Arguments to pass to the program
     * \return Output if it ran.
     * \retval NULL Program was not able to execute
     */
    LIBOXIDE_EXPORT QString execute(const QString& program, const QStringList& args);
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
         * \brief getTouchPressure
         * \return
         */
        int getTouchPressure() const;
        /*!
         * \brief supportsMultiTouch
         * \return
         */
        bool supportsMultiTouch() const;
        /*!
         * \brief isTouchTypeB
         * \return
         */
        bool isTouchTypeB() const;
        /*!
         * \brief getTouchSlots
         * \return
         */
        int getTouchSlots() const;
        /*!
         * \brief getWacomWidth
         * \return
         */
        int getWacomWidth() const;
        /*!
         * \brief getWacomHeight
         * \return
         */
        int getWacomHeight() const;
        /*!
         * \brief getWacomPressure
         * \return
         */
        int getWacomPressure() const;
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
         * \brief The QtEnvironmentType enum
         */
        enum QtEnvironmentType{
            Default, /*!< Default, standalone Qt environment >*/
            NoPen, /*!< Standalone Qt environment with no pen support >*/
            Oxide /*!< Oxide environment >*/
        };
        /*!
         * \brief Setup the Qt environment
         * \snippet examples/oxide.cpp setupQtEnvironment
         */
        void setupQtEnvironment(QtEnvironmentType type = Default);
        /*!
         * \brief screenGeometry
         * \return
         */
        QRect screenGeometry();

    private:
        DeviceType _deviceType;

        DeviceSettings();
        ~DeviceSettings();
        void readDeviceType();
        bool checkBitSet(int fd, int type, int i);
        std::string buttonsPath = "";
        std::string wacomPath = "";
        std::string touchPath = "";
    };
}
