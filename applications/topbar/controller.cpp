#include "controller.h"

#include <liboxide.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QSet>
#include <QTextStream>

Controller::Controller(QObject* parent)
  : QObject(parent) {

  auto bus = QDBusConnection::systemBus();
  qDebug() << "Waiting for tarnish to start up";
  while (!bus.interface()->registeredServiceNames().value().contains(
    OXIDE_SERVICE
  )) {
    struct timespec args{
      .tv_sec = 1,
      .tv_nsec = 0,
    },
      res;
    nanosleep(&args, &res);
  }
  qDebug() << "Requesting APIs";
  api = new General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
  connect(api, &General::aboutToQuit, qApp, &QGuiApplication::quit);
  auto reply = api->requestAPI("power");
  reply.waitForFinished();
  if (reply.isError()) {
    qDebug() << reply.error();
    qFatal("Could not request power API");
  }
  auto path = ((QDBusObjectPath)reply).path();
  if (path == "/") {
    qDebug() << "API not available";
    qFatal("Power API was not available");
  }
  powerApi = new Power(OXIDE_SERVICE, path, bus);
  // Connect to signals
  connect(
    powerApi,
    &Power::batteryLevelChanged,
    this,
    &Controller::batteryLevelChanged
  );
  connect(
    powerApi,
    &Power::batteryStateChanged,
    this,
    &Controller::batteryStateChanged
  );
  connect(
    powerApi,
    &Power::batteryTemperatureChanged,
    this,
    &Controller::batteryTemperatureChanged
  );
  connect(
    powerApi,
    &Power::chargerStateChanged,
    this,
    &Controller::chargerStateChanged
  );
  connect(powerApi, &Power::stateChanged, this, &Controller::powerStateChanged);
  connect(powerApi, &Power::batteryAlert, this, &Controller::batteryAlert);
  connect(powerApi, &Power::batteryWarning, this, &Controller::batteryWarning);
  connect(powerApi, &Power::chargerWarning, this, &Controller::chargerWarning);
  reply = api->requestAPI("wifi");
  reply.waitForFinished();
  if (reply.isError()) {
    qDebug() << reply.error();
    qFatal("Could not request wifi API");
  }
  path = ((QDBusObjectPath)reply).path();
  if (path == "/") {
    qDebug() << "API not available";
    qFatal("Wifi API was not available");
  }
  wifiApi = new Wifi(OXIDE_SERVICE, path, bus);
  connect(wifiApi, &Wifi::disconnected, this, &Controller::disconnected);
  connect(wifiApi, &Wifi::stateChanged, this, &Controller::wifiStateChanged);
  connect(wifiApi, &Wifi::rssiChanged, this, &Controller::wifiRssiChanged);
  QTimer::singleShot(1000, [this]() {
    while (this->root == nullptr) {
      qApp->processEvents(QEventLoop::AllEvents, 100);
    }
    // Get initial values when UI is ready
    batteryLevelChanged(powerApi->batteryLevel());
    batteryStateChanged(powerApi->batteryState());
    batteryTemperatureChanged(powerApi->batteryTemperature());
    chargerStateChanged(powerApi->chargerState());
    powerStateChanged(powerApi->state());

    wifiStateChanged(wifiApi->state());
    wifiRssiChanged(wifiApi->rssi());
  });
  reply = api->requestAPI("apps");
  reply.waitForFinished();
  if (reply.isError()) {
    qDebug() << reply.error();
    qFatal("Could not request apps API");
  }
  path = ((QDBusObjectPath)reply).path();
  if (path == "/") {
    qDebug() << "API not available";
    qFatal("Apps API was not available");
  }
  appsApi = new Apps(OXIDE_SERVICE, path, bus);
  connect(
    appsApi,
    &Apps::applicationLaunched,
    this,
    &Controller::checkCurrentApplication
  );
  connect(
    appsApi,
    &Apps::applicationResumed,
    this,
    &Controller::checkCurrentApplication
  );
  connect(
    appsApi,
    &Apps::applicationExited,
    this,
    [this](const QDBusObjectPath& path) {
      Q_UNUSED(path);
      checkCurrentApplication();
    }
  );
  checkCurrentApplication();
  reply = api->requestAPI("notification");
  reply.waitForFinished();
  if (reply.isError()) {
    qDebug() << reply.error();
    qFatal("Could not request notification API");
  }
  path = ((QDBusObjectPath)reply).path();
  if (path == "/") {
    qDebug() << "API not available";
    qFatal("Notification API was not available");
  }
  notificationApi = new Notifications(OXIDE_SERVICE, path, bus);
  connect(
    notificationApi,
    &Notifications::notificationAdded,
    this,
    &Controller::notificationAdded
  );
  connect(
    notificationApi,
    &Notifications::notificationRemoved,
    this,
    &Controller::notificationRemoved
  );
  connect(
    &sharedSettings,
    &Oxide::SharedSettings::showWifiDbChanged,
    this,
    &Controller::showWifiDbChanged
  );
  connect(
    &sharedSettings,
    &Oxide::SharedSettings::showBatteryPercentChanged,
    this,
    &Controller::showBatteryPercentChanged
  );
  connect(
    &sharedSettings,
    &Oxide::SharedSettings::showBatteryTemperatureChanged,
    this,
    &Controller::showBatteryTemperatureChanged
  );
  connect(
    &sharedSettings,
    &Oxide::SharedSettings::showDateChanged,
    this,
    [this](bool showDate) {
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
      clock->setProperty(
        "text", text + QTime::currentTime().toString("h:mm a")
      );
      emit showDateChanged(showDate);
    }
  );
  emit showDateChanged(sharedSettings.showDate());
}

void
Controller::checkCurrentApplication(QDBusObjectPath path) {
  if (path.path().isNull()) {
    path = appsApi->currentApplication();
  }
  QString name;
  if (path.path() == "/") {
    m_visible = true;
  } else {
    Application app(
      OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this
    );
    name = app.name();
    m_visible = app.flags().contains("topbar");
  }
  auto lockscreenPath = appsApi->lockscreenApplication().path();
  m_enabled = lockscreenPath == "/" || path.path() != lockscreenPath;
  qDebug() << name << m_visible << m_enabled;
  emit enabledChanged(m_enabled);
  emit visibleChanged(m_visible);
}

void
Controller::openSettings(const QString& category) {
  appsApi->openSettings(category);
}

void
Controller::setNotification(QString notificationText) {
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

void
Controller::updateNotificationText() {
  if (m_notificationCount > 1) {
    setNotification(
      QStringLiteral("%1 notifications").arg(m_notificationCount)
    );
    return;
  }
  if (m_notificationCount == 1) {
    setNotification("1 notification");
    return;
  }
  m_notificationText = "";
  emit notificationTextChanged(m_notificationText);
  m_hasNotification = false;
  emit hasNotificationChanged(m_hasNotification);
}

void
Controller::notificationAdded(const QDBusObjectPath& path) {
  Q_UNUSED(path);
  m_notificationCount++;
  updateNotificationText();
}

void
Controller::notificationRemoved(const QDBusObjectPath& path) {
  Q_UNUSED(path);
  if (m_notificationCount > 0) {
    m_notificationCount--;
  }
  updateNotificationText();
}

void
Controller::batteryAlert() {
  if (root == nullptr) {
    return;
  }
  QObject* ui = root->findChild<QObject*>("batteryLevel");
  if (ui) {
    ui->setProperty("alert", true);
  }
}

void
Controller::batteryLevelChanged(int level) {
  qDebug() << "Battery level: " << level;
  if (root == nullptr) {
    return;
  }
  QObject* ui = root->findChild<QObject*>("batteryLevel");
  if (ui) {
    ui->setProperty("level", level);
  }
}

void
Controller::batteryStateChanged(int state) {
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

void
Controller::batteryTemperatureChanged(int temperature) {
  qDebug() << "Battery temperature: " << temperature;
  if (root == nullptr) {
    return;
  }
  QObject* ui = root->findChild<QObject*>("batteryLevel");
  if (ui) {
    ui->setProperty("temperature", temperature);
  }
}

void
Controller::batteryWarning() {
  qDebug() << "Battery Warning!";
  if (root == nullptr) {
    return;
  }
  QObject* ui = root->findChild<QObject*>("batteryLevel");
  if (ui) {
    ui->setProperty("warning", true);
  }
}

void
Controller::chargerStateChanged(int state) {
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
        break;
      case ChargerNotConnected:
      case ChargerNotPresent:
        ui->setProperty("connected", false);
        break;
      case ChargerUnknown:
      default:
        ui->setProperty("connected", false);
    }
  }
}

void
Controller::chargerWarning() {
  // TODO handle charger
}

void
Controller::powerStateChanged(int state) {
  Q_UNUSED(state);
  // TODO handle requested battery state
}

void
Controller::disconnected() {
  wifiStateChanged(wifiApi->state());
}

void
Controller::wifiStateChanged(int state) {
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

void
Controller::wifiRssiChanged(int rssi) {
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

#include "moc_controller.cpp"
