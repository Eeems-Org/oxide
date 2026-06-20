#include "receiver.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
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
  if (!QFile::exists("/usr/sbin/wait-for-home")) {
    qCDebug(loggingCategory)
      << "wait-for-home missing, assuming safe to continue";
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

HookState
LockscreenHookReceiver::waitForHook(const QString& name) {
  auto path = QString("/home/root/.local/share/lockscreen_hook.d/%1").arg(name);
  auto info = QFileInfo(path);
  if (!info.exists()) {
    qCWarning(loggingCategory) << name << "hook missing";
    return HookState::Missing;
  }
  if (!info.isFile()) {
    qCWarning(loggingCategory) << name << "hook is not a file";
    return HookState::NotFile;
  }
  if (!info.isExecutable()) {
    qCWarning(loggingCategory) << name << "hook is not executable";
    return HookState::NoPermission;
  }
  int res = execute(path);
  if (res == 0) {
    qCDebug(loggingCategory) << name << "hook successfully ran";
    return HookState::Success;
  }
  switch (res) {
    case -2:
      qCWarning(loggingCategory) << name << "hook failed to start";
      return HookState::NoStart;
    case -1:
      qCWarning(loggingCategory) << name << "hook crashed";
      return HookState::Crash;
    default:
      qCWarning(loggingCategory)
        << name << "hook had non-zero exit code (" << res << ")";
      return HookState::Fail;
  }
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
