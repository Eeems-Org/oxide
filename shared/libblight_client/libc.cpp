#include "libc.h"
#include <dlfcn.h>

namespace Libc {
  ssize_t (*write)(int, const void*, size_t) = (ssize_t (*)(
    int,
    const void*,
    size_t
  ))dlvsym(RTLD_NEXT, "write", "GLIBC_2.4");

  ssize_t (*writev)(int, const iovec*, int) = (ssize_t (*)(
    int,
    const iovec*,
    int
  ))dlvsym(RTLD_NEXT, "writev", "GLIBC_2.4");

  ssize_t (*writev64)(int, const iovec*, int) = (ssize_t (*)(
    int,
    const iovec*,
    int
  ))dlvsym(RTLD_NEXT, "writev64", "GLIBC_2.4");

  ssize_t (*pwrite)(int, const void*, size_t, int) = (ssize_t (*)(
    int,
    const void*,
    size_t,
    int
  ))dlvsym(RTLD_NEXT, "pwrite", "GLIBC_2.4");

  ssize_t (*pwrite64)(int, const void*, size_t, int) = (ssize_t (*)(
    int,
    const void*,
    size_t,
    int
  ))dlvsym(RTLD_NEXT, "pwrite64", "GLIBC_2.4");

  ssize_t (*pwritev)(int, const iovec*, int, int) = (ssize_t (*)(
    int,
    const iovec*,
    int,
    int
  ))dlvsym(RTLD_NEXT, "pwritev", "GLIBC_2.4");

  ssize_t (*pwritev64)(int, const iovec*, int, int) = (ssize_t (*)(
    int,
    const iovec*,
    int,
    int
  ))dlvsym(RTLD_NEXT, "pwritev64", "GLIBC_2.4");

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

  int (*read)(int, void*, size_t) =
    (int (*)(int, void*, size_t))dlsym(RTLD_NEXT, "read");
}
