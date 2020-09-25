#ifndef DBUSSETTINGS_H
#define DBUSSETTINGS_H

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

#endif // DBUSSETTINGS_H
