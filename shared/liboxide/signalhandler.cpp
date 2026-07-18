#include "signalhandler.h"

#include <csignal>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <QCoreApplication>

#include "debug.h"

static bool initialized = false;
struct NotifierItem {
  int signal;
  QLocalSocket* notifier;
  int fd;
};
static NotifierItem notifiers[8] = {};

namespace Oxide {

  int SignalHandler::setup_unix_signal_handlers() {
    struct sigaction action;
    action.sa_sigaction = SignalHandler::handleSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_flags |= SA_RESTART | SA_SIGINFO;
#define _sigaction(signal)                                                     \
  if (sigaction(signal, &action, 0)) {                                         \
    return signal;                                                             \
  }
    _sigaction(SIGTERM);
    _sigaction(SIGINT);
    _sigaction(SIGUSR1);
    _sigaction(SIGUSR2);
    _sigaction(SIGCONT);
    _sigaction(SIGPIPE);
    _sigaction(SIGSEGV);
    _sigaction(SIGBUS);
#undef _sigaction
    initialized = true;
    return 0;
  }
  SignalHandler* SignalHandler::__singleton() {
    static SignalHandler* instance;
    if (instance == nullptr) {
      instance = new SignalHandler(qApp);
    }
    if (!initialized) {
      auto res = setup_unix_signal_handlers();
      if (res) {
        qFatal(QString("Failed to setup signal handlers: %1")
                 .arg(res)
                 .toStdString()
                 .c_str());
      }
    }
    return instance;
  }
  SignalHandler::SignalHandler(QObject* parent)
    : QObject(parent) {
    for (int idx = 0; idx < 8; idx++) {
      auto& entry = notifiers[idx];
      entry.signal = indexSignal(idx);
      entry.notifier = nullptr;
      entry.fd = -1;
    }
    addNotifier(SIGTERM, "sigTerm");
    addNotifier(SIGINT, "sigInt");
    addNotifier(SIGUSR1, "sigUsr1");
    addNotifier(SIGUSR2, "sigUsr2");
    addNotifier(SIGCONT, "sigCont");
    addNotifier(SIGPIPE, "sigPipe");
    addNotifier(SIGSEGV, "sigSegv");
    addNotifier(SIGBUS, "sigBus");
  }
  SignalHandler::~SignalHandler() {
    for (int idx = 0; idx < 8; idx++) {
      auto& entry = notifiers[idx];
      if (entry.notifier != nullptr) {
        delete entry.notifier;
        entry.notifier = nullptr;
      }
      if (entry.fd >= 0) {
        ::close(entry.fd);
        entry.fd = -1;
      }
    }
  }
  void SignalHandler::handleSignal(int signal, siginfo_t* si, void* vcontext) {
    Q_UNUSED(si);
#if defined(__arm__) || defined(__aarch64__) || defined(__x86_64__)
    if (signal == SIGPIPE || signal == SIGSEGV || signal == SIGBUS) {
      constexpr int depth = 10;
      auto uc = (ucontext_t*)vcontext;
      auto caller_address =
#if defined(__arm__)
        (void*)uc->uc_mcontext.arm_pc;
#elif defined(__aarch64__)
        (void*)uc->uc_mcontext.pc;
#elif defined(__x86_64__)
        (void*)uc->uc_mcontext.gregs[REG_RIP];
#endif
      void* array[depth];
      size_t size = ::backtrace(array, depth);
      const char* msg = strsignal(signal);
      ::write(STDERR_FILENO, msg, std::strlen(msg));
      ::write(STDERR_FILENO, "\n", std::strlen("\n"));
      if (size > 1) {
        array[1] = caller_address;
        ::backtrace_symbols_fd(array + 1, size - 1, STDERR_FILENO);
      } else {
        const char* msg2 = "Unable to get backtrace\n";
        ::write(STDERR_FILENO, msg2, std::strlen(msg2));
      }
    }
#endif
    if (!hasNotifier(signal)) {
      ::signal(signal, SIG_DFL);
      ::raise(signal);
      return;
    }
    static const char msg_prefix[] = "Signal received: ";
    ::write(STDERR_FILENO, msg_prefix, sizeof(msg_prefix) - 1);
    const char* name = strsignal(signal);
    if (name) {
      ::write(STDERR_FILENO, name, strlen(name));
    }
    ::write(STDERR_FILENO, "\n", 1);
    int idx = signalIndex(signal);
    if (idx < 0) {
      return;
    }
    auto item = notifiers[idx];
    char a = 1;
    ::write(item.fd, &a, sizeof(a));
  }
  void SignalHandler::addNotifier(int signal, const char* name) {
    int idx = signalIndex(signal);
    if (!hasNotifier(signal)) {
      int fds[2];
      if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) {
        qFatal("Couldn't create socketpair");
      }
      auto socket = new QLocalSocket();
      if (!socket->setSocketDescriptor(
            fds[1],
            QLocalSocket::ConnectedState,
            QLocalSocket::ReadOnly | QLocalSocket::Unbuffered
          )) {
        qFatal("Couldn't connect QLocalSocket to socket descriptor");
      }
      notifiers[idx].notifier = socket;
      notifiers[idx].fd = fds[0];
    }
    auto notifier = notifiers[idx].notifier;
    connect(
      notifier,
      &QLocalSocket::readyRead,
      this,
      [this, signal, notifier, name] {
        while (!notifier->atEnd()) {
          notifier->read(sizeof(char));
          if (!QMetaObject::invokeMethod(this, name, Qt::QueuedConnection)) {
            O_WARNING("Failed to emit" << name);
          }
          O_DEBUG("emitted" << name);
        }
      },
      Qt::QueuedConnection
    );
  }
  int SignalHandler::signalIndex(int signal) {
    switch (signal) {
      case SIGTERM:
        return 0;
      case SIGINT:
        return 1;
      case SIGUSR1:
        return 2;
      case SIGUSR2:
        return 3;
      case SIGCONT:
        return 4;
      case SIGPIPE:
        return 5;
      case SIGSEGV:
        return 6;
      case SIGBUS:
        return 7;
      default:
        return -1;
    }
  }
  int SignalHandler::indexSignal(int idx) {
    switch (idx) {
      case 0:
        return SIGTERM;
      case 1:
        return SIGINT;
      case 2:
        return SIGUSR1;
      case 3:
        return SIGUSR2;
      case 4:
        return SIGCONT;
      case 5:
        return SIGPIPE;
      case 6:
        return SIGSEGV;
      case 7:
        return SIGBUS;
      default:
        return -1;
    }
  }
  bool SignalHandler::hasNotifier(int signal) {
    int idx = signalIndex(signal);
    if (idx < 0 || idx > 7) {
      return false;
    }
    auto& entry = notifiers[idx];
    return entry.notifier != nullptr && entry.fd >= 0;
  }
  void SignalHandler::removeNotifier(int signal) {
    int idx = signalIndex(signal);
    if (idx < 0 || idx > 7) {
      return;
    }
    auto& entry = notifiers[idx];
    if (entry.notifier != nullptr) {
      entry.notifier->disconnect();
      delete entry.notifier;
      entry.notifier = nullptr;
    }
    if (entry.fd >= 0) {
      ::close(entry.fd);
      entry.fd = -1;
    }
    ::signal(signal, SIG_DFL);
  }
} // namespace Oxide

#include "moc_signalhandler.cpp"
