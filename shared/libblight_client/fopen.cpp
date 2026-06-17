#include "fopen.h"

#include <errno.h>

#ifndef __arm__
#include "libc.h"
#include "state.h"
#include <fcntl.h>
#include <libblight/debug.h>
#else
#define O_ACCMODE 3
#define O_RDONLY 0
#define O_WRONLY (1 << 0)
#define O_RDWR (1 << 1)
/* (1 << 2) must not be used -- it collides with flags on alpha, sparc */
/* (1 << 3) must not be used -- it collides with flags on alpha, mips, parisc,
 * sparc */
/* (1 << 4) must not be used -- it collides with flags on mips */
/* (1 << 5) is free */
#ifndef O_CREAT
#define O_CREAT (1 << 6) /* not fcntl */
#endif
#ifndef O_EXCL
#define O_EXCL (1 << 7) /* not fcntl */
#endif
#ifndef O_TRUNC
#define O_TRUNC (1 << 9) /* not fcntl */
#endif
#ifndef O_APPEND
#define O_APPEND (1 << 10)
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC (1 << 19) /* set close_on_exec */
#endif
extern "C" int
open(const char* pathname, int flags, ...);
namespace Libc {
  extern FILE* (*fopen)(const char* path, const char* mode);
  extern FILE* (*fopen64)(const char* path, const char* mode);
}
namespace Client {
  extern bool INITIALIZED;
}
#endif

extern "C" FILE*
fdopen(int fd, const char* mode) noexcept;

int
modeToFlags(const char* mode) {
  if (!mode || !*mode) {
    return -1;
  }
  int flags = 0;
  switch (*mode) {
    case 'r':
      flags = O_RDONLY;
      break;
    case 'w':
      flags = O_WRONLY | O_CREAT | O_TRUNC;
      break;
    case 'a':
      flags = O_WRONLY | O_CREAT | O_APPEND;
      break;
    default:
      return -1;
  }

  for (const char* c = mode + 1; *c; c++) {
    switch (*c) {
      case '+':
        flags = (flags & ~O_ACCMODE) | O_RDWR;
        break;
      case 'b':
        break;
      case 'x':
        flags |= O_EXCL;
        break;
      case 'e':
        flags |= O_CLOEXEC;
        break;
      default:
        break;
    }
  }
  return flags;
}

extern "C" {

FILE*
fopen(const char* path, const char* mode) {
  if (!Client::INITIALIZED) {
    return Libc::fopen(path, mode);
  }
#ifndef __arm__
  _DEBUG("fopen %s %s", path, mode);
#endif
  int flags = modeToFlags(mode);
  if (flags == -1) {
    errno = EINVAL;
    return nullptr;
  }
  int fd = open(path, flags);
  if (fd < 0) {
    return nullptr;
  }
#ifndef __arm__
  _DEBUG("opened %s with fd %d", path, fd);
#endif
  return fdopen(fd, mode);
}

FILE*
fopen64(const char* path, const char* mode) {
  if (!Client::INITIALIZED) {
    return Libc::fopen64(path, mode);
  }
#ifndef __arm__
  _DEBUG("fopen64 %s %s", path, mode);
#endif
  int flags = modeToFlags(mode);
  if (flags == -1) {
#ifndef __arm__
    _DEBUG("%s", strerror(errno));
#endif
    errno = EINVAL;
    return nullptr;
  }
  int fd = open(path, flags);
  if (fd < 0) {
#ifndef __arm__
    _DEBUG("%s", strerror(errno));
#endif
    return nullptr;
  }
#ifndef __arm__
  _DEBUG("opened %s with fd %d", path, fd);
#endif
  return fdopen(fd, mode);
}
}
