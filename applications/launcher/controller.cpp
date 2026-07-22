#include "controller.h"

#include <liboxide.h>
#include <unistd.h>

#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QTimer>

Controller::Controller(QObject* parent)
  : QObject(parent)
  , m_appList(new AppListModel(this)) {
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
  connect(powerApi, &Power::batteryAlert, this, &Controller::batteryAlert);
  connect(powerApi, &Power::batteryWarning, this, &Controller::batteryWarning);
  QTimer::singleShot(1000, [this]() {
    while (this->root == nullptr) {
      qApp->processEvents(QEventLoop::AllEvents, 100);
    }
    // Get initial values when UI is ready
    batteryLevelChanged(powerApi->batteryLevel());
    batteryStateChanged(powerApi->batteryState());
    batteryTemperatureChanged(powerApi->batteryTemperature());
    chargerStateChanged(powerApi->chargerState());
  });
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

#include "moc_controller.cpp"
