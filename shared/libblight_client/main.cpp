#ifdef __aarch64__
#include "drm.h"
#endif
#include "fb.h"
#include "input.h"
#include "libc.h"
#include "state.h"

#include <asm/ioctl.h>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <filesystem>
#include <libblight.h>
#include <libblight/clock.h>
#include <libblight/connection.h>
#include <libblight/debug.h>
#include <libblight/socket.h>
#include <libblight/system.h>
#include <linux/fb.h>
#include <linux/fcntl.h>
#include <linux/input.h>
#include <mxcfb.h>
#include <stdio.h>
#include <string>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <systemd/sd-journal.h>
#include <unistd.h>

#ifdef DEBUG
#include <execinfo.h>
#include <iostream>
#include <ucontext.h>
#endif

namespace {
#ifdef DEBUG
  void __sigsegv_handler(int nSignum, siginfo_t* si, void* vcontext) {
    (void)nSignum;
    (void)si;
    constexpr int depth = 10;
    auto uc = (ucontext_t*)vcontext;
    auto caller_address =
#if defined(__arm__)
      (void*)uc->uc_mcontext.arm_pc;
#elif defined(__aarch64__)
      (void*)uc->uc_mcontext.pc;
#else
#error "Unsupported architecture"
#endif
    void* array[depth];
    size_t size = ::backtrace(array, depth);
    array[1] = caller_address;
    char** messages = ::backtrace_symbols(array, size);
    std::cerr << "Segmentation fault:" << std::endl;
    for (size_t i = 1; i < size && messages != NULL; ++i) {
      std::cerr << "  " << messages[i] << std::endl;
    }
    std::_Exit(139);
  }

  void __sigabrt_handler(int nSignum, siginfo_t* si, void* vcontext) {
    (void)nSignum;
    (void)si;
    constexpr int depth = 10;
    auto uc = (ucontext_t*)vcontext;
    auto caller_address =
#if defined(__arm__)
      (void*)uc->uc_mcontext.arm_pc;
#elif defined(__aarch64__)
      (void*)uc->uc_mcontext.pc;
#else
#error "Unsupported architecture"
#endif
    void* array[depth];
    size_t size = ::backtrace(array, depth);
    array[1] = caller_address;
    const char* msg = "Abort (heap corruption suspected):\n";
    Libc::write(STDERR_FILENO, msg, std::strlen(msg));
    ::backtrace_symbols_fd(array + 1, size - 1, STDERR_FILENO);
    std::_Exit(134);
  }
#endif

  int __open(const char* pathname, int flags) {
    if (!Client::INITIALIZED) {
      return -2;
    }
    std::string actualpath(pathname);
    if (std::filesystem::exists(actualpath)) {
      Client::realpath(pathname, actualpath);
    }
    int res = -2;
    if (Client::isFakeRM1() && actualpath == "/sys/devices/soc0/machine") {
      int fd = memfd_create("machine", MFD_ALLOW_SEALING);
      std::string data("reMarkable 1.0");
      // Don't include trailing null
      Libc::write(fd, data.data(), data.size());
      Libc::fcntl(
        fd,
        F_ADD_SEALS,
        F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE |
          F_SEAL_FUTURE_WRITE
      );
      lseek(fd, 0, SEEK_SET);
      return fd;
    } else if (actualpath == "/tmp/epframebuffer.lock") {
      if (FB::epframebufferLockFd > 0) {
        return FB::epframebufferLockFd;
      }
      res = FB::epframebufferLockFd = Libc::open(
        ("/tmp/epframebuffer." + std::to_string(getpid()) + ".lock").c_str(),
        flags
      );
    } else if (actualpath == "/tmp/epd.lock") {
      if (FB::epdLockFd > 0) {
        return FB::epdLockFd;
      }
      res = FB::epdLockFd = Libc::open(
        ("/tmp/epd." + std::to_string(getpid()) + ".lock").c_str(), flags
      );
    } else if (actualpath == "/dev/fb0" || actualpath == "/dev/shm/swtfb.01") {
      return FB::createBuffer();
#ifdef __aarch64__
    } else if (actualpath == "/dev/dri/card0") {
      FB::createBuffer();
      if (DRM::card0 <= 0) {
        DRM::card0 = Libc::open(actualpath.c_str(), flags);
      }
      return DRM::card0;
#endif
    } else if (
      Client::isInputEnabled() && actualpath.starts_with("/dev/input/event")
    ) {
      _INFO("Opening event device: %s", actualpath.c_str());
      return Input::open(actualpath, flags);
    }
    if (res == -1) {
      errno = EIO;
    }
    return res;
  }
}

extern "C" {
__attribute__((visibility("default"))) int
msgget(key_t key, int msgflg) {
  if (!Client::INITIALIZED || !Client::isFbEnabled()) {
    return Libc::msgget(key, msgflg);
  }
  // Catch rm2fb ipc
  if (key == 0x2257c) {
    // inject our own ipc here
    if (FB::msgq == -1) {
      FB::msgq = Libc::msgget(0x2257d, msgflg);
    }
    return FB::msgq;
  }
  return Libc::msgget(key, msgflg);
}

__attribute__((visibility("default"))) int
msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg) {
  if (!Client::INITIALIZED || !Client::isFbEnabled()) {
    return Libc::msgsnd(msqid, msgp, msgsz, msgflg);
  }
  if (msqid == FB::msgq) {
    FB::ensure_surface();
    _DEBUG("%s", "rm2fb ipc repaint");
    auto buf = static_cast<const swtfb::swtfb_update*>(msgp);
    auto region = buf->mdata.update.update_region;
    FB::repaint(
      region.left,
      region.top,
      region.width,
      region.height,
      Blight::WaveformMode::HighQualityGrayscale,
      Blight::UpdateMode::PartialUpdate,
      0
    );
    return 0;
  }
  return Libc::msgsnd(msqid, msgp, msgsz, msgflg);
}

__attribute__((visibility("default"))) int
open64(const char* pathname, int flags, ...) {
  if (!Client::INITIALIZED) {
    va_list args;
    va_start(args, flags);
    int fd = Libc::open64(pathname, flags, args);
    va_end(args);
    return fd;
  }
  _DEBUG("open64 %s %d", pathname, flags);
  int fd = __open(pathname, flags);
  if (fd == -2) {
    va_list args;
    va_start(args, flags);
    fd = Libc::open64(pathname, flags, args);
    va_end(args);
  }
  _DEBUG("opened %s with fd %d", pathname, fd);
  return fd;
}

__attribute__((visibility("default"))) int
openat(int dirfd, const char* pathname, int flags, ...) {
  if (!Client::INITIALIZED) {
    va_list args;
    va_start(args, flags);
    int fd = Libc::openat(dirfd, pathname, flags, args);
    va_end(args);
    return fd;
  }
  _DEBUG("openat %s %d", pathname, flags);
  int fd = __open(pathname, flags);
  if (fd == -2) {
    DIR* save = opendir(".");
    fchdir(dirfd);
    char path[PATH_MAX + 1];
    getcwd(path, PATH_MAX);
    fchdir(::dirfd(save));
    closedir(save);
    std::string fullpath(path);
    fullpath += "/";
    fullpath += pathname;
    fd = __open(fullpath.c_str(), flags);
  }
  if (fd == -2) {
    va_list args;
    va_start(args, flags);
    fd = Libc::openat(dirfd, pathname, flags, args);
    va_end(args);
  }
  _DEBUG("opened %s with fd %d", pathname, fd);
  return fd;
}

__attribute__((visibility("default"))) int
openat64(int dirfd, const char* pathname, int flags, ...) {
  if (!Client::INITIALIZED) {
    va_list args;
    va_start(args, flags);
    int fd = Libc::openat(dirfd, pathname, flags, args);
    va_end(args);
    return fd;
  }
  _DEBUG("openat64 %s %d", pathname, flags);
  int fd = __open(pathname, flags);
  if (fd == -2) {
    DIR* save = opendir(".");
    fchdir(dirfd);
    char path[PATH_MAX + 1];
    getcwd(path, PATH_MAX);
    fchdir(::dirfd(save));
    closedir(save);
    std::string fullpath(path);
    fullpath += "/";
    fullpath += pathname;
    fd = __open(fullpath.c_str(), flags);
  }
  if (fd == -2) {
    va_list args;
    va_start(args, flags);
    fd = Libc::openat64(dirfd, pathname, flags, args);
    va_end(args);
  }
  _DEBUG("opened %s with fd %d", pathname, fd);
  return fd;
}

__attribute__((visibility("default"))) int
open(const char* pathname, int flags, ...) {
  if (!Client::INITIALIZED) {
    va_list args;
    va_start(args, flags);
    int fd = Libc::open(pathname, flags, args);
    ;
    va_end(args);
    return fd;
  }
  _DEBUG("open %s %d", pathname, flags);
  int fd = __open(pathname, flags);
  if (fd == -2) {
    va_list args;
    va_start(args, flags);
    fd = Libc::open(pathname, flags, args);
    va_end(args);
  }
  _DEBUG("opened %s with fd %d", pathname, fd);
  return fd;
}

__attribute__((visibility("default"))) int
close(int fd) {
  if (Client::INITIALIZED) {
    _DEBUG("close %d", fd);
    if (FB::is_fb(fd)) {
      // Maybe actually close it?
      return 0;
    }
    if (Client::isInputEnabled() && Input::isInputFd(fd)) {
      return Input::close(fd);
    }
    if (fd == FB::epframebufferLockFd) {
      int res = Libc::close(FB::epframebufferLockFd);
      if (res == 0) {
        FB::epframebufferLockFd = -1;
        unlink(
          ("/tmp/epframebuffer." + std::to_string(getpid()) + ".lock").c_str()
        );
      }
      return res;
    }
    if (fd == FB::epdLockFd) {
      int res = Libc::close(FB::epdLockFd);
      if (res == 0) {
        FB::epdLockFd = -1;
        unlink(("/tmp/epd." + std::to_string(getpid()) + ".lock").c_str());
      }
      return res;
    }
  }
  return Libc::close(fd);
}

__attribute__((visibility("default"))) int
fcntl(int fd, int cmd, ...) {
  va_list args;
  va_start(args, cmd);
  void* ptr = va_arg(args, void*);
  _DEBUG("fcntl %d %d %p", fd, cmd, ptr);
  int res;
  if (Client::INITIALIZED && Client::isInputEnabled() && Input::isInputFd(fd)) {
    res = Input::fcntl(fd, cmd, ptr);
  } else {
    res = Libc::fcntl(fd, cmd, ptr);
  }
  va_end(args);
  return res;
}

__attribute__((visibility("default"))) ssize_t
read(int fd, void* buf, size_t count) {
  if (Client::INITIALIZED && Client::isInputEnabled() && Input::isInputFd(fd)) {
    return Input::read(fd, buf, count);
  }
  return Libc::read(fd, buf, count);
}

__attribute__((visibility("default"))) int
ioctl(int fd, unsigned long request, ...) {
  va_list args;
  va_start(args, request);
  char* ptr = va_arg(args, char*);
  if (Client::INITIALIZED) {
    if (FB::is_fb(fd)) {
      int res = FB::ioctl(request, ptr);
      va_end(args);
      return res;
    }
#ifdef __aarch64__
    if (DRM::is_drm(fd)) {
      int res = DRM::ioctl(request, ptr);
      va_end(args);
      return res;
    }
#endif
    if (!Client::isFbEnabled() && fd == FB::frameBuffer) {
      int res = FB::exclusive_ioctl(request, ptr);
      va_end(args);
      return res;
    }
    if (Client::isInputEnabled() && Input::isInputFd(fd)) {
      int res = Input::ioctlv(fd, request, ptr);
      va_end(args);
      return res;
    }
  }
  _DEBUG("unhandled ioctl %d %lu", fd, request);
  int res = Libc::ioctl(fd, request, ptr);
  va_end(args);
  return res;
}
__attribute__((visibility("default"))) int
setenv(const char* name, const char* value, int overwrite) {
  if (
    getenv("OXIDE_PRELOAD_FORCE_QT") != nullptr &&
    (strcmp(name, "QMLSCENE_DEVICE") == 0 ||
     strcmp(name, "QT_QUICK_BACKEND") == 0 ||
     strcmp(name, "QT_QPA_PLATFORM") == 0 ||
     strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0 ||
     strcmp(name, "QT_QPA_GENERIC_PLUGINS") == 0 ||
     strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0 ||
     strcmp(name, "QT_QPA_EVDEV_MOUSE_PARAMETERS") == 0 ||
     strcmp(name, "QT_QPA_EVDEV_KEYBOARD_PARAMETERS") == 0 ||
     strcmp(name, "QT_PLUGIN_PATH") == 0)
  ) {
    _DEBUG("IGNORED setenv %s=%s", name, value);
    return 0;
  }
  if (strcmp(name, "OXIDE_PRELOAD_DEBUG") == 0) {
    try {
      set_blight_debug_level(std::stoi(value));
    } catch (std::invalid_argument&) {
    } catch (std::out_of_range&) {
      _WARN("OXIDE_PRELOAD_DEBUG invalid value");
      return 1;
    }
  }
  _DEBUG("setenv %s=%s", name, value);
  return Libc::setenv(name, value, overwrite);
}

__attribute__((visibility("default"))) int
unsetenv(const char* name) {
  if (
    strcmp(name, "OXIDE_PRELOAD_DEBUG") == 0 ||
    (getenv("OXIDE_PRELOAD_FORCE_QT") != nullptr &&
     (strcmp(name, "QMLSCENE_DEVICE") == 0 ||
      strcmp(name, "QT_QUICK_BACKEND") == 0 ||
      strcmp(name, "QT_QPA_PLATFORM") == 0 ||
      strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0 ||
      strcmp(name, "QT_QPA_GENERIC_PLUGINS") == 0 ||
      strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0 ||
      strcmp(name, "QT_QPA_EVDEV_MOUSE_PARAMETERS") == 0 ||
      strcmp(name, "QT_QPA_EVDEV_KEYBOARD_PARAMETERS") == 0 ||
      strcmp(name, "QT_PLUGIN_PATH") == 0))
  ) {
    _DEBUG("IGNORED unsetenv %s", name);
    return 0;
  }
  _DEBUG("unsetenv %s", name);
  return Libc::unsetenv(name);
}

__attribute__((visibility("default"))) int
flock(int fd, int op) {
  if (!Client::INITIALIZED) {
    return Libc::flock(fd, op);
  }
  _DEBUG("flock %d %d", fd, op);
  if (fd == FB::epframebufferLockFd) {
    switch (op) {
      case LOCK_SH:
      case LOCK_EX:
      case LOCK_UN:
        return 0;
      default:
        break;
    }
  }
  return Libc::flock(fd, op);
}

__attribute__((visibility("default"))) int
poll(struct pollfd* fds, nfds_t nfds, int timeout) {
  struct pollfd* backup = nullptr;
  if (Client::INITIALIZED && Client::isInputEnabled()) {
    backup = Input::translatePollfds(fds, nfds);
  }
  int res = Libc::poll(fds, nfds, timeout);
  if (res == 0 && nfds > 0 && timeout < 0) {
    _WARN("poll exited early with no revents");
  }
  if (backup != nullptr) {
    Input::restorePollfds(fds, nfds, backup);
  }
  return res;
}

__attribute__((visibility("default"))) int
__ppoll64(
  struct pollfd* fds,
  nfds_t nfds,
  const struct timespec* tmo,
  const sigset_t* sigmask
) {
  struct pollfd* backup = nullptr;
  if (Client::INITIALIZED && Client::isInputEnabled()) {
    backup = Input::translatePollfds(fds, nfds);
  }
  int res = Libc::__ppoll64(fds, nfds, tmo, sigmask);
  if (res == 0 && nfds > 0 && tmo == nullptr) {
    _WARN("__ppoll64 exited early with no revents");
  }
  if (backup != nullptr) {
    Input::restorePollfds(fds, nfds, backup);
  }
  return res;
}

__attribute__((visibility("default"))) int
select(
  int nfds,
  fd_set* readfds,
  fd_set* writefds,
  fd_set* exceptfds,
  struct timeval* timeout
) {
  int backup;
  if (Client::INITIALIZED && Client::isInputEnabled()) {
    backup = Input::translateSelectFds(nfds, readfds, writefds, exceptfds);
  }
  int res = Libc::select(nfds, readfds, writefds, exceptfds, timeout);
  if (Client::INITIALIZED && Client::isInputEnabled()) {
    Input::restoreSelectFds(nfds, readfds, writefds, exceptfds, backup);
  }
  return res;
}

__attribute__((visibility("default"))) int
pselect(
  int nfds,
  fd_set* readfds,
  fd_set* writefds,
  fd_set* exceptfds,
  const struct timespec* ntimeout,
  const sigset_t* sigmask
) {
  int backup;
  if (Client::INITIALIZED && Client::isInputEnabled()) {
    backup = Input::translateSelectFds(nfds, readfds, writefds, exceptfds);
  }
  int res =
    Libc::pselect(nfds, readfds, writefds, exceptfds, ntimeout, sigmask);
  if (Client::INITIALIZED && Client::isInputEnabled()) {
    Input::restoreSelectFds(nfds, readfds, writefds, exceptfds, backup);
  }
  return res;
}

__attribute__((visibility("default"))) int
epoll_ctl(int epfd, int op, int fd, struct epoll_event* ev) {
  if (Client::INITIALIZED && Client::isInputEnabled() && Input::isInputFd(fd)) {
    return Input::epoll_ctl(epfd, op, fd, ev);
  }
  return Libc::epoll_ctl(epfd, op, fd, ev);
}

__attribute__((visibility("default"))) int
epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
  int res = Libc::epoll_wait(epfd, events, maxevents, timeout);
  if (res <= 0) {
    return res;
  }
  if (Client::INITIALIZED && Client::isInputEnabled()) {
    return Input::restoreEpollfds(epfd, events, res);
  }
  return res;
}

void __attribute__((constructor))
init(void) {
#ifdef DEBUG
  struct sigaction action{0};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = __sigsegv_handler;
  sigaction(SIGSEGV, &action, NULL);
  action.sa_sigaction = __sigabrt_handler;
  sigaction(SIGABRT, &action, NULL);
#endif
  if (Client::init() && FB::init()) {
    Client::FAILED_INIT = false;
    Client::INITIALIZED = true;
    FB::connection->focused();
    _DEBUG("blight_client initialized")
  }
}

__attribute__((visibility("default"))) int
__libc_start_main(
  int (*_main)(int, char**, char**),
  int argc,
  char** argv,
  int (*init)(int, char**, char**),
  void (*fini)(void),
  void (*rtld_fini)(void),
  void* stack_end
) {
  if (Client::FAILED_INIT) {
    return EXIT_FAILURE;
  }
  auto func_main =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");
  _DEBUG("Starting main(%d, ...)", argc);
  auto res = func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
  _DEBUG("Main exit code: %d", res);
  if (!Client::isFbEnabled()) {
    Blight::exitExclusiveMode();
  }
  return res;
}
}
