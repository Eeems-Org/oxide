#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstring>
#include <dlfcn.h>
#include <thread>
#include <unistd.h>

#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include "receiver.h"

#define DEBUG

#ifdef DEBUG
#include <execinfo.h>
#include <signal.h>
#include <ucontext.h>
#endif

#ifdef DEBUG
void
__sigsegv_handler(int nSignum, siginfo_t* si, void* vcontext) {
  Q_UNUSED(nSignum);
  Q_UNUSED(si);
  constexpr int depth = 10;
  auto uc = static_cast<ucontext_t*>(vcontext);
  void* caller_address =
#if defined(__arm__)
    reinterpret_cast<void*>(uc->uc_mcontext.arm_pc);
#elif defined(__aarch64__)
    reinterpret_cast<void*>(uc->uc_mcontext.pc);
#endif
  void* array[depth];
  size_t size = backtrace(array, depth);
  array[1] = caller_address;
  char** messages = backtrace_symbols(array, size);
  if (messages != nullptr) {
    write(STDERR_FILENO, "Segmentation fault:\n", 20);
    for (size_t i = 1; i < size && messages[i] != nullptr; ++i) {
      write(STDERR_FILENO, "  ", 2);
      size_t len = 0;
      while (messages[i][len])
        ++len;
      write(STDERR_FILENO, messages[i], len);
      write(STDERR_FILENO, "\n", 1);
    }
    free(messages);
  }
  _Exit(139);
}

void
__sigabrt_handler(int nSignum, siginfo_t* si, void* vcontext) {
  Q_UNUSED(nSignum);
  Q_UNUSED(si);
  constexpr int depth = 10;
  auto uc = static_cast<ucontext_t*>(vcontext);
  void* caller_address =
#if defined(__arm__)
    reinterpret_cast<void*>(uc->uc_mcontext.arm_pc);
#elif defined(__aarch64__)
    reinterpret_cast<void*>(uc->uc_mcontext.pc);
#endif
  void* array[depth];
  size_t size = backtrace(array, depth);
  array[1] = caller_address;
  const char* msg = "Abort (heap corruption suspected):\n";
  write(STDERR_FILENO, msg, 31);
  backtrace_symbols_fd(array + 1, size - 1, STDERR_FILENO);
  _Exit(134);
}

#endif

static void
hook_PasscodeHandler(QObject* this_ptr) {
  if (passcodeHandlerHooked) {
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
  passcodeHandlerHooked = true;
  qCDebug(
    loggingCategory
  ) << "Hooked xofm::libs::pincode::PasscodeHandler::unlocked()";
  std::thread([receiver]() {
    receiver->waitForUnlock();
    qCDebug(loggingCategory) << "Unlock complete, exiting xochitl";
    if (auto* app = QCoreApplication::instance()) {
      app->exit(EXIT_SUCCESS);
    }
  }).detach();
}

extern "C" {

__attribute__((visibility("default"))) void
_ZN7QObjectC2EPS_(QObject* this_ptr, QObject* parent) {
  static auto const constructor =
    reinterpret_cast<void (*)(QObject*, QObject*)>(
      dlsym(RTLD_NEXT, "_ZN7QObjectC2EPS_")
    );
  constructor(this_ptr, parent);
  if (passcodeHandlerHooked) {
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
  if (passcodeHandlerHooked) {
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
  if (!fileName.contains(QStringLiteral("poweroff.png"))) {
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
  if (!fileName.contains(QStringLiteral("poweroff.png"))) {
    constructor(this_ptr, fileName, format);
    return;
  }
  qCDebug(loggingCategory) << "Intercepted poweroff.png constructor";
  new (this_ptr) QImage();
}

void __attribute__((constructor))
init(void) {
#ifdef DEBUG
  QLoggingCategory::setFilterRules("lockscreen_hook=true");
  struct sigaction action{};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = __sigsegv_handler;
  sigaction(SIGSEGV, &action, nullptr);
  action.sa_sigaction = __sigabrt_handler;
  sigaction(SIGABRT, &action, nullptr);
#endif
}
}
