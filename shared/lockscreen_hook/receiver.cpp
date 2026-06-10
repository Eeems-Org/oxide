#include "receiver.h"

#include <QDebug>
#include <QProcess>
#include <QVariant>
#include <unistd.h>

Q_LOGGING_CATEGORY(loggingCategory, "lockscreen_hook");

LockscreenHookReceiver*
LockscreenHookReceiver::singleton() {
  static LockscreenHookReceiver* instance = new LockscreenHookReceiver();
  return instance;
}

LockscreenHookReceiver::LockscreenHookReceiver(QObject* parent)
  : QObject(parent) {}

void
LockscreenHookReceiver::onUnlocked() {
  qCDebug(
    loggingCategory
  ) << "xofm::libs::pincode::PasscodeHandler::unlocked() triggered";
  m_mutex.lock();
  if (!m_unlockNotified) {
    m_unlockNotified = true;
    m_cond.wakeAll();
  }
  m_mutex.unlock();
}

bool
LockscreenHookReceiver::waitForUnlock() {
  while (m_passcodeHandler == nullptr) {
    usleep(1000);
  }
  if (!m_passcodeHandler->property("hasPasscode").toBool()) {
    return waitForHome();
  }
  m_mutex.lock();
  while (!m_unlockNotified) {
    m_cond.wait(&m_mutex);
  }
  m_mutex.unlock();
  return waitForHome();
}

bool
LockscreenHookReceiver::setPasscodeHandler(QObject* passcodeHandler) {
  if (m_passcodeHandler != nullptr) {
    qCWarning(loggingCategory)
      << "xofm::libs::pincode::PasscodeHandler already hooked";
    return false;
  }
  m_passcodeHandler = passcodeHandler;
  return QObject::connect(
    passcodeHandler,
    SIGNAL(unlocked()),
    this,
    SLOT(onUnlocked()),
    Qt::DirectConnection
  );
}

bool
LockscreenHookReceiver::waitForHome() {
  QProcess process;
  process.setProgram("/usr/sbin/wait-for-home");
  process.start();
  return process.waitForStarted() && process.waitForFinished() &&
         process.exitStatus() != QProcess::CrashExit && process.exitCode() == 0;
}
