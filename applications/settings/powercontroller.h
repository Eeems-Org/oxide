#pragma once

#include <QObject>

#include <liboxide.h>
#include <liboxide/dbus.h>

#define OXIDE_SERVICE "codes.eeems.oxide1"

using namespace codes::eeems::oxide1;

class PowerController : public QObject {
  Q_OBJECT
  Q_PROPERTY(
    bool automaticSleep READ automaticSleep WRITE setAutomaticSleep NOTIFY
      automaticSleepChanged
  )
  Q_PROPERTY(
    int sleepAfter READ sleepAfter WRITE setSleepAfter NOTIFY sleepAfterChanged
  )
  Q_PROPERTY(
    bool automaticLock READ automaticLock WRITE setAutomaticLock NOTIFY
      automaticLockChanged
  )
  Q_PROPERTY(
    int lockAfter READ lockAfter WRITE setLockAfter NOTIFY lockAfterChanged
  )
  Q_PROPERTY(
    bool lockOnSuspend READ lockOnSuspend WRITE setLockOnSuspend NOTIFY
      lockOnSuspendChanged
  )

public:
  explicit PowerController(QObject* parent, System* systemApi)
    : QObject(parent)
    , systemApi(systemApi) {
    deactivate();
    connect(systemApi, &System::autoSleepChanged, this, [this](int) {
      emit automaticSleepChanged(automaticSleep());
      emit sleepAfterChanged(sleepAfter());
    });
    connect(systemApi, &System::autoLockChanged, this, [this](int) {
      emit automaticLockChanged(automaticLock());
      emit lockAfterChanged(lockAfter());
    });
    connect(
      systemApi,
      &System::lockOnSuspendChanged,
      this,
      &PowerController::lockOnSuspendChanged
    );
  }

  Q_INVOKABLE void activate() {
    systemApi->blockSignals(false);
    emit automaticSleepChanged(automaticSleep());
    emit sleepAfterChanged(sleepAfter());
    emit automaticLockChanged(automaticLock());
    emit lockAfterChanged(lockAfter());
    emit lockOnSuspendChanged(lockOnSuspend());
  }

  Q_INVOKABLE void deactivate() { systemApi->blockSignals(true); }

  bool automaticSleep() { return systemApi->autoSleep() > 0; }
  void setAutomaticSleep(bool automaticSleep) {
    if (!automaticSleep) {
      systemApi->setAutoSleep(0);
    } else {
      auto current = systemApi->autoSleep();
      systemApi->setAutoSleep(current > 0 ? current : 5);
    }
  }
  int sleepAfter() {
    auto autoSleep = systemApi->autoSleep();
    return autoSleep > 0 ? autoSleep : 5;
  }
  void setSleepAfter(int sleepAfter) { systemApi->setAutoSleep(sleepAfter); }
  bool automaticLock() { return systemApi->autoLock() > 0; }
  void setAutomaticLock(bool automaticLock) {
    if (!automaticLock) {
      systemApi->setAutoLock(0);
    } else {
      auto current = systemApi->autoLock();
      systemApi->setAutoLock(current > 0 ? current : 5);
    }
  }
  int lockAfter() {
    auto autoLock = systemApi->autoLock();
    return autoLock > 0 ? autoLock : 5;
  }
  void setLockAfter(int lockAfter) { systemApi->setAutoLock(lockAfter); }
  bool lockOnSuspend() { return systemApi->lockOnSuspend(); }
  void setLockOnSuspend(bool lockOnSuspend) {
    systemApi->setLockOnSuspend(lockOnSuspend);
  }

signals:
  void automaticSleepChanged(bool);
  void sleepAfterChanged(int);
  void automaticLockChanged(bool);
  void lockAfterChanged(int);
  void lockOnSuspendChanged(bool);

private:
  System* systemApi = nullptr;
};
