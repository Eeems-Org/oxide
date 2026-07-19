#include "controller.h"

#include <liboxide.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <fstream>
#include <memory>
#include <sstream>

QList<QString> configDirectoryPaths =
  {"/opt/etc/draft", "/etc/draft", "/home/root/.config/draft"};
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
