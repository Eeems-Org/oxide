#include "controller.h"

#include <liboxide.h>
#include <unistd.h>

#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QTimer>

Controller::Controller(QObject* parent)
  : QObject(parent)
  , m_appList(new AppListModel(this))
  , m_refreshTimer(new QTimer(this)) {
  m_refreshTimer->setSingleShot(true);
  m_refreshTimer->setInterval(100);
  connect(m_refreshTimer, &QTimer::timeout, this, &Controller::refreshApps);
  for (const auto& path : {
         "/home/root/.config/oxide.conf",
         "/opt/etc/oxide.conf",
         "/etc/oxide.conf",
       }) {
    QFile file(path);
    if (!file.exists()) {
      continue;
    }
    qDebug() << "Migrating settings from" << path;
    QSettings oxideConf(path, QSettings::IniFormat);
    if (oxideConf.contains("columns")) {
      sharedSettings.set_columns(oxideConf.value("columns").toInt());
    }
    if (oxideConf.contains("autoStartApplication")) {
      sharedSettings.set_autoStartApplication(
        oxideConf.value("autoStartApplication").toString()
      );
    }
    file.remove();
  }

  auto bus = QDBusConnection::systemBus();
  if (!bus.isConnected() || !bus.interface()) {
    qFatal("D-Bus system bus connection unavailable");
  }
  qDebug() << "Waiting for tarnish to start up";
  int retries = 30;
  while (!bus.interface()->registeredServiceNames().value().contains(
    OXIDE_SERVICE
  )) {
    if (!retries--) {
      qFatal("Timed out waiting for %s to register on D-Bus", OXIDE_SERVICE);
    }
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
  connect(powerApi, &Power::batteryAlert, this, &Controller::batteryAlert);
  connect(powerApi, &Power::batteryWarning, this, &Controller::batteryWarning);
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
  connect(
    systemApi,
    &System::powerOffInhibitedChanged,
    this,
    &Controller::powerOffInhibitedChanged
  );
  connect(systemApi, &System::powerOffInhibitedChanged, [=](bool value) {
    qDebug() << "Power Off Inhibited:" << value;
  });
  connect(
    systemApi,
    &System::sleepInhibitedChanged,
    this,
    &Controller::sleepInhibitedChanged
  );
  emit powerOffInhibitedChanged(powerOffInhibited());
  emit sleepInhibitedChanged(sleepInhibited());
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
    &Apps::applicationUnregistered,
    this,
    &Controller::unregisterApplication
  );
  connect(
    appsApi,
    &Apps::applicationRegistered,
    this,
    &Controller::registerApplication
  );
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

  // SharedSettings signal forwarding
  connect(
    &sharedSettings,
    &Oxide::SharedSettings::columnsChanged,
    this,
    &Controller::columnsChanged
  );
  connect(
    &sharedSettings,
    &Oxide::SharedSettings::autoStartApplicationChanged,
    this,
    &Controller::autoStartApplicationChanged
  );
}

void
Controller::setRoot(QObject* newRoot) {
  root = newRoot;
  if (newRoot == nullptr) {
    return;
  }
  batteryLevelChanged(powerApi->batteryLevel());
  batteryStateChanged(powerApi->batteryState());
  batteryTemperatureChanged(powerApi->batteryTemperature());
  chargerStateChanged(powerApi->chargerState());
}

void
Controller::startup() {
  if (autoStartApplication().isEmpty()) {
    qDebug() << "No auto start application";
    return;
  }
  auto app = m_appList->findByName(autoStartApplication());
  if (app == nullptr) {
    qDebug() << "Unable to find auto start application";
    return;
  }
  app->execute();
}

void
Controller::setLocale(const QString& locale) {
  deviceSettings.setLocale(locale);
  emit localeChanged(locale);
}

void
Controller::breadcrumb(QString category, QString message, QString type) {
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

void
Controller::refreshApps() {
  auto bus = QDBusConnection::systemBus();
  auto running = appsApi->runningApplications();
  auto paused = appsApi->pausedApplications();
  for (auto key : paused.keys()) {
    if (running.contains(key)) {
      continue;
    }
    running.insert(key, paused[key]);
  }
  m_appList->update(OXIDE_SERVICE, this, appsApi->applications(), running);
}

void
Controller::scheduleRefresh() {
  m_refreshTimer->start();
}

void
Controller::importDraftApps() {
  QProcess::execute("update-desktop-database", QStringList() << "--verbose");
}
void
Controller::suspend() {
  qDebug() << "Suspending...";
  systemApi->suspend();
}

void
Controller::lock() {
  qDebug() << "Locking...";
  appsApi->openLockScreen();
}

void
Controller::unregisterApplication(QDBusObjectPath path) {
  Q_UNUSED(path)
  scheduleRefresh();
}

void
Controller::registerApplication(QDBusObjectPath path) {
  qDebug() << "New application detected" << path.path();
  scheduleRefresh();
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

#include "moc_controller.cpp"
