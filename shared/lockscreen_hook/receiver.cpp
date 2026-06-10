#include "receiver.h"

#include <QDebug>
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

void
LockscreenHookReceiver::waitForUnlock() {
  while (m_passcodeHandler == nullptr) {
    usleep(1000);
  }
  if (!m_passcodeHandler->property("hasPasscode").toBool()) {
    return;
  }
  m_mutex.lock();
  while (!m_unlockNotified) {
    m_cond.wait(&m_mutex);
  }
  m_mutex.unlock();
}

bool
LockscreenHookReceiver::setPasscodeHandler(QObject* passcodeHandler) {
  if (m_passcodeHandler != nullptr) {
    qCWarning(loggingCategory)
      << "xofm::libs::pincode::PasscodeHandler already hooked";
    return false;
  }
  m_passcodeHandler = passcodeHandler;
  if (!m_passcodeHandler->property("hasPasscode").toBool()) {
    qCDebug(loggingCategory)
      << "xofm::libs::pincode::PasscodeHandler::hasPasscode is false";
  }
  return QObject::connect(
    passcodeHandler,
    SIGNAL(unlocked()),
    this,
    SLOT(onUnlocked()),
    Qt::DirectConnection
  );
}
