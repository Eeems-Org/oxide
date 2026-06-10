#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include "receiver.h"

#define DEBUG

#ifdef DEBUG
#include <execinfo.h>
#include <iostream>
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
hook_PasscodeHandler(QObject* obj) {
  if (passcodeHandlerHooked) {
    return;
  }
  auto* receiver = LockscreenHookReceiver::singleton();
  if (receiver->passcodeHandler() != nullptr) {
    return;
  }
  const QMetaObject* metaObject = obj->metaObject();
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
  if (!receiver->setPasscodeHandler(obj)) {
    qCWarning(loggingCategory)
      << "Failed to connect PasscodeHandler::unlocked signal";
    return;
  }
  passcodeHandlerHooked = true;
  qCDebug(
    loggingCategory
  ) << "Hooked xofm::libs::pincode::PasscodeHandler::unlocked()";
}

extern "C" {

__attribute__((visibility("default"))) void
_ZN7QObjectC2EPS_(QObject* obj, QObject* parent) {
  static auto const constructor =
    reinterpret_cast<void (*)(QObject*, QObject*)>(
      dlsym(RTLD_NEXT, "_ZN7QObjectC2EPS_")
    );
  constructor(obj, parent);
  if (passcodeHandlerHooked) {
    return;
  }
  QPointer<QObject> safe(obj);
  QMetaObject::invokeMethod(
    obj,
    [safe]() {
      if (safe) {
        hook_PasscodeHandler(safe.data());
      }
    },
    Qt::QueuedConnection
  );
}

__attribute__((visibility("default"))) void
_ZN7QObjectC1EPS_(QObject* obj, QObject* parent) {
  static auto const constructor =
    reinterpret_cast<void (*)(QObject*, QObject*)>(
      dlsym(RTLD_NEXT, "_ZN7QObjectC1EPS_")
    );
  constructor(obj, parent);
  if (passcodeHandlerHooked) {
    return;
  }
  QPointer<QObject> safe(obj);
  QMetaObject::invokeMethod(
    obj,
    [safe]() {
      if (safe) {
        hook_PasscodeHandler(safe.data());
      }
    },
    Qt::QueuedConnection
  );
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
