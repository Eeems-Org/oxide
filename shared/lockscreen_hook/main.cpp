#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <atomic>
#include <csignal>
#include <cstdlib>
#endif
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>

#include "receiver.h"

#define DEBUG

#ifdef DEBUG
#include <execinfo.h>
#include <signal.h>
#include <string.h>
#include <ucontext.h>
#endif

#ifdef DEBUG
#if defined(__arm__) || defined(__aarch64__) || defined(__x86_64__)
void
__signal_handler(int signal, siginfo_t* si, void* vcontext) {
  Q_UNUSED(si);
  constexpr int depth = 10;
  auto uc = static_cast<ucontext_t*>(vcontext);
  void* caller_address =
#if defined(__arm__)
    reinterpret_cast<void*>(uc->uc_mcontext.arm_pc);
#elif defined(__aarch64__)
    reinterpret_cast<void*>(uc->uc_mcontext.pc);
#elif defined(__x86_64__)
    (void*)uc->uc_mcontext.gregs[REG_RIP];
#else
    nullptr;
  _Exit(128 + signal);
#endif
  void* array[depth];
  size_t size = backtrace(array, depth);
  const char* msg = strsignal(signal);
  ::write(STDERR_FILENO, msg, std::strlen(msg));
  ::write(STDERR_FILENO, "\n", std::strlen("\n"));
  if (size > 1) {
    array[1] = caller_address;
    backtrace_symbols_fd(array + 1, size - 1, STDERR_FILENO);
  } else {
    const char* msg2 = "Unable to get backtrace\n";
    ::write(STDERR_FILENO, msg2, std::strlen(msg2));
  }
  _Exit(128 + signal);
}
#endif
#endif

static std::atomic<bool> enabled = false;
static std::atomic<bool> pendingExit = false;
static std::atomic<bool> passcodeHandlerHooked = false;
static std::atomic<bool> disablePoweroffScreen = false;

void
handle_unlock() {
  if (!enabled) {
    return;
  }
  auto* receiver = LockscreenHookReceiver::singleton();
  if (receiver == nullptr) {
    return;
  }
  if (!receiver->waitForHome()) {
    return;
  }
  if (receiver->waitForHook("unlock") != HookState::Success) {
    qCDebug(loggingCategory) << "Device unlocked, continuing into xochitl";
    return;
  }
  qCDebug(loggingCategory) << "Device unlocked, exiting xochitl";
  disablePoweroffScreen = true;
  pendingExit = true;
  // Normally this is not safe to do because of screen updates etc. It's safe in
  // this scenario because:
  // No-pin: display never updates
  // Pin: display stops updating after last key pressed
  _Exit(0);
}

static void
hook_PasscodeHandler(QObject* this_ptr) {
  if (!enabled || passcodeHandlerHooked) {
    return;
  }
  auto* receiver = LockscreenHookReceiver::singleton();
  if (receiver->passcodeHandler() != nullptr) {
    return;
  }
  const QMetaObject* metaObject = this_ptr->metaObject();
  if (metaObject == nullptr) {
    return;
  }
  const char* nameStr = metaObject->className();
  if (nameStr == nullptr) {
    return;
  }
  if (std::strcmp(nameStr, "xofm::libs::pincode::PasscodeHandler") != 0) {
    return;
  }
  if (!receiver->setPasscodeHandler(this_ptr)) {
    qCWarning(loggingCategory)
      << "Failed to connect PasscodeHandler::unlocked signal";
    return;
  }
  switch (receiver->waitForHook("hook")) {
    case HookState::Missing:
    case HookState::NotFile:
    case HookState::Success:
      break;
    case HookState::NoStart:
    case HookState::Crash:
    case HookState::Fail:
    default:
      qCWarning(loggingCategory) << "Disabling hook";
      enabled = false;
      return;
  }
  passcodeHandlerHooked = true;
  qCDebug(
    loggingCategory
  ) << "Hooked xofm::libs::pincode::PasscodeHandler::unlocked()";
  if (receiver->hasPasscode()) {
    QObject::connect(receiver, &LockscreenHookReceiver::unlocked, [] {
      handle_unlock();
    });
    return;
  }
  handle_unlock();
}

extern "C" {

__attribute__((visibility("default"))) void
_ZN7QObjectC2EPS_(QObject* this_ptr, QObject* parent) {
  static auto const constructor =
    reinterpret_cast<void (*)(QObject*, QObject*)>(
      dlsym(RTLD_NEXT, "_ZN7QObjectC2EPS_")
    );
  constructor(this_ptr, parent);
  if (!enabled || passcodeHandlerHooked || pendingExit) {
    return;
  }
  QPointer<QObject> safe(this_ptr);
  QMetaObject::invokeMethod(
    this_ptr,
    [safe]() {
      if (safe) {
        hook_PasscodeHandler(safe.data());
      }
    },
    Qt::QueuedConnection
  );
}

__attribute__((visibility("default"))) void
_ZN7QObjectC1EPS_(QObject* this_ptr, QObject* parent) {
  static auto const constructor =
    reinterpret_cast<void (*)(QObject*, QObject*)>(
      dlsym(RTLD_NEXT, "_ZN7QObjectC1EPS_")
    );
  constructor(this_ptr, parent);
  if (!enabled || passcodeHandlerHooked || pendingExit) {
    return;
  }
  QPointer<QObject> safe(this_ptr);
  QMetaObject::invokeMethod(
    this_ptr,
    [safe]() {
      if (safe) {
        hook_PasscodeHandler(safe.data());
      }
    },
    Qt::QueuedConnection
  );
}

__attribute__((visibility("default"))) void
_ZN6QImageC1ERK7QStringPKc(
  QImage* this_ptr,
  const QString& fileName,
  const char* format
) {
  static auto const constructor =
    reinterpret_cast<void (*)(QImage*, const QString&, const char*)>(
      dlsym(RTLD_NEXT, "_ZN6QImageC1ERK7QStringPKc")
    );
  if (
    !enabled || !disablePoweroffScreen ||
    !fileName.contains(QStringLiteral("poweroff.png"))
  ) {
    constructor(this_ptr, fileName, format);
    return;
  }
  qCDebug(loggingCategory) << "Intercepted poweroff.png constructor";
  new (this_ptr) QImage();
}

__attribute__((visibility("default"))) void
_ZN6QImageC2ERK7QStringPKc(
  QImage* this_ptr,
  const QString& fileName,
  const char* format
) {
  static auto const constructor =
    reinterpret_cast<void (*)(QImage*, const QString&, const char*)>(
      dlsym(RTLD_NEXT, "_ZN6QImageC2ERK7QStringPKc")
    );
  if (
    !enabled || !disablePoweroffScreen ||
    !fileName.contains(QStringLiteral("poweroff.png"))
  ) {
    constructor(this_ptr, fileName, format);
    return;
  }
  qCDebug(loggingCategory) << "Intercepted poweroff.png constructor";
  new (this_ptr) QImage();
}

void __attribute__((constructor))
init(void) {
#ifdef DEBUG
#if defined(__arm__) || defined(__aarch64__) || defined(__x86_64__)
  QLoggingCategory::setFilterRules("lockscreen_hook=true");
  struct sigaction action{};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = __signal_handler;
  sigaction(SIGSEGV, &action, nullptr);
  action.sa_sigaction = __signal_handler;
  sigaction(SIGABRT, &action, nullptr);
  action.sa_sigaction = __signal_handler;
  sigaction(SIGBUS, &action, nullptr);
#endif
#endif
  enabled = true;
}
}
