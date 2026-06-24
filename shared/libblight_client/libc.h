#pragma once
#include <cstdint>
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <unistd.h>

struct _IO_FILE;
typedef struct _IO_FILE FILE;

namespace Libc {
  extern sighandler_t (*signal)(int, sighandler_t);
  extern int (*sigaction)(int, const struct sigaction*, struct sigaction*);

  extern ssize_t (*write)(int, const void*, size_t);
  extern ssize_t (*writev)(int, const iovec*, int);
  extern ssize_t (*writev64)(int, const iovec*, int);
  extern ssize_t (*pwrite)(int, const void*, size_t, off_t);
  extern ssize_t (*pwrite64)(int, const void*, size_t, off64_t);
  extern ssize_t (*pwritev)(int, const iovec*, int, off_t);
  extern ssize_t (*pwritev64)(int, const iovec*, int, off64_t);
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
  extern bool (*unsetenv)(const char*);
  extern int (*read)(int, void*, size_t);
  extern int (*poll)(struct pollfd*, nfds_t, int);
  extern int (*ppoll)(
    struct pollfd*,
    nfds_t,
    const struct timespec*,
    const sigset_t*
  );
  extern int (*__ppoll64)(
    struct pollfd*,
    nfds_t,
    const struct timespec*,
    const sigset_t*
  );
  extern int (*select)(int, fd_set*, fd_set*, fd_set*, struct timeval*);
  extern int (*pselect)(
    int,
    fd_set*,
    fd_set*,
    fd_set*,
    const struct timespec*,
    const sigset_t*
  );
  extern int (*epollCtl)(int, int, int, struct epoll_event*);
  extern int (*epoll_wait)(int, struct epoll_event*, int, int);
  extern int (*system)(const char*);
  extern FILE* (*fopen)(const char* path, const char* mode);
  extern FILE* (*fopen64)(const char* path, const char* mode);
  extern int (*getdents)(unsigned int, struct dirent*, unsigned int);
  extern ssize_t (*getdents64)(int, void*, size_t);
  extern DIR* (*opendir)(const char*);
  extern struct dirent* (*readdir)(DIR*);
  extern struct dirent64* (*readdir64)(DIR*);
  extern int (*closedir)(DIR*);
}
