#include "receiver.h"

#include <QDebug>
#include <QProcess>
#include <QFile>
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
  emit unlocked();
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
LockscreenHookReceiver::hasPasscode() {
  while (m_passcodeHandler == nullptr) {
    usleep(1000);
  }
  return m_passcodeHandler->property("hasPasscode").toBool();
}

bool
LockscreenHookReceiver::waitForHome() {
  if(!QFile::exists("/usr/sbin/wait-for-home")){
    qCDebug(loggingCategory) << "wait-for-home missing, assuming safe to continue";
    return true;
  }
  int res = execute("/usr/sbin/wait-for-home");
  if (res == 0) {
    qCDebug(loggingCategory) << "wait-for-home successful, safe to continue";
    return true;
  }
  switch (res) {
    case -2:
      qCWarning(loggingCategory) << "wait-for-home failed to start, continuing";
      break;
    case -1:
      qCWarning(loggingCategory) << "wait-for-home crashed, continuing";
      break;
    default:
      qCWarning(loggingCategory)
        << "wait-for-home had non-zero exit code (" << res << "), continuing";
      break;
  }
  return false;
}

bool
LockscreenHookReceiver::waitForHook() {
  if(!QFile::exists("/home/root/.local/share/lockscreen_hook.d/unlock")){
    qCWarning(loggingCategory) << "Unlock hook missing, continuing";
    return false;
  }
  int res = execute("/home/root/.local/share/lockscreen_hook.d/unlock");
  if (res == 0) {
    return true;
  }
  switch (res) {
    case -2:
      qCWarning(loggingCategory) << "Unlock hook failed to start, continuing";
      break;
    case -1:
      qCWarning(loggingCategory) << "Unlock hook crashed, continuing";
      break;
    default:
      qCWarning(loggingCategory)
        << "Unlock hook had non-zero exit code (" << res << "), continuing";
      break;
  }
  return false;
}

int
LockscreenHookReceiver::execute(const QString& executable) {
  qCDebug(loggingCategory) << "Executing" << executable;
  QProcess process;
  process.setProcessChannelMode(QProcess::ForwardedChannels);
  process.start(executable);
  if (!process.waitForStarted()) {
    return -2;
  }
  if (!process.waitForFinished()) {
    return -1;
  }
  if (process.exitStatus() == QProcess::CrashExit) {
    return -1;
  }
  return process.exitCode();
}
