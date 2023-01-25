#ifndef META_H
#define META_H

/*!
 * \brief wpa_supplicant DBus service name
 */
#define WPA_SUPPLICANT_SERVICE "fi.w1.wpa_supplicant1"
/*!
 * \brief wpa_supplicant DBus service path
 */
#define WPA_SUPPLICANT_SERVICE_PATH "/fi/w1/wpa_supplicant1"

/*!
 * \brief DBus service for tarnish
 */
#define OXIDE_SERVICE "codes.eeems.oxide1"
/*!
 * \brief DBus path for tarnish
 */
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"
/*!
 * \brief Version of Tarnish and liboxide
 */
#define OXIDE_INTERFACE_VERSION "2.5.0"
/*!
 * \brief DBus service for the general API
 */
#define OXIDE_GENERAL_INTERFACE OXIDE_SERVICE ".General"
/*!
 * \brief DBus service for the apps API
 */
#define OXIDE_APPS_INTERFACE OXIDE_SERVICE ".Apps"
/*!
 * \brief DBus service for the notifications API
 */
#define OXIDE_NOTIFICATIONS_INTERFACE OXIDE_SERVICE ".Notifications"
/*!
 * \brief DBus service for the power API
 */
#define OXIDE_POWER_INTERFACE OXIDE_SERVICE ".Power"
/*!
 * \brief DBus service for the screen API
 */
#define OXIDE_SCREEN_INTERFACE OXIDE_SERVICE ".Screen"
/*!
 * \brief DBus service for the system API
 */
#define OXIDE_SYSTEM_INTERFACE OXIDE_SERVICE ".System"
/*!
 * \brief DBus service for the wifi API
 */
#define OXIDE_WIFI_INTERFACE OXIDE_SERVICE ".Wifi"
/*!
 * \brief DBus service for an application object
 */
#define OXIDE_APPLICATION_INTERFACE OXIDE_SERVICE ".Application"
/*!
 * \brief DBus service for a bss object
 */
#define OXIDE_BSS_INTERFACE OXIDE_SERVICE ".BSS"
/*!
 * \brief DBus service for a network object
 */
#define OXIDE_NETWORK_INTERFACE OXIDE_SERVICE ".Network"
/*!
 * \brief DBus service for a notification object
 */
#define OXIDE_NOTIFICATION_INTERFACE OXIDE_SERVICE ".Notification"
/*!
 * \brief DBus service for a screenshot object
 */
#define OXIDE_SCREENSHOT_INTERFACE OXIDE_SERVICE ".Screenshot"

#endif // META_H
