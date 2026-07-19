#include "controller.h"

#include <liboxide.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <fstream>
#include <memory>
#include <sstream>

QList<QString> configDirectoryPaths =
  {"/opt/etc/draft", "/etc/draft", "/home/root/.config/draft"};

Controller::Controller(QObject* parent)
  : QObject(parent)
  , m_wifion(false)
  , wifi("/sys/class/net/wlan0")
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
    if (oxideConf.contains("showWifiDb")) {
      sharedSettings.set_showWifiDb(oxideConf.value("showWifiDb").toBool());
    }
    if (oxideConf.contains("showBatteryPercent")) {
      sharedSettings.set_showBatteryPercent(
        oxideConf.value("showBatteryPercent").toBool()
      );
    }
    if (oxideConf.contains("showBatteryTemperature")) {
      sharedSettings.set_showBatteryTemperature(
        oxideConf.value("showBatteryTemperature").toBool()
      );
    }
    if (oxideConf.contains("showDate")) {
      sharedSettings.set_showDate(oxideConf.value("showDate").toBool());
    }
    if (oxideConf.contains("autoStartApplication")) {
      sharedSettings.set_autoStartApplication(
        oxideConf.value("autoStartApplication").toString()
      );
    }
    file.remove();
  }

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
    if (
      stateController != nullptr && stateController->property("state") == "wifi"
    ) {
      connectWifiSignals();
    }
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
  connect(systemApi, &System::autoSleepChanged, [this](int sleepAfter) {
    setAutomaticSleep(sleepAfter);
    setSleepAfter(sleepAfter);
  });
  connect(
    systemApi, &System::swipeLengthChanged, [this](int direction, int length) {
      setSwipeLength(direction, length);
    }
  );
  auto autoSleep = systemApi->autoSleep();
  setAutomaticSleep(autoSleep);
  setSleepAfter(autoSleep);
  auto autoLock = systemApi->autoLock();
  setAutomaticLock(autoLock);
  setLockAfter(autoLock);
  for (short i = 1; i <= 4; ++i) {
    setSwipeLength(i, systemApi->getSwipeLength(i));
  }
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
  connect(
    notificationApi,
    &Notifications::displayTimeChanged,
    this,
    [this](uint displayTime) { setNotificationDisplayTime(displayTime); }
  );

  reply = api->requestAPI("frontlight");
  reply.waitForFinished();
  if (!reply.isError()) {
    path = ((QDBusObjectPath)reply).path();
    if (path != "/") {
      frontlightApi = new Frontlight(OXIDE_SERVICE, path, bus);
      connect(
        frontlightApi,
        &Frontlight::extraBrightnessChanged,
        this,
        &Controller::extraBrightnessChanged
      );
    }
  }

  uiTimer = new QTimer(this);
  uiTimer->setSingleShot(false);
  uiTimer->setInterval(3 * 1000); // 3 seconds
  connect(
    uiTimer,
    &QTimer::timeout,
    this,
    QOverload<>::of(&Controller::updateUIElements)
  );
  uiTimer->start();

  // SharedSettings signal forwarding
  connect(
    &sharedSettings,
    &Oxide::SharedSettings::columnsChanged,
    this,
    &Controller::columnsChanged
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
  connect(
    &sharedSettings,
    &Oxide::SharedSettings::autoStartApplicationChanged,
    this,
    &Controller::autoStartApplicationChanged
  );
}

void
Controller::loadSettings() {
  setNotificationDisplayTime(notificationApi->displayTime());
  auto sleepAfter = systemApi->autoSleep();
  qDebug() << "Automatic sleep" << sleepAfter;
  setAutomaticSleep(sleepAfter);
  setSleepAfter(sleepAfter);
  auto lockOnSuspend = systemApi->lockOnSuspend();
  qDebug() << "Lock on suspend" << lockOnSuspend;
  setLockOnSuspend(lockOnSuspend);
  auto autoLock = systemApi->autoLock();
  qDebug() << "Automatic lock" << autoLock;
  setAutomaticLock(autoLock);
  setLockAfter(autoLock);
  for (short i = 1; i <= 4; i++) {
    setSwipeLength(i, systemApi->getSwipeLength(i));
  }
  qDebug() << "Updating UI with settings...";
  if (root == nullptr) {
    return;
  }
  // Populate settings in UI
  QObject* columnsSpinBox = root->findChild<QObject*>("columnsSpinBox");
  if (!columnsSpinBox) {
    qDebug() << "Can't find columnsSpinBox";
  } else {
    columnsSpinBox->setProperty("value", columns());
  }
  QObject* sleepAfterSpinBox = root->findChild<QObject*>("sleepAfterSpinBox");
  if (!sleepAfterSpinBox) {
    qDebug() << "Can't find sleepAfterSpinBox";
  } else {
    sleepAfterSpinBox->setProperty("value", this->sleepAfter());
  }
  QObject* swipeLengthRightSpinBox =
    root->findChild<QObject*>("swipeLengthRightSpinBox");
  if (!swipeLengthRightSpinBox) {
    qDebug() << "Can't find swipeLengthRightSpinBox";
  } else {
    swipeLengthRightSpinBox->setProperty("value", this->swipeLengthRight());
  }
  QObject* swipeLengthLeftSpinBox =
    root->findChild<QObject*>("swipeLengthLeftSpinBox");
  if (!swipeLengthLeftSpinBox) {
    qDebug() << "Can't find swipeLengthLeftSpinBox";
  } else {
    swipeLengthLeftSpinBox->setProperty("value", this->swipeLengthLeft());
  }
  QObject* swipeLengthUpSpinBox =
    root->findChild<QObject*>("swipeLengthUpSpinBox");
  if (!swipeLengthUpSpinBox) {
    qDebug() << "Can't find swipeLengthUpSpinBox";
  } else {
    swipeLengthUpSpinBox->setProperty("value", this->swipeLengthUp());
  }
  QObject* swipeLengthDownSpinBox =
    root->findChild<QObject*>("swipeLengthDownSpinBox");
  if (!swipeLengthDownSpinBox) {
    qDebug() << "Can't find swipeLengthDownSpinBox";
  } else {
    swipeLengthDownSpinBox->setProperty("value", this->swipeLengthDown());
  }
  QObject* notificationDisplayTimeSpinBox =
    root->findChild<QObject*>("notificationDisplayTimeSpinBox");
  if (!notificationDisplayTimeSpinBox) {
    qDebug() << "Can't find notificationDisplayTimeSpinBox";
  } else {
    notificationDisplayTimeSpinBox->setProperty(
      "value", this->notificationDisplayTime()
    );
  }
  qDebug() << "Finished updating UI.";
}
void
Controller::saveSettings() {
  qDebug() << "Saving configuration...";
  if (!m_automaticSleep) {
    systemApi->setAutoSleep(0);
  } else {
    systemApi->setAutoSleep(m_sleepAfter);
    auto sleepAfter = systemApi->autoSleep();
    if (sleepAfter != m_sleepAfter) {
      setSleepAfter(sleepAfter);
      setAutomaticSleep(sleepAfter);
    }
  }
  systemApi->setLockOnSuspend(m_lockOnSuspend);
  if (!m_automaticLock) {
    systemApi->setAutoLock(0);
  } else {
    systemApi->setAutoLock(m_lockAfter);
    auto lockAfter = systemApi->autoLock();
    if (lockAfter != m_lockAfter) {
      setLockAfter(lockAfter);
      setAutomaticLock(lockAfter);
    }
  }
  for (short i = 1; i <= 4; i++) {
    systemApi->setSwipeLength(i, getSwipeLength(i));
  }
  notificationApi->setDisplayTime(m_notificationDisplayTime);
  qDebug() << "Done saving configuration.";
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
Controller::powerOff() {
  qDebug() << "Powering off...";
  systemApi->powerOff();
}
void
Controller::reboot() {
  qDebug() << "Rebooting...";
  systemApi->reboot();
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
inline void
updateUI(QObject* ui, const char* name, const QVariant& value) {
  if (ui) {
    ui->setProperty(name, value);
  }
}
void
Controller::updateUIElements() {}
void
Controller::setAutomaticSleep(bool state) {
  m_automaticSleep = state;
  if (state) {
    qDebug() << "Enabling automatic sleep";
  } else {
    qDebug() << "Disabling automatic sleep";
  }
  emit automaticSleepChanged(state);
}
void
Controller::setLockOnSuspend(bool state) {
  m_lockOnSuspend = state;
  if (state) {
    qDebug() << "Enabling lock on suspend";
  } else {
    qDebug() << "Disabling lock on suspend";
  }
  emit lockOnSuspendChanged(state);
}
void
Controller::setAutomaticLock(bool state) {
  m_automaticLock = state;
  if (state) {
    qDebug() << "Enabling automatic lock";
  } else {
    qDebug() << "Disabling automatic lock";
  }
  emit automaticLockChanged(state);
}
void
Controller::setSwipeLength(int direction, int length) {
  switch (direction) {
    case SwipeDirection::Right:
      m_swipeLengthRight = length;
      emit swipeLengthRightChanged(length);
      break;
    case SwipeDirection::Left:
      m_swipeLengthLeft = length;
      emit swipeLengthLeftChanged(length);
      break;
    case SwipeDirection::Up:
      m_swipeLengthUp = length;
      emit swipeLengthUpChanged(length);
      break;
    case SwipeDirection::Down:
      m_swipeLengthDown = length;
      emit swipeLengthDownChanged(length);
      break;
    default:
      qDebug() << "Invalid swipe direction: " << direction;
      break;
  }
}
int
Controller::getSwipeLength(int direction) {
  switch (direction) {
    case SwipeDirection::Right:
      return swipeLengthRight();
    case SwipeDirection::Left:
      return swipeLengthLeft();
    case SwipeDirection::Up:
      return swipeLengthUp();
    case SwipeDirection::Down:
      return swipeLengthDown();
    default:
      qDebug() << "Invalid swipe direction: " << direction;
      return -1;
  }
}
void
Controller::setSleepAfter(int sleepAfter) {
  m_sleepAfter = sleepAfter;
  qDebug() << "Sleep After: " << sleepAfter << " minutes";
  emit sleepAfterChanged(m_sleepAfter);
}
void
Controller::setLockAfter(int lockAfter) {
  m_lockAfter = lockAfter;
  qDebug() << "Lock After: " << lockAfter << " minutes";
  emit lockAfterChanged(m_lockAfter);
}

#include "moc_controller.cpp"
