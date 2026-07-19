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
  Application app(
    OXIDE_SERVICE, path.path(), QDBusConnection::systemBus(), this
  );
  m_visible = app.flags().contains("topbar");
  qDebug() << app.name() << m_visible;
  emit visibleChanged(m_visible);
}

void
Controller::openSettings(const QString& category) {
  appsApi->openSettings(category);
}

#include "moc_controller.cpp"
