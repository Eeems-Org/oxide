#pragma once

#include <liboxide.h>

#include <QDate>
#include <QObject>
#include <QTime>
#include <QTimer>

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
  Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY batteryLevelChanged)
  Q_PROPERTY(
    int batteryTemperature READ batteryTemperature NOTIFY
      batteryTemperatureChanged
  )
  Q_PROPERTY(bool batteryAlert READ batteryAlert NOTIFY batteryAlertChanged)
  Q_PROPERTY(
    bool batteryWarning READ batteryWarning NOTIFY batteryWarningChanged
  )
  Q_PROPERTY(
    bool batteryCharging READ batteryCharging NOTIFY batteryChargingChanged
  )
  Q_PROPERTY(
    bool chargerConnected READ chargerConnected NOTIFY chargerConnectedChanged
  )
  Q_PROPERTY(
    bool batteryPresent READ batteryPresent NOTIFY batteryPresentChanged
  )
  Q_PROPERTY(QString wifiStateStr READ wifiStateStr NOTIFY wifiStateChanged)
  Q_PROPERTY(bool wifiConnected READ wifiConnected NOTIFY wifiStateChanged)
  Q_PROPERTY(int wifiRssi READ wifiRssi NOTIFY wifiRssiChanged)
  Q_PROPERTY(QString clockText READ clockText NOTIFY clockTextChanged)

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
  int batteryLevel() { return Oxide::Power::batteryLevel(); }
  int batteryTemperature() { return Oxide::Power::batteryTemperature(); }
  bool batteryAlert() { return Oxide::Power::batteryHasAlert(); }
  bool batteryWarning() { return Oxide::Power::batteryHasWarning(); }
  bool batteryCharging() { return Oxide::Power::batteryCharging(); }
  bool chargerConnected() { return Oxide::Power::chargerConnected(); }
  bool batteryPresent() { return Oxide::Power::batteryPresent(); }
  QString wifiStateStr();
  bool wifiConnected();
  int wifiRssi();
  QString clockText();
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
  void batteryLevelChanged(int);
  void batteryTemperatureChanged(int);
  void batteryAlertChanged();
  void batteryWarningChanged();
  void batteryChargingChanged();
  void chargerConnectedChanged();
  void batteryPresentChanged();
  void wifiStateChanged();
  void wifiRssiChanged();
  void clockTextChanged();

public slots:
  void openSettings(const QString& category = {});
  void showPowerMenu();

private slots:
  void checkCurrentApplication(QDBusObjectPath path = QDBusObjectPath());
  void notificationAdded(const QDBusObjectPath& path);
  void notificationRemoved(const QDBusObjectPath& path);
  void onBatteryAlert();
  void onBatteryStateChanged(int state);
  void onBatteryWarning();
  void onChargerStateChanged(int state);
  void onChargerWarning();
  void onPowerStateChanged(int state);
  void onDisconnected();
  void onWifiStateChanged(int state);
  void onWifiRssiChanged(int rssi);
  void onShowDateChanged(bool showDate);
  void updateClock();

private:
  bool m_visible = false;
  bool m_hasNotification = false;
  bool m_enabled = true;
  QString m_notificationText = "";
  int m_notificationCount = 0;
  General* api = nullptr;
  Power* powerApi = nullptr;
  Wifi* wifiApi = nullptr;
  Apps* appsApi = nullptr;
  System* systemApi = nullptr;
  Notifications* notificationApi = nullptr;
  QTimer* clockTimer = nullptr;
  void updateNotificationText();
};
