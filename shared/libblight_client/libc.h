#pragma once
#include <cstdint>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <unistd.h>

namespace Libc {
  extern ssize_t (*write)(int, const void*, size_t);
  extern ssize_t (*writev)(int, const iovec*, int);
  extern ssize_t (*writev64)(int, const iovec*, int);
  extern ssize_t (*pwrite)(int, const void*, size_t, int);
  extern ssize_t (*pwrite64)(int, const void*, size_t, int);
  extern ssize_t (*pwritev)(int, const iovec*, int, int);
  extern ssize_t (*pwritev64)(int, const iovec*, int, int);
  extern int (*open)(const char*, int, ...);
  extern int (*open64)(const char*, int, ...);
  extern int (*openat)(int, const char*, int, ...);
  extern int (*openat64)(int, const char*, int, ...);
  extern int (*ioctl)(int, unsigned long, ...);
  extern int (*close)(int);
  extern int (*fcntl)(int, int, ...);
  extern int (*msgget)(key_t, int);
  extern int (*msgsnd)(int, const void*, size_t, int);
  extern int (*flock)(int, int);
  extern bool (*setenv)(const char*, const char*, int);
  extern int (*read)(int, void*, size_t);
  extern int (*poll)(struct pollfd*, nfds_t, int);
  extern int (*ppoll)(
    struct pollfd*,
    nfds_t,
    const struct timespec*,
    const sigset_t*
  );
  extern int (*select)(int, fd_set*, fd_set*, fd_set*, struct timeval*);
  extern int (*epoll_ctl)(int, int, int, struct epoll_event*);
  extern int (*epoll_wait)(int, struct epoll_event*, int, int);
}
