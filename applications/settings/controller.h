#pragma once

#include <liboxide.h>
#include <liboxide/sysobject.h>
#include <signal.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QVariant>

#include "wifinetworklist.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

using namespace codes::eeems::oxide1;
using codes::eeems::oxide1::Frontlight;
using namespace Oxide;
using namespace Oxide::Sentry;

enum WifiState {
  WifiUnknown,
  WifiOff,
  WifiDisconnected,
  WifiOffline,
  WifiOnline
};

enum SwipeDirection { None, Right, Left, Up, Down };

class Controller : public QObject {
  Q_OBJECT
  // === General ===
  Q_PROPERTY(QStringList locales READ locales NOTIFY localesChanged)
  Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
  Q_PROPERTY(QStringList timezones READ timezones NOTIFY timezonesChanged)
  Q_PROPERTY(
    QString timezone READ timezone WRITE setTimezone NOTIFY timezoneChanged
  )
  // === Display ===
  Q_PROPERTY(
    bool showBatteryPercent MEMBER m_showBatteryPercent WRITE
      setShowBatteryPercent NOTIFY showBatteryPercentChanged
  )
  Q_PROPERTY(
    bool showBatteryTemperature MEMBER m_showBatteryTemperature WRITE
      setShowBatteryTemperature NOTIFY showBatteryTemperatureChanged
  )
  Q_PROPERTY(
    bool showWifiDb MEMBER m_showWifiDb WRITE setShowWifiDb NOTIFY
      showWifiDbChanged
  )
  Q_PROPERTY(
    bool showDate MEMBER m_showDate WRITE setShowDate NOTIFY showDateChanged
  )
  Q_PROPERTY(int columns MEMBER m_columns WRITE setColumns NOTIFY columnsChanged)
  Q_PROPERTY(bool hasFrontlight READ hasFrontlight CONSTANT)
  Q_PROPERTY(
    bool extraBrightness READ extraBrightness WRITE setExtraBrightness NOTIFY
      extraBrightnessChanged
  )
  // === Power ===
  Q_PROPERTY(
    bool automaticSleep MEMBER m_automaticSleep WRITE setAutomaticSleep NOTIFY
      automaticSleepChanged
  )
  Q_PROPERTY(
    int sleepAfter MEMBER m_sleepAfter WRITE setSleepAfter NOTIFY
      sleepAfterChanged
  )
  Q_PROPERTY(
    bool automaticLock MEMBER m_automaticLock WRITE setAutomaticLock NOTIFY
      automaticLockChanged
  )
  Q_PROPERTY(
    int lockAfter MEMBER m_lockAfter WRITE setLockAfter NOTIFY lockAfterChanged
  )
  Q_PROPERTY(
    bool lockOnSuspend MEMBER m_lockOnSuspend WRITE setLockOnSuspend NOTIFY
      lockOnSuspendChanged
  )
  // === WiFi ===
  Q_PROPERTY(bool wifiOn MEMBER m_wifion READ wifiOn NOTIFY wifiOnChanged)
  Q_PROPERTY(
    WifiNetworkList* networks MEMBER networks READ getNetworks NOTIFY
      networksChanged
  )
  // === Lockscreen ===
  Q_PROPERTY(QString pin READ pin WRITE setPin NOTIFY pinChanged)
  // === Input ===
  Q_PROPERTY(
    int swipeLengthRight MEMBER m_swipeLengthRight WRITE setSwipeLengthRight
      NOTIFY swipeLengthRightChanged
  )
  Q_PROPERTY(
    int swipeLengthLeft MEMBER m_swipeLengthLeft WRITE setSwipeLengthLeft
      NOTIFY swipeLengthLeftChanged
  )
  Q_PROPERTY(
    int swipeLengthUp MEMBER m_swipeLengthUp WRITE setSwipeLengthUp NOTIFY
      swipeLengthUpChanged
  )
  Q_PROPERTY(
    int swipeLengthDown MEMBER m_swipeLengthDown WRITE setSwipeLengthDown
      NOTIFY swipeLengthDownChanged
  )
  Q_PROPERTY(int maxTouchWidth READ maxTouchWidth CONSTANT)
  Q_PROPERTY(int maxTouchHeight READ maxTouchHeight CONSTANT)
  // === Notifications ===
  Q_PROPERTY(
    uint notificationDisplayTime MEMBER m_notificationDisplayTime WRITE
      setNotificationDisplayTime NOTIFY notificationDisplayTimeChanged
  )

public:
  explicit Controller(QObject* parent = 0)
    : QObject(parent)
    , m_wifion(false)
    , networks(new WifiNetworkList()) {
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

    // System API
    auto reply = api->requestAPI("system");
    reply.waitForFinished();
    if (!reply.isError()) {
      auto path = ((QDBusObjectPath)reply).path();
      if (path != "/") {
        systemApi = new System(OXIDE_SERVICE, path, bus);
        connect(
          systemApi,
          &System::autoSleepChanged,
          this,
          [this](int v) { setSleepAfter(v); }
        );
        connect(
          systemApi,
          &System::swipeLengthChanged,
          this,
          [this](int d, int l) { setSwipeLength(d, l); }
        );
        // Load initial values
        auto autoSleep = systemApi->autoSleep();
        m_automaticSleep = autoSleep > 0;
        m_sleepAfter = autoSleep > 0 ? autoSleep : 5;
        auto autoLock = systemApi->autoLock();
        m_automaticLock = autoLock > 0;
        m_lockAfter = autoLock > 0 ? autoLock : 5;
        m_lockOnSuspend = systemApi->lockOnSuspend();
        for (short i = 1; i <= 4; ++i) {
          setSwipeLength(i, systemApi->getSwipeLength(i));
        }
      }
    }

    // WiFi API
    reply = api->requestAPI("wifi");
    reply.waitForFinished();
    if (!reply.isError()) {
      auto path = ((QDBusObjectPath)reply).path();
      if (path != "/") {
        wifiApi = new Wifi(OXIDE_SERVICE, path, bus);
        connect(wifiApi, &Wifi::stateChanged, this, &Controller::wifiStateChanged);
        connect(
          wifiApi, &Wifi::networkConnected, this, &Controller::networkConnected
        );
        connect(wifiApi, &Wifi::disconnected, this, &Controller::disconnected);
        networks->setAPI(wifiApi);
        auto state = wifiApi->state();
        m_wifion = state != WifiState::WifiOff && state != WifiState::WifiUnknown;
      }
    }

    // Notification API
    reply = api->requestAPI("notification");
    reply.waitForFinished();
    if (!reply.isError()) {
      auto path = ((QDBusObjectPath)reply).path();
      if (path != "/") {
        notificationApi = new Notifications(OXIDE_SERVICE, path, bus);
        m_notificationDisplayTime = notificationApi->displayTime();
        connect(
          notificationApi,
          &Notifications::displayTimeChanged,
          this,
          [this](uint v) { setNotificationDisplayTime(v); }
        );
      }
    }

    // Frontlight API (optional)
    reply = api->requestAPI("frontlight");
    reply.waitForFinished();
    if (!reply.isError()) {
      auto path = ((QDBusObjectPath)reply).path();
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

    // Load oxide.conf settings
    loadConfigFile();

    // Setup signal handlers for backgroundable support
    SignalHandler::setup_unix_signal_handlers();
    signalHandler->removeNotifier(SIGTERM);
    signalHandler->removeNotifier(SIGINT);
    signalHandler->removeNotifier(SIGCONT);
    signalHandler->removeNotifier(SIGPIPE);
    signalHandler->removeNotifier(SIGSEGV);
    signalHandler->removeNotifier(SIGBUS);
    connect(signalHandler, &SignalHandler::sigUsr1, this, &Controller::sigUsr1);
    connect(signalHandler, &SignalHandler::sigUsr2, this, &Controller::sigUsr2);
  }

  // === Standard methods ===
  Q_INVOKABLE void startup() {
    loadConfigFile();
    emit localesChanged();
    emit timezonesChanged();
  }
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
  void setRoot(QObject* root) { this->root = root; }

  // === Category navigation ===
  Q_INVOKABLE void navigateToCategory(QString name) {
    emit categoryRequested(name);
  }

  // === General ===
  QStringList locales() { return deviceSettings.getLocales(); }
  QString locale() { return deviceSettings.getLocale(); }
  void setLocale(const QString& locale) {
    deviceSettings.setLocale(locale);
    emit localeChanged(locale);
  }
  QStringList timezones() { return deviceSettings.getTimezones(); }
  QString timezone() { return deviceSettings.getTimezone(); }
  void setTimezone(const QString& timezone) {
    deviceSettings.setTimezone(timezone);
    emit timezoneChanged(timezone);
  }

  // === Display ===
  bool showBatteryPercent() const { return m_showBatteryPercent; }
  void setShowBatteryPercent(bool v) {
    if (m_showBatteryPercent == v) return;
    m_showBatteryPercent = v;
    emit showBatteryPercentChanged(v);
    saveConfigFile();
  }
  bool showBatteryTemperature() const { return m_showBatteryTemperature; }
  void setShowBatteryTemperature(bool v) {
    if (m_showBatteryTemperature == v) return;
    m_showBatteryTemperature = v;
    emit showBatteryTemperatureChanged(v);
    saveConfigFile();
  }
  bool showWifiDb() const { return m_showWifiDb; }
  void setShowWifiDb(bool v) {
    if (m_showWifiDb == v) return;
    m_showWifiDb = v;
    emit showWifiDbChanged(v);
    saveConfigFile();
  }
  bool showDate() const { return m_showDate; }
  void setShowDate(bool v) {
    if (m_showDate == v) return;
    m_showDate = v;
    emit showDateChanged(v);
    saveConfigFile();
  }
  int columns() const { return m_columns; }
  void setColumns(int v) {
    if (m_columns == v) return;
    m_columns = v;
    emit columnsChanged(v);
    saveConfigFile();
  }
  bool hasFrontlight() {
    return frontlightApi != nullptr && frontlightApi->hasFrontlight();
  }
  bool extraBrightness() {
    return frontlightApi != nullptr && frontlightApi->extraBrightness();
  }
  void setExtraBrightness(bool enabled) {
    if (frontlightApi != nullptr) {
      frontlightApi->setExtraBrightness(enabled);
    }
  }
  int maxTouchWidth() { return deviceSettings.getTouchWidth() * 0.9; }
  int maxTouchHeight() { return deviceSettings.getTouchHeight() * 0.9; }

  // === Power ===
  bool automaticSleep() const { return m_automaticSleep; }
  void setAutomaticSleep(bool v) {
    if (m_automaticSleep == v) return;
    m_automaticSleep = v;
    emit automaticSleepChanged(v);
    applyPowerSettings();
  }
  int sleepAfter() const { return m_sleepAfter; }
  void setSleepAfter(int v) {
    if (m_sleepAfter == v) return;
    m_sleepAfter = v;
    emit sleepAfterChanged(v);
    applyPowerSettings();
  }
  bool automaticLock() const { return m_automaticLock; }
  void setAutomaticLock(bool v) {
    if (m_automaticLock == v) return;
    m_automaticLock = v;
    emit automaticLockChanged(v);
    applyPowerSettings();
  }
  int lockAfter() const { return m_lockAfter; }
  void setLockAfter(int v) {
    if (m_lockAfter == v) return;
    m_lockAfter = v;
    emit lockAfterChanged(v);
    applyPowerSettings();
  }
  bool lockOnSuspend() const { return m_lockOnSuspend; }
  void setLockOnSuspend(bool v) {
    if (m_lockOnSuspend == v) return;
    m_lockOnSuspend = v;
    emit lockOnSuspendChanged(v);
    if (systemApi != nullptr) {
      systemApi->setLockOnSuspend(v);
    }
  }

  // === WiFi ===
  bool wifiOn() { return m_wifion; }
  Q_INVOKABLE bool turnOnWifi() {
    if (wifiApi == nullptr || !wifiApi->enable()) {
      return false;
    }
    m_wifion = true;
    emit wifiOnChanged(true);
    return true;
  }
  Q_INVOKABLE void turnOffWifi() {
    if (wifiApi == nullptr) return;
    wifiApi->disable();
    m_wifion = false;
    emit wifiOnChanged(false);
    networks->removeUnknown();
    wifiApi->flushBSSCache(0);
  }
  WifiNetworkList* getNetworks() { return networks; }
  Q_INVOKABLE void disconnectWifiSignals() {
    if (wifiApi == nullptr) return;
    disconnect(wifiApi, &Wifi::bssFound, this, &Controller::bssFound);
    disconnect(wifiApi, &Wifi::bssRemoved, this, &Controller::bssRemoved);
    disconnect(wifiApi, &Wifi::networkAdded, this, &Controller::networkAdded);
    disconnect(
      wifiApi, &Wifi::networkRemoved, this, &Controller::networkRemoved
    );
  }
  Q_INVOKABLE void connectWifiSignals() {
    if (wifiApi == nullptr) return;
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

  // === Lockscreen ===
  QString pin() { return sharedSettings.pin(); }
  void setPin(const QString& v) {
    if (sharedSettings.pin() == v) return;
    sharedSettings.set_pin(v);
    emit pinChanged(v);
  }

  // === Input ===
  int swipeLengthRight() const { return m_swipeLengthRight; }
  void setSwipeLengthRight(int v) { setSwipeLength(1, v); }
  int swipeLengthLeft() const { return m_swipeLengthLeft; }
  void setSwipeLengthLeft(int v) { setSwipeLength(2, v); }
  int swipeLengthUp() const { return m_swipeLengthUp; }
  void setSwipeLengthUp(int v) { setSwipeLength(3, v); }
  int swipeLengthDown() const { return m_swipeLengthDown; }
  void setSwipeLengthDown(int v) { setSwipeLength(4, v); }

  // === Notifications ===
  uint notificationDisplayTime() const { return m_notificationDisplayTime; }
  void setNotificationDisplayTime(uint v) {
    if (m_notificationDisplayTime == v) return;
    m_notificationDisplayTime = v;
    emit notificationDisplayTimeChanged(v);
    if (notificationApi != nullptr) {
      notificationApi->setDisplayTime(v);
    }
  }

signals:
  // Category navigation
  void categoryRequested(QString name);
  // General
  void localesChanged();
  void localeChanged(QString);
  void timezonesChanged();
  void timezoneChanged(QString);
  // Display
  void showBatteryPercentChanged(bool);
  void showBatteryTemperatureChanged(bool);
  void showWifiDbChanged(bool);
  void showDateChanged(bool);
  void columnsChanged(int);
  void extraBrightnessChanged(bool);
  // Power
  void automaticSleepChanged(bool);
  void sleepAfterChanged(int);
  void automaticLockChanged(bool);
  void lockAfterChanged(int);
  void lockOnSuspendChanged(bool);
  // WiFi
  void wifiOnChanged(bool);
  void networksChanged(WifiNetworkList*);
  // Lockscreen
  void pinChanged(QString);
  // Input
  void swipeLengthRightChanged(int);
  void swipeLengthLeftChanged(int);
  void swipeLengthUpChanged(int);
  void swipeLengthDownChanged(int);
  // Notifications
  void notificationDisplayTimeChanged(uint);

private slots:
  void sigUsr1() {
    ::kill(api->tarnishPid(), SIGUSR1);
    qDebug() << "Settings foregrounded";
  }
  void sigUsr2() {
    qDebug() << "Settings backgrounded";
    QTimer::singleShot(0, [this] { ::kill(api->tarnishPid(), SIGUSR2); });
  }
  void wifiStateChanged(int state) {
    bool on = state != WifiState::WifiOff && state != WifiState::WifiUnknown;
    if (m_wifion != on) {
      m_wifion = on;
      emit wifiOnChanged(on);
    }
  }
  void networkConnected(const QDBusObjectPath& path) {
    networks->setConnected(path);
  }
  void disconnected() {
    networks->setConnected(QDBusObjectPath("/"));
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
  void networkRemoved(const QDBusObjectPath& path) { networks->remove(path); }

private:
  // D-Bus API members
  General* api = nullptr;
  System* systemApi = nullptr;
  Wifi* wifiApi = nullptr;
  Notifications* notificationApi = nullptr;
  Frontlight* frontlightApi = nullptr;

  // State
  bool m_wifion;
  int wifiState = WifiUnknown;
  QObject* root = nullptr;
  WifiNetworkList* networks;

  // === Display settings (oxide.conf) ===
  bool m_showBatteryPercent = false;
  bool m_showBatteryTemperature = false;
  bool m_showWifiDb = false;
  bool m_showDate = false;
  int m_columns = 6;

  // === Power settings (D-Bus) ===
  bool m_automaticSleep = true;
  int m_sleepAfter = 5;
  bool m_automaticLock = true;
  int m_lockAfter = 5;
  bool m_lockOnSuspend = true;

  // === Input settings (D-Bus) ===
  int m_swipeLengthRight = 30;
  int m_swipeLengthLeft = 30;
  int m_swipeLengthUp = 30;
  int m_swipeLengthDown = 30;

  // === Notification settings (D-Bus) ===
  uint m_notificationDisplayTime = 5;

  void setSwipeLength(int direction, int length) {
    switch (direction) {
      case Right:
        if (m_swipeLengthRight == length) return;
        m_swipeLengthRight = length;
        emit swipeLengthRightChanged(length);
        break;
      case Left:
        if (m_swipeLengthLeft == length) return;
        m_swipeLengthLeft = length;
        emit swipeLengthLeftChanged(length);
        break;
      case Up:
        if (m_swipeLengthUp == length) return;
        m_swipeLengthUp = length;
        emit swipeLengthUpChanged(length);
        break;
      case Down:
        if (m_swipeLengthDown == length) return;
        m_swipeLengthDown = length;
        emit swipeLengthDownChanged(length);
        break;
      default:
        return;
    }
    if (systemApi != nullptr) {
      systemApi->setSwipeLength(direction, length);
    }
  }

  void applyPowerSettings() {
    if (systemApi == nullptr) return;
    if (!m_automaticSleep) {
      systemApi->setAutoSleep(0);
    } else {
      systemApi->setAutoSleep(m_sleepAfter);
    }
    if (!m_automaticLock) {
      systemApi->setAutoLock(0);
    } else {
      systemApi->setAutoLock(m_lockAfter);
    }
  }

  // === oxide.conf file handling ===
  QList<QString> configFileDirectoryPaths = {
    "/opt/etc", "/etc", "/home/root/.config", "/home/root/.vellum/etc"
  };

  QFile* getConfigFile() {
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

  void loadConfigFile() {
    auto configFile = getConfigFile();
    if (configFile == nullptr) return;
    if (!configFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
      delete configFile;
      return;
    }
    QSet<QString> booleanSettings = {
      "showWifiDb", "showBatteryPercent", "showBatteryTemperature", "showDate"
    };
    QSet<QString> intSettings = {"columns"};
    QTextStream in(configFile);
    while (!in.atEnd()) {
      QString line = in.readLine();
      if (line.startsWith("#") || line.isEmpty()) continue;
      QStringList parts = line.split("=");
      if (parts.length() != 2) continue;
      QString lhs = parts.at(0).trimmed();
      QString rhs = parts.at(1).trimmed();
      if (booleanSettings.contains(lhs)) {
        bool value = rhs.toLower() == "true" || rhs.toLower() == "t" ||
                     rhs.toLower() == "y" || rhs.toLower() == "yes" ||
                     rhs == "1";
        setProperty(lhs.toStdString().c_str(), value);
      } else if (intSettings.contains(lhs)) {
        setProperty(lhs.toStdString().c_str(), rhs.toInt());
      }
    }
    configFile->close();
    delete configFile;
  }

  void saveConfigFile() {
    QFile* configFile = getConfigFile();
    if (configFile == nullptr) {
      configFile =
        new QFile(configFileDirectoryPaths.last() + "/oxide.conf");
    }
    // Read existing content, update values, write back
    QMap<QString, QString> entries;
    if (configFile->exists()) {
      if (configFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(configFile);
        while (!in.atEnd()) {
          QString line = in.readLine();
          if (line.startsWith("#") || line.isEmpty()) {
            entries[line] = "";
            continue;
          }
          QStringList parts = line.split("=");
          if (parts.length() == 2) {
            entries[parts.at(0).trimmed()] = parts.at(1).trimmed();
          }
        }
        configFile->close();
      }
    }
    // Update values
    entries["showBatteryPercent"] = m_showBatteryPercent ? "yes" : "no";
    entries["showBatteryTemperature"] = m_showBatteryTemperature ? "yes" : "no";
    entries["showWifiDb"] = m_showWifiDb ? "yes" : "no";
    entries["showDate"] = m_showDate ? "yes" : "no";
    entries["columns"] = QString::number(m_columns);
    // Write back
    if (!configFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
      delete configFile;
      return;
    }
    QTextStream out(configFile);
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
      if (it.value().isEmpty()) {
        out << it.key() << "\n";
      } else {
        out << it.key() << "=" << it.value() << "\n";
      }
    }
    configFile->close();
    delete configFile;
  }
};
