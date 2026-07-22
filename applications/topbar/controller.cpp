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
  // Connect to power signals
  connect(
    powerApi,
    &Power::batteryLevelChanged,
    this,
    &Controller::batteryLevelChanged
  );
  connect(
    powerApi,
    &Power::batteryTemperatureChanged,
    this,
    &Controller::batteryTemperatureChanged
  );
  connect(
    powerApi,
    &Power::batteryStateChanged,
    this,
    &Controller::onBatteryStateChanged
  );
  connect(
    powerApi,
    &Power::chargerStateChanged,
    this,
    &Controller::onChargerStateChanged
  );
  connect(
    powerApi, &Power::stateChanged, this, &Controller::onPowerStateChanged
  );
  connect(powerApi, &Power::batteryAlert, this, &Controller::onBatteryAlert);
  connect(
    powerApi, &Power::batteryWarning, this, &Controller::onBatteryWarning
  );
  connect(
    powerApi, &Power::chargerWarning, this, &Controller::onChargerWarning
  );
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
  connect(wifiApi, &Wifi::disconnected, this, &Controller::onDisconnected);
  connect(wifiApi, &Wifi::stateChanged, this, &Controller::onWifiStateChanged);
  connect(wifiApi, &Wifi::rssiChanged, this, &Controller::onWifiRssiChanged);
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
    &Controller::onShowDateChanged
  );
  emit showDateChanged(sharedSettings.showDate());
  reply = api->requestAPI("system");
  reply.waitForFinished();
  if (reply.isError()) {
    qDebug() << reply.error();
    qFatal("Could not request system API");
  }
  path = ((QDBusObjectPath)reply).path();
  if (path == "/") {
    qDebug() << "API not available";
    qFatal("System API was not available");
  }
  systemApi = new System(OXIDE_SERVICE, path, bus);

  // Setup clock timer
  clockTimer = new QTimer(this);
  clockTimer->setSingleShot(true);
  connect(clockTimer, &QTimer::timeout, this, &Controller::updateClock);
  updateClock();
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
Controller::showPowerMenu() {
  systemApi->showPowerMenu();
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
Controller::onBatteryAlert() {
  emit batteryAlertChanged();
}

void
Controller::onBatteryStateChanged(int state) {
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
  emit batteryChargingChanged();
  emit batteryPresentChanged();
}

void
Controller::onBatteryWarning() {
  qDebug() << "Battery Warning!";
  emit batteryWarningChanged();
}

void
Controller::onChargerStateChanged(int state) {
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
  emit chargerConnectedChanged();
}

void
Controller::onChargerWarning() {
  // TODO handle charger
}

void
Controller::onPowerStateChanged(int state) {
  Q_UNUSED(state);
  // TODO handle requested battery state
}

void
Controller::onDisconnected() {
  onWifiStateChanged(wifiApi->state());
}

void
Controller::onWifiStateChanged(int state) {
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
  emit wifiStateChanged();
}

void
Controller::onWifiRssiChanged(int rssi) {
  Q_UNUSED(rssi);
  emit wifiRssiChanged();
}

QString
Controller::wifiStateStr() {
  if (wifiApi == nullptr) {
    return "unknown";
  }
  switch (wifiApi->state()) {
    case WifiOff:
      return "down";
    case WifiDisconnected:
    case WifiOffline:
    case WifiOnline:
      return "up";
    case WifiUnknown:
    default:
      return "unknown";
  }
}

bool
Controller::wifiConnected() {
  if (wifiApi == nullptr) {
    return false;
  }
  auto state = wifiApi->state();
  return state == WifiOffline || state == WifiOnline;
}

int
Controller::wifiRssi() {
  if (wifiApi == nullptr) {
    return -100;
  }
  if (wifiApi->state() != WifiOnline) {
    return -100;
  }
  return wifiApi->rssi();
}

QString
Controller::clockText() {
  QString text = "";
  if (sharedSettings.showDate()) {
    text = QDate::currentDate().toString(Qt::TextDate) + " ";
  }
  return text + QTime::currentTime().toString("h:mm a");
}

void
Controller::updateClock() {
  emit clockTextChanged();
  auto currentTime = QTime::currentTime();
  clockTimer->start((60 - currentTime.second()) * 1000 - currentTime.msec());
}

void
Controller::onShowDateChanged(bool showDate) {
  Q_UNUSED(showDate);
  emit showDateChanged(showDate);
  emit clockTextChanged();
}

#include "moc_controller.cpp"
