#include "fopen.h"

#include <fcntl.h>

extern "C" {
// fdopen not defined to allow using the actual symbol

FILE*
fopen(const char* path, const char* mode) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return nullptr;
  }
  return fdopen(fd, mode);
}

FILE*
fopen64(const char* path, const char* mode) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return nullptr;
  }
  return fdopen(fd, mode);
}
}
