#include "controller.h"

#include <liboxide.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QSet>
#include <QTextStream>

QSet<QString> settings = {"columns", "autoStartApplication"};
QSet<QString> booleanSettings{
  "showWifiDb",
  "showBatteryPercent",
  "showBatteryTemperature",
  "showDate"
};
QList<QString> configDirectoryPaths =
  {"/opt/etc/draft", "/etc/draft", "/home/root/.config/draft"};
QList<QString> configFileDirectoryPaths =
  {"/opt/etc", "/etc", "/home/root/.config", "/home/root/.vellum/etc"};

QFile*
getConfigFile() {
  for (auto path : configFileDirectoryPaths) {
    QDir dir(path);
    if (dir.exists() && !dir.isEmpty()) {
      QFile* file = new QFile(path + "/oxide.conf");
      if (file->exists()) {
        return file;
      }
    }
  }
  return nullptr;
}
bool
configFileExists() {
  auto configFile = getConfigFile();
  if (configFile == nullptr) {
    return false;
  }
  bool exists = configFile->exists();
  configFile->close();
  delete configFile;
  return exists;
}

Controller::Controller(QObject* parent)
  : QObject(parent)
  , m_wifion(false)
  , wifi("/sys/class/net/wlan0")
  , applications() {
  networks = new WifiNetworkList();
  notifications = new NotificationList();

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
  connect(
    wifiApi, &Wifi::networkConnected, this, &Controller::networkConnected
  );
  connect(wifiApi, &Wifi::stateChanged, this, &Controller::wifiStateChanged);
  connect(wifiApi, &Wifi::rssiChanged, this, &Controller::wifiRssiChanged);
  networks->setAPI(wifiApi);
  auto state = wifiApi->state();
  m_wifion = state != WifiState::WifiOff && state != WifiState::WifiUnknown;
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
    emit wifiOnChanged(m_wifion);
    auto network = wifiApi->network();
    if (network.path() != "/") {
      networkConnected(network);
    }
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
    notificationApi,
    &Notifications::notificationChanged,
    this,
    &Controller::notificationChanged
  );
  connect(
    notifications,
    &NotificationList::updated,
    this,
    &Controller::notificationsUpdated
  );
}

void
Controller::loadSettings() {
  // If the config directory doesn't exist,
  // then print an error and stop.
  qDebug() << "parsing config file...";
  if (configFileExists()) {
    auto configFile = getConfigFile();
    if (!configFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
      qCritical() << "Couldn't read the config file. "
                  << configFile->fileName();
      return;
    }
    QTextStream in(configFile);
    while (!in.atEnd()) {
      QString line = in.readLine();
      if (!line.startsWith("#") && !line.isEmpty()) {
        QStringList parts = line.split("=");
        if (parts.length() != 2) {
          O_WARNING("  Wrong format on " << line);
          continue;
        }
        QString lhs = parts.at(0).trimmed();
        QString rhs = parts.at(1).trimmed();
        if (booleanSettings.contains(lhs)) {
          auto property = lhs.toStdString();
          auto value = rhs.toLower();
          auto boolValue = value == "true" || value == "t" || value == "y" ||
                           value == "yes" || value == "1";
          setProperty(property.c_str(), boolValue);
          qDebug() << "  " << (property + ":").c_str() << boolValue;
        } else if (settings.contains(lhs)) {
          auto property = lhs.toStdString();
          setProperty(property.c_str(), rhs);
          qDebug() << "  " << (property + ":").c_str()
                   << rhs.toStdString().c_str();
        }
      }
    }
    configFile->close();
    delete configFile;
  }
}
void
Controller::setShowWifiDb(bool state) {
  m_showWifiDb = state;
  if (root != nullptr) {
    qDebug() << "Show Wifi DB: " << state;
    emit showWifiDbChanged(state);
  }
}
void
Controller::setShowBatteryPercent(bool state) {
  m_showBatteryPercent = state;
  if (root != nullptr) {
    qDebug() << "Show Battery Percentage: " << state;
    emit showBatteryPercentChanged(state);
  }
}
void
Controller::setShowBatteryTemperature(bool state) {
  m_showBatteryTemperature = state;
  if (root != nullptr) {
    qDebug() << "Show Battery Temperature: " << state;
    emit showBatteryTemperatureChanged(state);
  }
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

#include "moc_controller.cpp"
