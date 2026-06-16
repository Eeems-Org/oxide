#include "libc.h"
#include <dlfcn.h>

namespace Libc {
  ssize_t (*write)(int, const void*, size_t) =
    (ssize_t (*)(int, const void*, size_t))dlsym(RTLD_NEXT, "write");

  ssize_t (*writev)(int, const iovec*, int) =
    (ssize_t (*)(int, const iovec*, int))dlsym(RTLD_NEXT, "writev");

  ssize_t (*writev64)(int, const iovec*, int) =
    (ssize_t (*)(int, const iovec*, int))dlsym(RTLD_NEXT, "writev64");

  ssize_t (*pwrite)(int, const void*, size_t, off_t) =
    (ssize_t (*)(int, const void*, size_t, off_t))dlsym(RTLD_NEXT, "pwrite");

  ssize_t (*pwrite64)(int, const void*, size_t, off64_t) =
    (ssize_t (*)(int, const void*, size_t, off64_t))dlsym(
      RTLD_NEXT,
      "pwrite64"
    );

  ssize_t (*pwritev)(int, const iovec*, int, off_t) =
    (ssize_t (*)(int, const iovec*, int, off_t))dlsym(RTLD_NEXT, "pwritev");

  ssize_t (*pwritev64)(int, const iovec*, int, off64_t) =
    (ssize_t (*)(int, const iovec*, int, off64_t))dlsym(RTLD_NEXT, "pwritev64");

  int (*open)(const char*, int, ...) =
    (int (*)(const char*, int, ...))dlsym(RTLD_NEXT, "open");

  int (*open64)(const char*, int, ...) =
    (int (*)(const char*, int, ...))dlsym(RTLD_NEXT, "open64");

  int (*openat)(int, const char*, int, ...) =
    (int (*)(int, const char*, int, ...))dlsym(RTLD_NEXT, "openat");

  int (*openat64)(int, const char*, int, ...) =
    (int (*)(int, const char*, int, ...))dlsym(RTLD_NEXT, "openat64");

  int (*ioctl)(int, unsigned long, ...) =
    (int (*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");

  int (*close)(int) = (int (*)(int))dlsym(RTLD_NEXT, "close");

  int (*fcntl)(int, int, ...) =
    (int (*)(int, int, ...))dlsym(RTLD_NEXT, "fcntl");

  int (*msgget)(key_t, int) = (int (*)(key_t, int))dlsym(RTLD_NEXT, "msgget");

  int (*msgsnd)(int, const void*, size_t, int) =
    (int (*)(int, const void*, size_t, int))dlsym(RTLD_NEXT, "msgsnd");

  int (*flock)(int, int) = (int (*)(int, int))dlsym(RTLD_NEXT, "flock");

  bool (*setenv)(const char*, const char*, int) =
    (bool (*)(const char*, const char*, int))dlsym(RTLD_NEXT, "setenv");

  bool (*unsetenv)(const char*) =
    (bool (*)(const char*))dlsym(RTLD_NEXT, "unsetenv");

  int (*read)(int, void*, size_t) =
    (int (*)(int, void*, size_t))dlsym(RTLD_NEXT, "read");

  int (*poll)(struct pollfd*, nfds_t, int) =
    (int (*)(struct pollfd*, nfds_t, int))dlsym(RTLD_NEXT, "poll");

  int (*ppoll)(
    struct pollfd*,
    nfds_t,
    const struct timespec*,
    const sigset_t*
  ) =
    (int (*)(
      struct pollfd*,
      nfds_t,
      const struct timespec*,
      const sigset_t*
    ))dlsym(RTLD_NEXT, "ppoll");

  int (*__ppoll64)(
    struct pollfd*,
    nfds_t,
    const struct timespec*,
    const sigset_t*
  ) =
    (int (*)(
      struct pollfd*,
      nfds_t,
      const struct timespec*,
      const sigset_t*
    ))dlsym(RTLD_NEXT, "__ppoll64");

  int (*select)(int, fd_set*, fd_set*, fd_set*, struct timeval*) =
    (int (*)(int, fd_set*, fd_set*, fd_set*, struct timeval*))dlsym(
      RTLD_NEXT,
      "select"
    );

  int (*pselect)(
    int,
    fd_set*,
    fd_set*,
    fd_set*,
    const struct timespec*,
    const sigset_t*
  ) =
    (int (*)(
      int,
      fd_set*,
      fd_set*,
      fd_set*,
      const struct timespec*,
      const sigset_t*
    ))dlsym(RTLD_NEXT, "pselect");

  int (*epoll_ctl)(int, int, int, struct epoll_event*) =
    (int (*)(int, int, int, struct epoll_event*))dlsym(RTLD_NEXT, "epoll_ctl");

  int (*epoll_wait)(int, struct epoll_event*, int, int) =
    (int (*)(int, struct epoll_event*, int, int))dlsym(RTLD_NEXT, "epoll_wait");

  sighandler_t (*signal)(int, sighandler_t) =
    (sighandler_t (*)(int, sighandler_t))dlsym(RTLD_NEXT, "signal");

  int (*sigaction)(int, const struct sigaction*, struct sigaction*) = (int (*)(
    int,
    const struct sigaction*,
    struct sigaction*
  ))dlsym(RTLD_NEXT, "sigaction");

  int (*system)(const char*) = (int (*)(const char*))dlsym(RTLD_NEXT, "system");

  FILE* (*fopen)(const char* path, const char* mode) =
    (FILE * (*)(const char* path, const char* mode)) dlsym(RTLD_NEXT, "fopen");

  FILE* (*fopen64)(const char* path, const char* mode) =
    (FILE *
     (*)(const char* path, const char* mode)) dlsym(RTLD_NEXT, "fopen64");
}
