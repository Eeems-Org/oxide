/*!
 * \addtogroup Oxide
 * \brief The xochitl setttings class
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include "settingsfile.h"

/*!
 * \def xochitlSettings()
 * \brief Get the Oxide::XochitlSettings instance
 */
#define xochitlSettings Oxide::XochitlSettings::instance()
/*!
 * \brief Wifi Network definition
 */
typedef QMap<QString, QVariantMap> WifiNetworks;
Q_DECLARE_METATYPE(WifiNetworks);

namespace Oxide{
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
}
/*! @} */
