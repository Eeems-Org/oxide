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
  Q_PROPERTY(bool visible MEMBER m_visible NOTIFY visibleChanged)
  Q_PROPERTY(bool showWifiDb MEMBER m_showWifiDb NOTIFY showWifiDbChanged)
  Q_PROPERTY(
    bool showBatteryPercent MEMBER m_showBatteryPercent NOTIFY
      showBatteryPercentChanged
  )
  Q_PROPERTY(
    bool showBatteryTemperature MEMBER m_showBatteryTemperature NOTIFY
      showBatteryTemperatureChanged
  )
  Q_PROPERTY(bool showDate MEMBER m_showDate NOTIFY showDateChanged)
  Q_PROPERTY(
    bool hasNotification MEMBER m_hasNotification NOTIFY hasNotificationChanged
  )
  Q_PROPERTY(
    QString notificationText MEMBER m_notificationText NOTIFY
      notificationTextChanged
  )
public:
  QObject* root = nullptr;
  explicit Controller(QObject* parent = nullptr);
  Q_INVOKABLE void loadSettings();
  bool showWifiDb() const { return m_showWifiDb; }
  void setShowWifiDb(bool);
  bool showBatteryPercent() const { return m_showBatteryPercent; }
  void setShowBatteryPercent(bool);
  bool showBatteryTemperature() const { return m_showBatteryTemperature; }
  void setShowBatteryTemperature(bool);
  void setShowDate(bool showDate) {
    m_showDate = showDate;
    emit showDateChanged(showDate);
    if (root == nullptr) {
      return;
    }
    QObject* clock = root->findChild<QObject*>("clock");
    if (clock == nullptr) {
      return;
    }
    QString text = "";
    if (showDate) {
      text = QDate::currentDate().toString(Qt::TextDate) + " ";
    }
    clock->setProperty("text", text + QTime::currentTime().toString("h:mm a"));
  }
  bool showDate() { return m_showDate; }
  void setNotification(QString notificationText) {
    if (m_notificationText == notificationText) {
      return;
    }
    m_notificationText = notificationText;
    emit notificationTextChanged(notificationText);
    if (!m_hasNotification) {
      m_hasNotification = true;
      emit hasNotificationChanged(true);
    }
  }
  void clearNotification() {
    m_notificationText = "";
    emit notificationTextChanged(m_notificationText);
    m_hasNotification = false;
    emit hasNotificationChanged(m_hasNotification);
  }
  bool getPowerConnected() { return m_powerConnected; }
signals:
  void visibleChanged(bool);
  void showWifiDbChanged(bool);
  void showBatteryPercentChanged(bool);
  void showBatteryTemperatureChanged(bool);
  void showDateChanged(bool);
  void hasNotificationChanged(bool);
  void notificationTextChanged(QString);

public slots:
  void openSettings(const QString& category = {});

private slots:
  void checkCurrentApplication(QDBusObjectPath path = QDBusObjectPath());
  void notificationAdded(const QDBusObjectPath& path) {
    Q_UNUSED(path);
    m_notificationCount++;
    updateNotificationText();
  }
  void notificationRemoved(const QDBusObjectPath& path) {
    Q_UNUSED(path);
    if (m_notificationCount > 0) {
      m_notificationCount--;
    }
    updateNotificationText();
  }
  void batteryAlert() {
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      ui->setProperty("alert", true);
    }
  }
  void batteryLevelChanged(int level) {
    qDebug() << "Battery level: " << level;
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      ui->setProperty("level", level);
    }
  }
  void batteryStateChanged(int state) {
    switch (state) {
      case BatteryCharging:
        qDebug() << "Battery state: Charging";
        break;
      case BatteryNotPresent:
        qDebug() << "Battery state: Not Present";
        break;
      case BatteryDischarging:
        qDebug() << "Battery state: Discharging";
        break;
      case BatteryUnknown:
      default:
        qDebug() << "Battery state: Unknown";
    }
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      if (state != BatteryNotPresent) {
        ui->setProperty("present", true);
      }
      switch (state) {
        case BatteryCharging:
          ui->setProperty("charging", true);
          m_powerConnected = true;
          break;
        case BatteryNotPresent:
          ui->setProperty("present", false);
          break;
        case BatteryDischarging:
          ui->setProperty("charging", false);
          break;
        case BatteryUnknown:
        default:
          ui->setProperty("charging", false);
      }
    }
  }
  void batteryTemperatureChanged(int temperature) {
    qDebug() << "Battery temperature: " << temperature;
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      ui->setProperty("temperature", temperature);
    }
  }
  void batteryWarning() {
    qDebug() << "Battery Warning!";
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      ui->setProperty("warning", true);
    }
  }
  void chargerStateChanged(int state) {
    switch (state) {
      case ChargerConnected:
        qDebug() << "Charger state: Connected";
        break;
      case ChargerNotPresent:
        qDebug() << "Charger state: Not Present";
        break;
      case ChargerNotConnected:
        qDebug() << "Charger state: Not Connected";
        break;
      case ChargerUnknown:
      default:
        qDebug() << "Charger state: Unknown";
    }
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if (ui) {
      if (state != ChargerNotPresent) {
        ui->setProperty("present", true);
      }
      switch (state) {
        case ChargerConnected:
          ui->setProperty("connected", true);
          m_powerConnected = true;
          break;
        case ChargerNotConnected:
        case ChargerNotPresent:
          ui->setProperty("connected", false);
          m_powerConnected = false;
          break;
        case ChargerUnknown:
        default:
          ui->setProperty("connected", false);
      }
    }
  }
  void chargerWarning() {
    // TODO handle charger
  }
  void powerStateChanged(int state) {
    Q_UNUSED(state);
    // TODO handle requested battery state
  }
  void disconnected() { wifiStateChanged(wifiApi->state()); }
  void wifiStateChanged(int state) {
    if (root == nullptr) {
      wifiState = state;
      return;
    }
    if (state == wifiState) {
      return;
    }
    wifiState = state;
    switch (state) {
      case WifiOff:
        qDebug() << "Wifi state: Off";
        break;
      case WifiDisconnected:
        qDebug() << "Wifi state: On+Disconnected";
        break;
      case WifiOffline:
        qDebug() << "Wifi state: On+Connected+Offline";
        break;
      case WifiOnline:
        qDebug() << "Wifi state: On+Connected+Online";
        break;
      case WifiUnknown:
      default:
        qDebug() << "Wifi state: Unknown";
    }
    QObject* ui = root->findChild<QObject*>("wifiState");
    if (ui) {
      switch (state) {
        case WifiOff:
          ui->setProperty("state", "down");
          break;
        case WifiDisconnected:
          ui->setProperty("state", "up");
          ui->setProperty("connected", false);
          break;
        case WifiOffline:
          ui->setProperty("state", "up");
          ui->setProperty("connected", true);
          break;
        case WifiOnline:
          ui->setProperty("state", "up");
          ui->setProperty("connected", true);
          ui->setProperty("rssi", wifiApi->rssi());
          break;
        case WifiUnknown:
        default:
          ui->setProperty("state", "unknown");
          ui->setProperty("connected", false);
          ui->setProperty("rssi", -100);
      }
    }
  }
  void wifiRssiChanged(int rssi) {
    if (root == nullptr) {
      return;
    }
    QObject* ui = root->findChild<QObject*>("wifiState");
    if (ui) {
      if (wifiState != WifiOnline) {
        rssi = -100;
      }
      ui->setProperty("rssi", rssi);
    }
  }

private:
  bool m_visible = false;
  bool m_showWifiDb = false;
  bool m_showBatteryPercent = false;
  bool m_showBatteryTemperature = false;
  bool m_showDate = false;
  bool m_hasNotification = false;
  QString m_notificationText = "";
  bool m_powerConnected = false;
  int wifiState = WifiUnknown;
  General* api = nullptr;
  Power* powerApi = nullptr;
  Wifi* wifiApi = nullptr;
  Apps* appsApi = nullptr;
  Notifications* notificationApi = nullptr;
  int m_notificationCount = 0;
  void updateNotificationText() {
    if (m_notificationCount > 1) {
      setNotification(
        QStringLiteral("%1 notifications").arg(m_notificationCount)
      );
    } else if (m_notificationCount == 1) {
      setNotification("1 notification");
    } else {
      clearNotification();
    }
  }
};
