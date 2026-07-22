#pragma once

#include <liboxide.h>

#include <QObject>

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

using namespace codes::eeems::oxide1;
using codes::eeems::oxide1::Power;
using namespace Oxide::Sentry;

enum BatteryState {
  BatteryUnknown,
  BatteryCharging,
  BatteryDischarging,
  BatteryNotPresent
};
enum ChargerState {
  ChargerUnknown,
  ChargerConnected,
  ChargerNotConnected,
  ChargerNotPresent
};
enum WifiState {
  WifiUnknown,
  WifiOff,
  WifiDisconnected,
  WifiOffline,
  WifiOnline
};

class Controller : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
  Q_PROPERTY(bool showWifiDb READ showWifiDb NOTIFY showWifiDbChanged)
  Q_PROPERTY(
    bool showBatteryPercent READ showBatteryPercent NOTIFY
      showBatteryPercentChanged
  )
  Q_PROPERTY(
    bool showBatteryTemperature READ showBatteryTemperature NOTIFY
      showBatteryTemperatureChanged
  )
  Q_PROPERTY(bool showDate READ showDate NOTIFY showDateChanged)
  Q_PROPERTY(
    bool hasNotification MEMBER m_hasNotification NOTIFY hasNotificationChanged
  )
  Q_PROPERTY(
    QString notificationText MEMBER m_notificationText NOTIFY
      notificationTextChanged
  )
  Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
public:
  QObject* root = nullptr;
  explicit Controller(QObject* parent = nullptr);
  bool showWifiDb() const { return sharedSettings.showWifiDb(); }
  bool showBatteryPercent() const {
    return sharedSettings.showBatteryPercent();
  }
  bool showBatteryTemperature() const {
    return sharedSettings.showBatteryTemperature();
  }
  bool visible() const { return m_visible; }
  bool enabled() const { return m_enabled; }
  void setShowDate(bool showDate) { sharedSettings.set_showDate(showDate); }
  bool showDate() { return sharedSettings.showDate(); }
  void setNotification(QString notificationText);

signals:
  void visibleChanged(bool);
  void showWifiDbChanged(bool);
  void showBatteryPercentChanged(bool);
  void showBatteryTemperatureChanged(bool);
  void showDateChanged(bool);
  void hasNotificationChanged(bool);
  void notificationTextChanged(QString);
  void enabledChanged(bool);

public slots:
  void openSettings(const QString& category = {});
  void showPowerMenu();

private slots:
  void checkCurrentApplication(QDBusObjectPath path = QDBusObjectPath());
  void notificationAdded(const QDBusObjectPath& path);
  void notificationRemoved(const QDBusObjectPath& path);
  void batteryAlert();
  void batteryLevelChanged(int level);
  void batteryStateChanged(int state);
  void batteryTemperatureChanged(int temperature);
  void batteryWarning();
  void chargerStateChanged(int state);
  void chargerWarning();
  void powerStateChanged(int state);
  void disconnected();
  void wifiStateChanged(int state);
  void wifiRssiChanged(int rssi);

private:
  bool m_visible = false;
  bool m_hasNotification = false;
  bool m_enabled = true;
  QString m_notificationText = "";
  int wifiState = WifiUnknown;
  General* api = nullptr;
  Power* powerApi = nullptr;
  Wifi* wifiApi = nullptr;
  Apps* appsApi = nullptr;
  System* systemApi = nullptr;
  Notifications* notificationApi = nullptr;
  int m_notificationCount = 0;
  void updateNotificationText();
};
