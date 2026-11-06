#include "libc.h"
#include <cstring>
#include <dlfcn.h>
#include <sys/syscall.h>

namespace Libc {

#define register_function(name, arguments, return_type)                        \
  return_type(*name) arguments =                                               \
    (return_type(*) arguments)dlsym(RTLD_NEXT, #name);

  register_function(write, (int, const void*, size_t), ssize_t);
  register_function(writev, (int, const iovec*, int), ssize_t);
  register_function(pwrite, (int, const void*, size_t, off_t), ssize_t);
  register_function(pwrite64, (int, const void*, size_t, off64_t), ssize_t);
  register_function(pwritev, (int, const iovec*, int, off_t), ssize_t);
  register_function(pwritev64, (int, const iovec*, int, off64_t), ssize_t);
  register_function(open, (const char*, int, ...), int);
  register_function(open64, (const char*, int, ...), int);
  register_function(openat, (int, const char*, int, ...), int);
  register_function(openat64, (int, const char*, int, ...), int);
  register_function(ioctl, (int, unsigned long, ...), int);
  register_function(close, (int), int);
  register_function(fcntl, (int, int, ...), int);
  register_function(fcntl64, (int, int, ...), int);
  register_function(msgget, (key_t, int), int);
  register_function(msgsnd, (int, const void*, size_t, int), int);
  register_function(flock, (int, int), int);
  register_function(setenv, (const char*, const char*, int), bool);
  register_function(unsetenv, (const char*), bool);
  register_function(read, (int, void*, size_t), int);
  register_function(poll, (struct pollfd*, nfds_t, int), int);
  register_function(
    ppoll,
    (struct pollfd*, nfds_t, const struct timespec*, const sigset_t*),
    int
  );
  register_function(
    __ppoll64,
    (struct pollfd*, nfds_t, const struct timespec*, const sigset_t*),
    int
  );
  register_function(
    select,
    (int, fd_set*, fd_set*, fd_set*, struct timeval*),
    int
  );
  register_function(
    pselect,
    (int, fd_set*, fd_set*, fd_set*, const struct timespec*, const sigset_t*),
    int
  );
  int (*epollCtl)(int, int, int, struct epoll_event*) =
    (int (*)(int, int, int, struct epoll_event*))dlsym(RTLD_NEXT, "epoll_ctl");
  register_function(epoll_wait, (int, struct epoll_event*, int, int), int);
  register_function(signal, (int, sighandler_t), sighandler_t);
  register_function(
    sigaction,
    (int, const struct sigaction*, struct sigaction*),
    int
  );
  register_function(system, (const char*), int);
  register_function(fopen, (const char* path, const char* mode), FILE*);
  register_function(fopen64, (const char* path, const char* mode), FILE*);
  register_function(
    getdents,
    (unsigned int, struct dirent*, unsigned int),
    int
  );
  register_function(getdents64, (int, void*, size_t), ssize_t);
  register_function(opendir, (const char*), DIR*);
  register_function(readdir, (DIR*), struct dirent*);
  register_function(readdir64, (DIR*), struct dirent64*);
  register_function(closedir, (DIR*), int);

#undef register_function

  bool init() {
    static bool initialized = false;
    if (initialized) {
      return true;
    }

    bool success = true;
#define hook(name, symbol)                                                     \
  if (Libc::name == nullptr) {                                                 \
    Libc::name =                                                               \
      reinterpret_cast<decltype(Libc::name)>(dlsym(RTLD_NEXT, #symbol));       \
  }

#define hook_ensure(name, symbol)                                              \
  hook(name, symbol);                                                          \
  if (Libc::name == nullptr) {                                                 \
    static const char message[] = "Failed to resolve " #symbol "\n";           \
    syscall(SYS_write, 2, message, sizeof(message) - 1);                       \
    success = false;                                                           \
  }

    hook_ensure(write, write);
    hook_ensure(writev, writev);
    hook_ensure(pwrite, pwrite);
    hook_ensure(pwrite64, pwrite64);
    hook_ensure(pwritev, pwritev);
    hook_ensure(pwritev64, pwritev64);
    hook_ensure(open, open);
    hook_ensure(open64, open64);
    hook_ensure(openat, openat);
    hook_ensure(openat64, openat64);
    hook_ensure(ioctl, ioctl);
    hook_ensure(close, close);
    hook_ensure(fcntl, fcntl);
    hook_ensure(fcntl64, fcntl64);
    hook_ensure(msgget, msgget);
    hook_ensure(msgsnd, msgsnd);
    hook_ensure(flock, flock);
    hook_ensure(setenv, setenv);
    hook_ensure(unsetenv, unsetenv);
    hook_ensure(read, read);
    hook_ensure(poll, poll);
    hook_ensure(ppoll, ppoll);
#ifdef __aarch64__
    hook(__ppoll64, __ppoll64);
#else
    hook_ensure(__ppoll64, __ppoll64);
#endif
    hook_ensure(select, select);
    hook_ensure(pselect, pselect);
    hook_ensure(epollCtl, epoll_ctl);
    hook_ensure(epoll_wait, epoll_wait);
    hook_ensure(signal, signal);
    hook_ensure(sigaction, sigaction);
    hook_ensure(system, system);
    hook_ensure(fopen, fopen);
    hook_ensure(fopen64, fopen64);
    hook(getdents, getdents);
    hook_ensure(getdents64, getdents64);
    hook_ensure(opendir, opendir);
    hook_ensure(readdir, readdir);
    hook_ensure(readdir64, readdir64);
    hook_ensure(closedir, closedir);

#undef hook
#undef hook_ensure

    initialized = true;
    return success;
  }
}
