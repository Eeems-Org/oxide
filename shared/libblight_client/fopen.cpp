#include "fopen.h"
#include "libc.h"
#include "state.h"

#include <cstring>
#include <fcntl.h>
#include <libblight/debug.h>

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
  _DEBUG("fopen %s %s", path, mode);
  int flags = modeToFlags(mode);
  if (flags == -1) {
    errno = EINVAL;
    return nullptr;
  }
  int fd = open(path, flags);
  if (fd < 0) {
    return nullptr;
  }
  _DEBUG("opened %s with fd %d", path, fd);
  return fdopen(fd, mode);
}

FILE*
fopen64(const char* path, const char* mode) {
  if (!Client::INITIALIZED) {
    return Libc::fopen64(path, mode);
  }
  _DEBUG("fopen64 %s %s", path, mode);
  int flags = modeToFlags(mode);
  if (flags == -1) {
    _DEBUG("%s", strerror(errno));
    errno = EINVAL;
    return nullptr;
  }
  int fd = open(path, flags);
  if (fd < 0) {
    _DEBUG("%s", strerror(errno));
    return nullptr;
  }
  _DEBUG("opened %s with fd %d", path, fd);
  return fdopen(fd, mode);
}
}
