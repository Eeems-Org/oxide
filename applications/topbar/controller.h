#pragma once

#include <liboxide.h>
#include <liboxide/sysobject.h>

#include <QObject>

#include "notificationlist.h"
#include "wifinetworklist.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

using namespace codes::eeems::oxide1;
using codes::eeems::oxide1::Frontlight;
using codes::eeems::oxide1::Power;
using Oxide::SysObject;
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
  Q_PROPERTY(
    WifiNetworkList* networks MEMBER networks READ getNetworks NOTIFY
      networksChanged
  )
  Q_PROPERTY(
    NotificationList* notifications MEMBER notifications READ getNotifications
      NOTIFY notificationsChanged
  )
  Q_PROPERTY(QString locale READ locale NOTIFY localeChanged)
  Q_PROPERTY(QString timezone READ timezone NOTIFY timezoneChanged)
  Q_PROPERTY(bool wifiOn MEMBER m_wifion READ wifiOn NOTIFY wifiOnChanged)

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
  Q_INVOKABLE void startup() { loadSettings(); }
  Q_INVOKABLE bool turnOnWifi() {
    if (!wifiApi->enable()) {
      return false;
    }
    m_wifion = true;
    emit wifiOnChanged(true);
    connect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
    return true;
  };
  Q_INVOKABLE void turnOffWifi() {
    wifiApi->disable();
    m_wifion = false;
    emit wifiOnChanged(false);
    disconnect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
    networks->removeUnknown();
    wifiApi->flushBSSCache(0);
  };
  bool wifiOn() { return m_wifion; }
  Q_INVOKABLE void loadSettings();
  Q_INVOKABLE void
  breadcrumb(QString category, QString message, QString type = "default") {
#ifdef SENTRY
    sentry_breadcrumb(
      category.toStdString().c_str(),
      message.toStdString().c_str(),
      type.toStdString().c_str()
    );
#else
    Q_UNUSED(category);
    Q_UNUSED(message);
    Q_UNUSED(type);
#endif
  }
  void updateBatteryLevel();
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
  WifiNetworkList* getNetworks() { return networks; }
  NotificationList* getNotifications() { return notifications; }
  QString locale() { return deviceSettings.getLocale(); }
  QString timezone() { return deviceSettings.getTimezone(); }
  Q_INVOKABLE void disconnectWifiSignals() {
    disconnect(wifiApi, &Wifi::bssFound, this, &Controller::bssFound);
    disconnect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
    disconnect(wifiApi, &Wifi::networkAdded, this, &Controller::networkAdded);
    disconnect(
      wifiApi, &Wifi::networkRemoved, this, &Controller::networkRemoved
    );
  }
  Q_INVOKABLE void connectWifiSignals() {
    networks->clear();
    QList<Network*> networksToAdd;
    for (auto path : wifiApi->networks()) {
      auto network = new Network(
        OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this
      );
      auto ssid = network->ssid();
      if (ssid.isEmpty()) {
        delete network;
        continue;
      }
      networksToAdd.append(network);
    }
    networks->append(networksToAdd);
    QList<BSS*> bsssToAdd;
    for (auto path : wifiApi->bSSs()) {
      auto bss =
        new BSS(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
      auto ssid = bss->ssid();
      if (ssid.isEmpty()) {
        delete bss;
        continue;
      }
      bsssToAdd.append(bss);
    }
    networks->append(bsssToAdd);
    networks->sort();
    networks->setConnected(wifiApi->network());
    connect(wifiApi, &Wifi::bssFound, this, &Controller::bssFound);
    connect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
    connect(wifiApi, &Wifi::networkAdded, this, &Controller::networkAdded);
    connect(wifiApi, &Wifi::networkRemoved, this, &Controller::networkRemoved);
  }
  Apps* getAppsApi() { return appsApi; }
  Notifications* getNotificationApi() { return notificationApi; }
signals:
  void visibleChanged(bool);
  void showWifiDbChanged(bool);
  void showBatteryPercentChanged(bool);
  void showBatteryTemperatureChanged(bool);
  void networksChanged(WifiNetworkList*);
  void localeChanged(QString);
  void timezoneChanged(QString);
  void wifiOnChanged(bool);
  void showDateChanged(bool);
  void hasNotificationChanged(bool);
  void notificationTextChanged(QString);
  void notificationsChanged(NotificationList*);

private slots:
  void checkCurrentApplication(QDBusObjectPath path = QDBusObjectPath());
  void notificationsUpdated() {
    if (notifications->length() > 1) {
      setNotification(
        QStringLiteral("%1 notifications").arg(notifications->length())
      );
    } else if (!notifications->empty()) {
      setNotification("1 notification");
    } else {
      clearNotification();
    }
    emit notificationsChanged(notifications);
  }
  void notificationAdded(const QDBusObjectPath& path) {
    auto notification = new Notification(
      OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this
    );
    notifications->append(notification);
    // Notification UI updates will be handled by notificationsUpdated
  }
  void notificationRemoved(const QDBusObjectPath& path) {
    auto notification = notifications->get(path);
    if (notification == nullptr) {
      return;
    }
    notifications->removeAll(notification);
    notification->deleteLater();
    // Notification UI updates will be handled by notificationsUpdated
  }
  void notificationChanged(const QDBusObjectPath& path) {
    Q_UNUSED(path);
    emit notificationsChanged(notifications);
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
  void bssFound(const QDBusObjectPath& path) {
    auto bss =
      new BSS(OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this);
    auto ssid = bss->ssid();
    if (ssid.isEmpty()) {
      delete bss;
      return;
    }
    networks->append(bss);
  }
  void bssRemoved(const QDBusObjectPath& path) { networks->remove(path); }
  void disconnected() {
    wifiStateChanged(wifiApi->state());
    networks->setConnected(QDBusObjectPath("/"));
  }
  void networkAdded(const QDBusObjectPath& path) {
    auto network = new Network(
      OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this
    );
    auto ssid = network->ssid();
    if (ssid.isEmpty()) {
      delete network;
      return;
    }
    networks->append(network);
  }
  void networkConnected(const QDBusObjectPath& path) {
    networks->setConnected(path);
  }
  void networkRemoved(const QDBusObjectPath& path) { networks->remove(path); }
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
  void checkUITimer();
  bool m_visible = false;
  bool m_showWifiDb = false;
  bool m_showBatteryPercent = false;
  bool m_showBatteryTemperature = false;
  bool m_showDate = false;
  bool m_hasNotification = false;
  QString m_notificationText = "";
  bool m_powerConnected = false;
  int wifiState = WifiUnknown;
  bool m_wifion;
  SysObject wifi;
  General* api = nullptr;
  Power* powerApi = nullptr;
  Wifi* wifiApi = nullptr;
  System* systemApi = nullptr;
  Apps* appsApi = nullptr;
  Notifications* notificationApi = nullptr;
  Frontlight* frontlightApi = nullptr;
  QList<QObject*> applications;
  WifiNetworkList* networks;
  NotificationList* notifications;
};
