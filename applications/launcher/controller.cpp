#include "controller.h"

#include <liboxide.h>
#include <unistd.h>

#include <QDebug>
#include <QFile>
#include <QSettings>

Controller::Controller(QObject* parent)
  : QObject(parent)
  , applications() {
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

QList<QObject*>
Controller::getApps() {
  auto bus = QDBusConnection::systemBus();
  auto running = appsApi->runningApplications();
  auto paused = appsApi->pausedApplications();
  for (auto key : paused.keys()) {
    if (running.contains(key)) {
      continue;
    }
    running.insert(key, paused[key]);
  }
  for (auto item : appsApi->applications()) {
    auto path = item.value<QDBusObjectPath>().path();
    Application app(OXIDE_SERVICE, path, bus, this);
    if (app.hidden() || app.transient()) {
      continue;
    }
    auto name = app.name();
    auto appItem = getApplication(name);
    if (appItem == nullptr) {
      qDebug() << "New application:" << name;
      appItem = new AppItem(this);
      applications.append(appItem);
    }
    auto displayName = app.displayName();
    if (displayName.isEmpty()) {
      displayName = name;
    }
    appItem->setProperty("path", path);
    appItem->setProperty("name", name);
    appItem->setProperty("displayName", displayName);
    appItem->setProperty("desc", app.description());
    appItem->setProperty("call", app.bin());
    appItem->setProperty("running", running.contains(name));
    auto icon = app.icon();
    if (!icon.isEmpty() && QFile(icon).exists()) {
      appItem->setProperty("imgFile", "file:" + icon);
    } else {
      appItem->setProperty("imgFile", "");
    }
    if (!appItem->ok()) {
      qDebug() << "Invalid item" << appItem->property("name").toString();
      applications.removeAll(appItem);
      appItem->deleteLater();
    }
  }
  // Sort by name
  std::sort(
    applications.begin(),
    applications.end(),
    [=](const QObject* a, const QObject* b) -> bool {
      return a->property("displayName").toString() <
             b->property("displayName").toString();
    }
  );
  return applications;
}
AppItem*
Controller::getApplication(QString name) {
  for (auto app : applications) {
    if (app->property("name").toString() == name) {
      return reinterpret_cast<AppItem*>(app);
    }
  }
  return nullptr;
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
