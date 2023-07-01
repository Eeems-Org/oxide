/*!
 * \addtogroup Oxide
 * @{
 * \file
 */

#pragma once
/*!
 * \def WPA_SUPPLICANT_SERVICE
 * \brief wpa_supplicant DBus service name
 */
#define WPA_SUPPLICANT_SERVICE "fi.w1.wpa_supplicant1"
/*!
 * \def WPA_SUPPLICANT_SERVICE_PATH
 * \brief wpa_supplicant DBus service path
 */
#define WPA_SUPPLICANT_SERVICE_PATH "/fi/w1/wpa_supplicant1"

/*!
 * \def OXIDE_SERVICE
 * \brief DBus service for tarnish
 */
#define OXIDE_SERVICE "codes.eeems.oxide1"
/*!
 * \def OXIDE_SERVICE_PATH
 * \brief DBus path for tarnish
 */
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"
/*!
 * \def OXIDE_INTERFACE_VERSION
 * \brief Version of Tarnish and liboxide
 */
#define OXIDE_INTERFACE_VERSION "2.7.0"
/*!
 * \def OXIDE_GENERAL_INTERFACE
 * \brief DBus service for the general API
 */
#define OXIDE_GENERAL_INTERFACE OXIDE_SERVICE ".General"
/*!
 * \def OXIDE_APPS_INTERFACE
 * \brief DBus service for the apps API
 */
#define OXIDE_APPS_INTERFACE OXIDE_SERVICE ".Apps"
/*!
 * \def OXIDE_NOTIFICATIONS_INTERFACE
 * \brief DBus service for the notifications API
 */
#define OXIDE_NOTIFICATIONS_INTERFACE OXIDE_SERVICE ".Notifications"
/*!
 * \def OXIDE_POWER_INTERFACE
 * \brief DBus service for the power API
 */
#define OXIDE_POWER_INTERFACE OXIDE_SERVICE ".Power"
/*!
 * \def OXIDE_SCREEN_INTERFACE
 * \brief DBus service for the screen API
 */
#define OXIDE_SCREEN_INTERFACE OXIDE_SERVICE ".Screen"
/*!
 * \def OXIDE_SYSTEM_INTERFACE
 * \brief DBus service for the system API
 */
#define OXIDE_SYSTEM_INTERFACE OXIDE_SERVICE ".System"
/*!
 * \def OXIDE_WIFI_INTERFACE
 * \brief DBus service for the wifi API
 */
#define OXIDE_WIFI_INTERFACE OXIDE_SERVICE ".Wifi"
/*!
 * \def OXIDE_APPLICATION_INTERFACE
 * \brief DBus service for an application object
 */
#define OXIDE_APPLICATION_INTERFACE OXIDE_SERVICE ".Application"
/*!
 * \def OXIDE_BSS_INTERFACE
 * \brief DBus service for a bss object
 */
#define OXIDE_BSS_INTERFACE OXIDE_SERVICE ".BSS"
/*!
 * \def OXIDE_NETWORK_INTERFACE
 * \brief DBus service for a network object
 */
#define OXIDE_NETWORK_INTERFACE OXIDE_SERVICE ".Network"
/*!
 * \def OXIDE_NOTIFICATION_INTERFACE
 * \brief DBus service for a notification object
 */
#define OXIDE_NOTIFICATION_INTERFACE OXIDE_SERVICE ".Notification"
/*!
 * \def OXIDE_SCREENSHOT_INTERFACE
 * \brief DBus service for a screenshot object
 */
#define OXIDE_SCREENSHOT_INTERFACE OXIDE_SERVICE ".Screenshot"
/*! @} */
