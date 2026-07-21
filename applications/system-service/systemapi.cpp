#include "systemapi.h"

#include <liboxide.h>
#include <liboxide/oxideqml.h>

#include <QString>

#include "appsapi.h"
#include "controller.h"
#include "dbusservice.h"
#include "frontlightapi.h"
#include "notificationapi.h"
#include "powerapi.h"
#include "wifiapi.h"

QDebug
operator<<(QDebug debug, const Touch& touch) {
  QDebugStateSaver saver(debug);
  debug.nospace() << touch.debugString().c_str();
  return debug.maybeSpace();
}
QDebug
operator<<(QDebug debug, Touch* touch) {
  QDebugStateSaver saver(debug);
  debug.nospace() << touch->debugString().c_str();
  return debug.maybeSpace();
}
#ifdef __aarch64__
static void
disableVPDD() {
  QString basePath;
  switch (deviceSettings.getDeviceType()) {
    case Oxide::DeviceSettings::RMPP:
      basePath = "/sys/bus/i2c/devices/1-0048";
      break;
    case Oxide::DeviceSettings::RMPPM:
      basePath = "/sys/bus/i2c/devices/0-0048";
      break;
    default:
      return;
  }
  Oxide::SysObject device(basePath);
  if (device.hasProperty("vpdd_length")) {
    device.setProperty("vpdd_length", "0");
    O_DEBUG(
      "Cancelled VPDD timer at" << device.propertyPath("vpdd_length").c_str()
    );
  }
}
#endif

void
SystemAPI::PrepareForSleep(bool suspending) {
  if (suspending) {
    if (reSuspend) {
      return;
    }
    Controller::singleton()->enabled = false;
    Oxide::Sentry::sentry_transaction(
      "Suspend System", "suspend", [this](Oxide::Sentry::Transaction* t) {
        if (autoLock()) {
          lockTimestamp =
            QDateTime::currentMSecsSinceEpoch() + lockTimer.remainingTime();
          O_DEBUG("Auto Lock timestamp:" << lockTimestamp);
        }
        O_INFO("Preparing for suspend...");
        Oxide::Sentry::sentry_span(
          t, "screen", "Update screen with suspend image", [this] {
            QImage img;
            for (const QString& path :
                 {xochitlSettings.SleepScreenPath(),
                  QStringLiteral("/usr/share/remarkable/sleeping.png"),
                  QStringLiteral("/usr/share/remarkable/suspended.png")}) {
              if (path.isEmpty() || !QFile::exists(path)) {
                continue;
              }
              img = QImage(path);
              if (!img.isNull()) {
                break;
              }
            }
            if (img.isNull()) {
              return;
            }
            m_buffer = createBuffer();
            if (m_buffer == nullptr) {
              return;
            }
            if (landscape()) {
              img = img.transformed(QTransform().rotate(90));
            }
            auto image = Oxide::QML::getImageForSurface(m_buffer);
            image.fill(Qt::white);
            QPainter painter(&image);
            painter.setRenderHints(
              QPainter::Antialiasing | QPainter::SmoothPixmapTransform, 1
            );
            auto rect = image.rect();
            if (rect == img.rect()) {
              painter.drawImage(rect, img, rect);
            } else {
              QPoint center(rect.width() / 2, rect.height() / 2);
              painter.translate(center);
              qreal scale = qMin(
                rect.width() / qreal(img.width()),
                rect.height() / qreal(img.height())
              );
              painter.scale(scale, scale);
              painter.translate(-img.width() / 2, -img.height() / 2);
              painter.drawImage(QPoint(0, 0), img);
              painter.end();
            }
            addSystemBuffer(m_buffer);
            Blight::connection()->raise(m_buffer);
            // Repaint to attempt to reduce ghosting
            Blight::connection()->repaint(
              m_buffer,
              Blight::WaveformMode::UI,
              Blight::ContentType::Color,
              Blight::UpdateMode::FullUpdate
            );
          }
        );
        Oxide::Sentry::sentry_span(t, "prepare", "Prepare for suspend", [this] {
          wifiAPI->stopUpdating();
          emit deviceSuspending();
          appsAPI->recordPreviousApplication();
          auto path = appsAPI->currentApplicationNoSecurityCheck();
          if (path.path() != "/") {
            resumeApp = appsAPI->getApplication(path);
            resumeApp->pauseNoSecurityCheck(false);
            O_INFO("Resume app set to " << resumeApp->name());
          } else {
            O_INFO("Unable to set resume app");
            resumeApp = nullptr;
          }
        });
        Oxide::Sentry::sentry_span(
          t, "disable", "Disable various services", [this] {
            if (wifiAPI->state() != WifiAPI::State::Off) {
              wifiWasOn = true;
#ifdef __arm__
              wifiAPI->disable();
#endif
            }
            getCompositorDBus()->waitForNoRepaints().waitForFinished();
            if (m_buffer != nullptr) {
              Blight::connection()->waitForMarker(m_buffer->surface);
            }
#ifdef __aarch64__
            disableVPDD();
#endif
            {
              auto* api = FrontlightAPI::singleton();
              if (api->hasFrontlightNoPermissionCheck()) {
                m_savedFrontlightBrightness = api->brightness();
                O_DEBUG(
                  "Saved frontlight brightness:" << m_savedFrontlightBrightness
                );
                api->setBrightnessNoPermissionCheck(0);
                O_DEBUG("Turned off frontlight for suspend");
              }
            }
            {
              QString sysPath =
                QStringLiteral("/sys/class/backlight/rm_keyboard_backlight");
              if (QFile::exists(sysPath)) {
                m_savedKeyboardBrightness =
                  Oxide::SysObject(sysPath).intProperty("brightness");
                O_DEBUG(
                  "Saved keyboard brightness:" << m_savedKeyboardBrightness
                );
                Oxide::SysObject(sysPath).setProperty("brightness", "0");
                O_DEBUG("Turned off keyboard backlight for suspend");
              }
            }
            releaseSleepInhibitors();
          }
        );
        O_INFO("Suspending...");
      }
    );
  } else {
    Oxide::Sentry::sentry_transaction(
      "Resume System", "resume", [this](Oxide::Sentry::Transaction* t) {
        if (!Controller::singleton()->shouldResume()) {
          O_INFO("Resuspending...");
          reSuspend = true;
          auto reply = systemd->Suspend(false);
          reply.waitForFinished();
          if (reply.isValid()) {
            return;
          }
          O_WARNING("Failed to resuspend:" << reply.error().message());
        }
        reSuspend = false;
        qDebug() << "Resuming...";
        Oxide::Sentry::sentry_span(t, "inhibit", "Inhibit sleep", [this] {
          inhibitSleep();
        });
        {
          auto* api = FrontlightAPI::singleton();
          if (
            api->hasFrontlightNoPermissionCheck() &&
            m_savedFrontlightBrightness > 0
          ) {
            api->setBrightnessNoPermissionCheck(m_savedFrontlightBrightness);
            O_DEBUG(
              "Restored frontlight brightness:" << m_savedFrontlightBrightness
            );
          }
        }
        {
          QString sysPath =
            QStringLiteral("/sys/class/backlight/rm_keyboard_backlight");
          if (m_savedKeyboardBrightness > 0 && QFile::exists(sysPath)) {
            Oxide::SysObject sysObject(sysPath);
            sysObject.setProperty(
              "brightness", std::to_string(m_savedKeyboardBrightness)
            );
            sysObject.setProperty("bl_power", "0");
            O_DEBUG(
              "Restored keyboard brightness:" << m_savedKeyboardBrightness
            );
          }
        }
        Oxide::Sentry::sentry_span(
          t,
          "resume",
          "Resume running application or go to lockscreen",
          [this] {
            auto now = QDateTime::currentMSecsSinceEpoch();
            bool lockTimeout = autoLock();
            if (lockTimeout) {
              O_DEBUG("Current timestamp:" << now);
              lockTimeout = now >= lockTimestamp;
            }
            if (lockOnSuspend() || lockTimeout) {
              if (lockTimeout) {
                O_DEBUG("Lock timer expired while suspended");
              } else {
                O_DEBUG("Always locking after suspend");
              }
              auto lockscreenApp =
                appsAPI->getApplication(appsAPI->lockscreenApplication());
              if (lockscreenApp != nullptr) {
                O_DEBUG("Resume app set to lockscreen application");
                resumeApp = lockscreenApp;
              }
            }
            if (resumeApp == nullptr) {
              O_DEBUG("Resume app set to startup application");
              resumeApp =
                appsAPI->getApplication(appsAPI->startupApplication());
            }
            if (resumeApp != nullptr) {
              resumeApp->resumeNoSecurityCheck();
            } else {
              O_WARNING("Unable to find an app to resume");
            }
          }
        );
        if (m_buffer != nullptr) {
          Blight::connection()->remove(m_buffer);
          m_buffer = nullptr;
        }
        Oxide::Sentry::sentry_span(t, "process", "Process events", [] {
          QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        });
        Oxide::Sentry::sentry_span(
          t, "enable", "Enable various services", [this] {
            emit deviceResuming();
            if (
              autoSleep() &&
              powerAPI->chargerState() != PowerAPI::ChargerConnected
            ) {
              O_DEBUG("Suspend timer re-enabled due to resume");
              suspendTimer.start(autoSleep() * 60 * 1000);
            }
            if (autoLock()) {
              O_DEBUG("Lock timer re-enabled due to resume");
              lockTimer.start(autoLock() * 60 * 1000);
            }
            if (wifiWasOn) {
              wifiAPI->enable();
            }
            wifiAPI->resumeUpdating();
          }
        );
        QProcess::execute("csl", QStringList() << "power" << "-s" << "run");
      }
    );
    Controller::singleton()->enabled = true;
  }
}
SystemAPI*
SystemAPI::singleton(SystemAPI* self) {
  static SystemAPI* instance;
  if (self != nullptr) {
    instance = self;
  }
  return instance;
}

SystemAPI::SystemAPI(QObject* parent)
  : APIBase(parent)
  , suspendTimer(this)
  , lockTimer(this)
  , activityTimer(this)
  , settings(this)
  , sleepInhibitors()
  , powerOffInhibitors()
  , mutex()
  , reSuspend{false}
  , wifiWasOn{false}
  , swipeStates()
  , swipeLengths() {
  Oxide::Sentry::sentry_transaction(
    "System API Init", "init", [this](Oxide::Sentry::Transaction* t) {
      Oxide::Sentry::sentry_span(
        t, "settings", "Sync settings", [this](Oxide::Sentry::Span* s) {
          Oxide::Sentry::sentry_span(
            s, "swipes", "Default swipe values", [this] {
              for (short i = Right; i <= Down; i++) {
                swipeStates[(SwipeDirection)i] = true;
              }
            }
          );
          Oxide::Sentry::sentry_span(s, "sync", "Sync settings", [this] {
            settings.sync();
          });
          Oxide::Sentry::sentry_span(
            s, "singleton", "Instantiate singleton", [this] { singleton(this); }
          );
          resumeApp = nullptr;
        }
      );
      Oxide::Sentry::sentry_span(
        t,
        "systemd",
        "Connect to SystemD DBus",
        [this](Oxide::Sentry::Span* s) {
          Oxide::Sentry::sentry_span(
            s, "manager", "Create manager object", [this] {
              systemd = new Manager(
                "org.freedesktop.login1",
                "/org/freedesktop/login1",
                QDBusConnection::systemBus(),
                this
              );
            }
          );
          Oxide::Sentry::sentry_span(
            s, "connect", "Connect to signals", [this] {
              // Handle Systemd signals
              connect(
                systemd,
                &Manager::PrepareForSleep,
                this,
                &SystemAPI::PrepareForSleep
              );
              connect(
                &suspendTimer,
                &QTimer::timeout,
                this,
                &SystemAPI::suspendTimeout
              );
              connect(
                &lockTimer, &QTimer::timeout, this, &SystemAPI::lockTimeout
              );
            }
          );
        }
      );
      Oxide::Sentry::sentry_span(
        t,
        "autoSleep",
        "Setup automatic sleep",
        [this](Oxide::Sentry::Span* s) {
          QSettings settings;
          if (QFile::exists(settings.fileName())) {
            O_INFO("Importing old settings");
            settings.sync();
            if (settings.contains("autoSleep")) {
              O_DEBUG("Importing old autoSleep");
              sharedSettings.set_autoSleep(settings.value("autoSleep").toInt());
            }
            int size = settings.beginReadArray("swipes");
            if (size) {
              sharedSettings.beginWriteArray("swipes");
              for (short i = Right; i <= Down && i < size; i++) {
                settings.setArrayIndex(i);
                sharedSettings.setArrayIndex(i);
                O_DEBUG(QString("Importing old swipe[%1]").arg(i));
                sharedSettings.setValue(
                  "enabled", settings.value("enabled", true)
                );
                sharedSettings.setValue("length", settings.value("length", 30));
              }
              sharedSettings.endArray();
            }
            settings.endArray();
            settings.remove("swipes");
            settings.sync();
            sharedSettings.sync();
          }
          if (autoSleep() < 0) {
            sharedSettings.set_autoSleep(0);
          } else if (autoSleep() > 10) {
            sharedSettings.set_autoSleep(10);
          }
          O_DEBUG("Auto Sleep" << autoSleep());
          Oxide::Sentry::sentry_span(s, "timer", "Setup timers", [this] {
            if (autoSleep()) {
              suspendTimer.start(autoSleep() * 60 * 1000);
            } else {
              suspendTimer.stop();
            }
            if (autoLock()) {
              lockTimer.start(autoLock() * 60 * 1000);
            } else {
              lockTimer.stop();
            }
          });
          connect(
            &sharedSettings,
            &Oxide::SharedSettings::autoSleepChanged,
            [this](int _autoSleep) { emit autoSleepChanged(_autoSleep); }
          );
          connect(&sharedSettings, &Oxide::SharedSettings::changed, [this]() {
            sharedSettings.beginReadArray("swipes");
            for (short i = Right; i <= Down; i++) {
              sharedSettings.setArrayIndex(i);
              swipeStates[(SwipeDirection)i] =
                sharedSettings.value("enabled", true).toBool();
              int length = sharedSettings.value("length", 30).toInt();
              swipeLengths[(SwipeDirection)i] = length;
              emit swipeLengthChanged(i, length);
            }
            sharedSettings.endArray();
          });
        }
      );
      Oxide::Sentry::sentry_span(t, "swipes", "Load swipe settings", [this]() {
        sharedSettings.beginReadArray("swipes");
        for (short i = Right; i <= Down; i++) {
          sharedSettings.setArrayIndex(i);
          swipeStates[(SwipeDirection)i] =
            sharedSettings.value("enabled", true).toBool();
          swipeLengths[(SwipeDirection)i] =
            sharedSettings.value("length", 30).toInt();
        }
        sharedSettings.endArray();
      });
      // Ask Systemd to tell us nicely when we are about to suspend or
      // resume
      Oxide::Sentry::sentry_span(
        t,
        "inhibit",
        "Inhibit sleep and power off",
        [this](Oxide::Sentry::Span* s) {
          Oxide::Sentry::sentry_span(
            s, "inhibitSleep", "Inhibit sleep", [this] { inhibitSleep(); }
          );
          Oxide::Sentry::sentry_span(
            s, "inhibitPowerOff", "Inhibit power off", [this] {
              inhibitPowerOff();
            }
          );
        }
      );
      qRegisterMetaType<input_event>();
      Oxide::Sentry::sentry_span(t, "input", "Connect input events", [this] {
        eventListener->append([this](QObject* object, QEvent* event) {
          Q_UNUSED(object);
          switch (event->type()) {
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::Shortcut:
            case QEvent::ShortcutOverride:
            case QEvent::TabletPress:
            case QEvent::TabletMove:
            case QEvent::TabletRelease:
            case QEvent::TabletTrackingChange:
            case QEvent::TabletEnterProximity:
            case QEvent::TabletLeaveProximity:
            case QEvent::TouchBegin:
            case QEvent::TouchCancel:
            case QEvent::TouchEnd:
            case QEvent::TouchUpdate:
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseMove:
            case QEvent::MouseTrackingChange:
            case QEvent::Wheel:
            case QEvent::Pointer:
            case QEvent::HoverEnter:
            case QEvent::HoverLeave:
            case QEvent::HoverMove:
            case QEvent::Gesture:
            case QEvent::GestureOverride:
            case QEvent::NonClientAreaMouseButtonDblClick:
            case QEvent::NonClientAreaMouseButtonPress:
            case QEvent::NonClientAreaMouseButtonRelease:
            case QEvent::NonClientAreaMouseMove:
              activity();
              break;
            default:
              break;
          }
          return false;
        });
        deviceSettings.onKeyboardAttachedChanged([this] {
          emit landscapeChanged(landscape());
        });
      });
      O_INFO("System API ready to use");
    }
  );
  activityTimer.setSingleShot(true);
  connect(&activityTimer, &QTimer::timeout, this, [this]() {
    auto active = suspendTimer.isActive();
    suspendTimer.stop();
    if (autoSleep() && powerAPI->m_chargerState != PowerAPI::ChargerConnected) {
      if (!active) {
        O_DEBUG("Suspend timer re-enabled due to activity");
      }
      suspendTimer.start(autoSleep() * 60 * 1000);
    } else if (active) {
      O_DEBUG("Suspend timer disabled");
    }
    active = lockTimer.isActive();
    if (autoLock()) {
      if (!active) {
        O_DEBUG("Lock timer re-enabled due to activity");
      }
      lockTimer.start(autoLock() * 60 * 1000);
    } else if (active) {
      O_DEBUG("Lock timer disabled");
    }
  });
}

void
SystemAPI::shutdown() {
  O_INFO("Removing all inhibitors");
  QMutableListIterator<Inhibitor> i(inhibitors);
  while (i.hasNext()) {
    auto inhibitor = i.next();
    inhibitor.release();
    i.remove();
  }
  delete systemd;
}

void
SystemAPI::setEnabled(bool enabled) {
  O_INFO("System API" << enabled);
}

int
SystemAPI::autoSleep() {
  return sharedSettings.autoSleep();
}

void
SystemAPI::setAutoSleep(int _autoSleep) {
  if (_autoSleep < 0 || _autoSleep > 360) {
    return;
  }
  O_INFO("Auto Sleep" << _autoSleep);
  sharedSettings.set_autoSleep(_autoSleep);
  if (_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected) {
    suspendTimer.setInterval(_autoSleep * 60 * 1000);
  } else if (!_autoSleep) {
    suspendTimer.stop();
  }
  sharedSettings.sync();
  emit autoSleepChanged(_autoSleep);
}

int
SystemAPI::autoLock() {
  return sharedSettings.autoLock();
}
void
SystemAPI::setAutoLock(int _autoLock) {
  if (_autoLock < 0 || _autoLock > 360) {
    return;
  }
  O_INFO("Auto Lock" << _autoLock);
  sharedSettings.set_autoLock(_autoLock);
  if (_autoLock) {
    lockTimer.setInterval(_autoLock * 60 * 1000);
  } else {
    lockTimer.stop();
  }
  sharedSettings.sync();
  emit autoLockChanged(_autoLock);
}

bool
SystemAPI::lockOnSuspend() {
  return sharedSettings.lockOnSuspend();
}
void
SystemAPI::setLockOnSuspend(bool _lockOnSuspend) {
  sharedSettings.set_lockOnSuspend(_lockOnSuspend);
  O_INFO("Lock on Suspend" << _lockOnSuspend);
  sharedSettings.sync();
  emit lockOnSuspendChanged(_lockOnSuspend);
}

bool
SystemAPI::sleepInhibited() {
  return sleepInhibitors.length();
}

bool
SystemAPI::powerOffInhibited() {
  return powerOffInhibitors.length();
}

bool
SystemAPI::landscape() {
  return deviceSettings.keyboardAttached();
}
void
SystemAPI::uninhibitAll(QString name) {
  if (powerOffInhibited()) {
    powerOffInhibitors.removeAll(name);
    if (!powerOffInhibited()) {
      emit powerOffInhibitedChanged(false);
    }
  }
  if (sleepInhibited()) {
    sleepInhibitors.removeAll(name);
    if (!sleepInhibited()) {
      emit sleepInhibitedChanged(false);
    }
  }
  if (
    !sleepInhibited() && autoSleep() &&
    powerAPI->chargerState() != PowerAPI::ChargerConnected &&
    !suspendTimer.isActive()
  ) {
    O_DEBUG("Suspend timer re-enabled due to uninhibit" << name);
    suspendTimer.start(autoSleep() * 60 * 1000);
  }
}

void
SystemAPI::stopSuspendTimer() {
  O_DEBUG("Suspend timer disabled");
  suspendTimer.stop();
}

void
SystemAPI::stopLockTimer() {
  O_DEBUG("Lock timer disabled");
  lockTimer.stop();
}
void
SystemAPI::startSuspendTimer() {
  if (
    autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected &&
    !suspendTimer.isActive()
  ) {
    O_DEBUG("Suspend timer re-enabled due to start Suspend timer");
    suspendTimer.start(autoSleep() * 60 * 1000);
  }
}
void
SystemAPI::startLockTimer() {
  if (autoLock() && !lockTimer.isActive()) {
    O_DEBUG("Lock timer re-enabled due to start lock timer");
    lockTimer.start(autoSleep() * 60 * 1000);
  }
}

void
SystemAPI::lock() {
  mutex.lock();
}

void
SystemAPI::unlock() {
  mutex.unlock();
}

void
SystemAPI::setSwipeEnabled(int direction, bool enabled) {
  if (!hasPermission("system")) {
    return;
  }
  if (direction <= SwipeDirection::None || direction > SwipeDirection::Down) {
    O_WARNING("Invalid swipe direction: " << direction);
    return;
  }
  setSwipeEnabled((SwipeDirection)direction, enabled);
}

void
SystemAPI::setSwipeEnabled(SwipeDirection direction, bool enabled) {
  if (direction == None) {
    return;
  }
  switch (direction) {
    case Left:
      O_INFO("Swipe Left: " << enabled);
      break;
    case Right:
      O_INFO("Swipe Right: " << enabled);
      break;
    case Up:
      O_INFO("Swipe Up: " << enabled);
      break;
    case Down:
      O_INFO("Swipe Down: " << enabled);
      break;
    default:
      return;
  }
  swipeStates[direction] = enabled;
  sharedSettings.beginWriteArray("swipes");
  for (short i = Right; i <= Down; i++) {
    sharedSettings.setArrayIndex(i);
    sharedSettings.setValue("enabled", swipeStates[(SwipeDirection)i]);
    sharedSettings.setValue("length", swipeLengths[(SwipeDirection)i]);
  }
  sharedSettings.endArray();
  sharedSettings.sync();
  emit swipeEnabledChanged(direction, enabled);
}

bool
SystemAPI::getSwipeEnabled(int direction) {
  if (!hasPermission("system")) {
    return false;
  }
  if (direction <= SwipeDirection::None || direction > SwipeDirection::Down) {
    O_WARNING("Invalid swipe direction: " << direction);
    return false;
  }
  return getSwipeEnabled(direction);
}

bool
SystemAPI::getSwipeEnabled(SwipeDirection direction) {
  return swipeStates[direction];
}

void
SystemAPI::toggleSwipeEnabled(int direction) {
  if (!hasPermission("system")) {
    return;
  }
  if (direction <= SwipeDirection::None || direction > SwipeDirection::Down) {
    O_WARNING("Invalid swipe direction: " << direction);
    return;
  }
  toggleSwipeEnabled((SwipeDirection)direction);
}

void
SystemAPI::toggleSwipeEnabled(SwipeDirection direction) {
  setSwipeEnabled(direction, !getSwipeEnabled(direction));
}

void
SystemAPI::setSwipeLength(int direction, int length) {
  int maxLength;
  if (!hasPermission("system")) {
    return;
  }
  if (direction <= SwipeDirection::None || direction > SwipeDirection::Down) {
    O_WARNING("Invalid swipe direction: " << direction);
    return;
  }
  if (direction == SwipeDirection::Up || direction == SwipeDirection::Down) {
    maxLength = deviceSettings.getTouchHeight();
  } else {
    maxLength = deviceSettings.getTouchWidth();
  }
  if (length < 0 || length > maxLength) {
    O_WARNING("Invalid swipe length: " << direction);
    return;
  }
  setSwipeLength((SwipeDirection)direction, length);
}

void
SystemAPI::setSwipeLength(SwipeDirection direction, int length) {
  if (direction == None) {
    return;
  }
  switch (direction) {
    case Left:
      O_INFO("Swipe Left Length: " << length);
      break;
    case Right:
      O_INFO("Swipe Right Length: " << length);
      break;
    case Up:
      O_INFO("Swipe Up Length: " << length);
      break;
    case Down:
      O_INFO("Swipe Down Length: " << length);
      break;
    default:
      return;
  }
  swipeLengths[direction] = length;
  sharedSettings.beginWriteArray("swipes");
  for (short i = Right; i <= Down; i++) {
    sharedSettings.setArrayIndex(i);
    sharedSettings.setValue("enabled", swipeStates[(SwipeDirection)i]);
    sharedSettings.setValue("length", swipeLengths[(SwipeDirection)i]);
  }
  sharedSettings.endArray();
  sharedSettings.sync();
  emit swipeLengthChanged(direction, length);
}

int
SystemAPI::getSwipeLength(int direction) {
  if (!hasPermission("system")) {
    return -1;
  }
  if (direction <= SwipeDirection::None || direction > SwipeDirection::Down) {
    O_WARNING("Invalid swipe direction: " << direction);
    return -1;
  }
  return getSwipeLength((SwipeDirection)direction);
}

int
SystemAPI::getSwipeLength(SwipeDirection direction) {
  return swipeLengths[direction];
}

void
SystemAPI::suspend() {
  if (sleepInhibited()) {
    O_INFO("Unable to suspend. Action is currently inhibited.");
    return;
  }
  O_INFO("Requesting Suspend...");
  systemd->Suspend(false).waitForFinished();
  O_INFO("Suspend requested.");
}

void
SystemAPI::powerOff() {
  if (powerOffInhibited()) {
    O_INFO("Unable to power off. Action is currently inhibited.");
    return;
  }
  O_INFO("Requesting Power off...");
  releasePowerOffInhibitors(true);
  systemd->PowerOff(false).waitForFinished();
  O_INFO("Power off requested");
}

void
SystemAPI::reboot() {
  if (powerOffInhibited()) {
    O_INFO("Unable to reboot. Action is currently inhibited.");
    return;
  }
  O_INFO("Requesting Reboot...");
  releasePowerOffInhibitors(true);
  systemd->Reboot(false).waitForFinished();
  O_INFO("Reboot requested");
}
void
SystemAPI::activity() {
  if (!activityTimer.isActive()) {
    activityTimer.start(1000);
  }
}

void
SystemAPI::inhibitSleep(QDBusMessage message) {
  if (!sleepInhibited()) {
    emit sleepInhibitedChanged(true);
  }
  O_DEBUG("Inhibiting sleep");
  suspendTimer.stop();
  sleepInhibitors.append(message.service());
  inhibitors.append(Inhibitor(
    systemd,
    "sleep:handle-suspend-key:idle",
    message.service(),
    "Application requested block",
    true
  ));
}

void
SystemAPI::uninhibitSleep(QDBusMessage message) {
  if (!sleepInhibited()) {
    return;
  }
  O_DEBUG("Uninhibiting sleep");
  sleepInhibitors.removeAll(message.service());
  if (
    !sleepInhibited() && autoSleep() &&
    powerAPI->chargerState() != PowerAPI::ChargerConnected
  ) {
    if (!suspendTimer.isActive()) {
      O_DEBUG(
        "Suspend timer re-enabled due to uninhibit sleep" << message.service()
      );
      suspendTimer.start(autoSleep() * 60 * 1000);
    }
    releaseSleepInhibitors(true);
  }
  if (!sleepInhibited()) {
    emit sleepInhibitedChanged(false);
  }
}

void
SystemAPI::inhibitPowerOff(QDBusMessage message) {
  if (!powerOffInhibited()) {
    emit powerOffInhibitedChanged(true);
  }
  O_DEBUG("Inhibiting poweroff");
  powerOffInhibitors.append(message.service());
  inhibitors.append(Inhibitor(
    systemd,
    "shutdown:handle-power-key",
    message.service(),
    "Application requested block",
    true
  ));
}

void
SystemAPI::uninhibitPowerOff(QDBusMessage message) {
  if (!powerOffInhibited()) {
    return;
  }
  O_DEBUG("Uninhibiting poweroff");
  powerOffInhibitors.removeAll(message.service());
  if (!powerOffInhibited()) {
    emit powerOffInhibitedChanged(false);
  }
}

void
SystemAPI::reload(QDBusMessage message) {
  O_INFO("Reloading overlays");
  dbusService->reload();
}

void
SystemAPI::suspendTimeout() {
  if (autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected) {
    O_INFO("Automatic suspend due to inactivity...");
    suspend();
  }
}

void
SystemAPI::lockTimeout() {
  if (autoLock()) {
    auto lockscreenApp =
      appsAPI->getApplication(appsAPI->lockscreenApplication());
    if (lockscreenApp != nullptr) {
      O_INFO("Automatic lock due to inactivity...");
      lockscreenApp->resumeNoSecurityCheck();
    }
  }
}

void
SystemAPI::inhibitSleep() {
  inhibitors.append(
    Inhibitor(systemd, "sleep", qApp->applicationName(), "Handle sleep screen")
  );
}

void
SystemAPI::inhibitPowerOff() {
  inhibitors.append(Inhibitor(
    systemd,
    "shutdown",
    qApp->applicationName(),
    "Block power off from any other method",
    true
  ));
}

void
SystemAPI::releaseSleepInhibitors(bool block) {
  QMutableListIterator<Inhibitor> i(inhibitors);
  while (i.hasNext()) {
    auto inhibitor = i.next();
    if (inhibitor.what.contains("sleep") && inhibitor.block == block) {
      inhibitor.release();
    }
    if (inhibitor.released()) {
      i.remove();
    }
  }
}

void
SystemAPI::releasePowerOffInhibitors(bool block) {
  QMutableListIterator<Inhibitor> i(inhibitors);
  while (i.hasNext()) {
    auto inhibitor = i.next();
    if (inhibitor.what.contains("shutdown") && inhibitor.block == block) {
      inhibitor.release();
    }
    if (inhibitor.released()) {
      i.remove();
    }
  }
}

void
SystemAPI::toggleSwipes() {
  bool state = !swipeStates[Up];
  setSwipeEnabled(Left, state);
  setSwipeEnabled(Right, state);
  setSwipeEnabled(Up, state);
  QString message = state ? "Swipes Enabled" : "Swipes Disabled";
  O_INFO(message);
  const QString& id = "system-swipe-toggle";
  auto notification =
    notificationAPI->add(id, OXIDE_SERVICE, "tarnish", message, "");
  if (notification == nullptr) {
    notification = notificationAPI->getByIdentifier(id);
    if (notification == nullptr) {
      return;
    }
  } else {
    connect(
      notification,
      &Notification::clicked,
      [notification](const QString& action) {
        Q_UNUSED(action);
        notification->remove();
      }
    );
  }
  notification->setText(message);
  notification->display();
}

Inhibitor::Inhibitor(
  Manager* systemd,
  QString what,
  QString who,
  QString why,
  bool block
)
  : who(who)
  , what(what)
  , why(why)
  , block(block) {
  O_DEBUG("Inhibiting" << what);
  QDBusUnixFileDescriptor reply =
    systemd->Inhibit(what, who, why, block ? "block" : "delay");
  if (reply.isValid()) {
    fd = reply.takeFileDescriptor();
    return;
  }
  QDBusError error = systemd->lastError();
  if (error.isValid()) {
    O_WARNING(
      "Failed to inhibit" << what << ":" << error.name() << error.message()
    );
  } else {
    O_WARNING("Failed to inhibit" << what << ": invalid reply");
  }
}

void
Inhibitor::release() {
  if (released()) {
    return;
  }
  O_DEBUG("Uninhibiting" << what);
  close(fd);
  fd = -1;
}

bool
Inhibitor::released() {
  return fd == -1;
}
#include "moc_systemapi.cpp"
