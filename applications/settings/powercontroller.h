#pragma once

#include <QObject>

#include <liboxide.h>
#include <liboxide/dbus.h>

#define OXIDE_SERVICE "codes.eeems.oxide1"

using namespace codes::eeems::oxide1;

enum SwipeDirection { None, Right, Left, Up, Down };

class PowerController : public QObject {
  Q_OBJECT
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

public:
  explicit PowerController(General* api, QObject* parent = nullptr)
    : QObject(parent)
    , api(api) {}

  void ensureApi() {
    if (apiReady || api == nullptr)
      return;
    auto reply = api->requestAPI("system");
    reply.waitForFinished();
    if (reply.isError())
      return;
    auto path = ((QDBusObjectPath)reply).path();
    if (path == "/")
      return;
    systemApi =
      new System(OXIDE_SERVICE, path, QDBusConnection::systemBus(), this);
    connect(systemApi, &System::autoSleepChanged, this, [this](int v) {
      m_automaticSleep = v > 0;
      m_sleepAfter = v;
      emit automaticSleepChanged(m_automaticSleep);
      emit sleepAfterChanged(m_sleepAfter);
    });
    // Load initial values
    auto autoSleep = systemApi->autoSleep();
    m_automaticSleep = autoSleep > 0;
    m_sleepAfter = autoSleep > 0 ? autoSleep : 5;
    auto autoLock = systemApi->autoLock();
    m_automaticLock = autoLock > 0;
    m_lockAfter = autoLock > 0 ? autoLock : 5;
    m_lockOnSuspend = systemApi->lockOnSuspend();
    apiReady = true;
    // Emit signals so QML bindings update with loaded values
    emit automaticSleepChanged(m_automaticSleep);
    emit sleepAfterChanged(m_sleepAfter);
    emit automaticLockChanged(m_automaticLock);
    emit lockAfterChanged(m_lockAfter);
    emit lockOnSuspendChanged(m_lockOnSuspend);
  }

  Q_INVOKABLE void init() { ensureApi(); }

  bool automaticSleep() const { return m_automaticSleep; }
  void setAutomaticSleep(bool v) {
    if (m_automaticSleep == v)
      return;
    m_automaticSleep = v;
    emit automaticSleepChanged(v);
    applyPowerSettings();
  }
  int sleepAfter() const { return m_sleepAfter; }
  void setSleepAfter(int v) {
    if (m_sleepAfter == v)
      return;
    m_sleepAfter = v;
    emit sleepAfterChanged(v);
    applyPowerSettings();
  }
  bool automaticLock() const { return m_automaticLock; }
  void setAutomaticLock(bool v) {
    if (m_automaticLock == v)
      return;
    m_automaticLock = v;
    emit automaticLockChanged(v);
    applyPowerSettings();
  }
  int lockAfter() const { return m_lockAfter; }
  void setLockAfter(int v) {
    if (m_lockAfter == v)
      return;
    m_lockAfter = v;
    emit lockAfterChanged(v);
    applyPowerSettings();
  }
  bool lockOnSuspend() const { return m_lockOnSuspend; }
  void setLockOnSuspend(bool v) {
    if (m_lockOnSuspend == v)
      return;
    m_lockOnSuspend = v;
    emit lockOnSuspendChanged(v);
    if (systemApi != nullptr) {
      systemApi->setLockOnSuspend(v);
    }
  }

signals:
  void automaticSleepChanged(bool);
  void sleepAfterChanged(int);
  void automaticLockChanged(bool);
  void lockAfterChanged(int);
  void lockOnSuspendChanged(bool);

private:
  General* api = nullptr;
  System* systemApi = nullptr;
  bool apiReady = false;

  bool m_automaticSleep = true;
  int m_sleepAfter = 5;
  bool m_automaticLock = true;
  int m_lockAfter = 5;
  bool m_lockOnSuspend = true;

  void applyPowerSettings() {
    ensureApi();
    if (systemApi == nullptr)
      return;
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
};
