#include "getdents.h"
#include "input.h"

#define _DIRENT_H
#include <bits/dirent.h>
extern "C" int
open(const char* pathname, int flags, ...);
namespace Libc {
  extern int (*getdents)(unsigned int, struct dirent*, unsigned int);
  extern ssize_t (*getdents64)(int, void*, size_t);
  extern struct dirent* (*readdir)(DIR*);
  extern struct dirent64* (*readdir64)(DIR*);
}
namespace Client {
  extern bool INITIALIZED;
  extern bool isFakeRM1Input();
}

extern "C" {
int
getdents(unsigned int fd, struct dirent* dirp, unsigned int count) {
  if (!Client::INITIALIZED || !Client::isFakeRM1Input()) {
    return Libc::getdents(fd, dirp, count);
  }
  return Input::getdents(fd, dirp, count, Libc::getdents);
}

ssize_t
getdents64(int fd, void* dirp, size_t count) {
  if (!Client::INITIALIZED || !Client::isFakeRM1Input()) {
    return Libc::getdents64(fd, dirp, count);
  }
  return Input::getdents<struct dirent64>(
    fd, static_cast<struct dirent64*>(dirp), count, Libc::getdents64
  );
}

struct dirent*
readdir(DIR* dirp) {
  if (!Client::INITIALIZED || !Client::isFakeRM1Input()) {
    return Libc::readdir(dirp);
  }
  while (true) {
    struct dirent* entry = Libc::readdir(dirp);
    if (entry == nullptr) {
      return nullptr;
    }
    if (!Input::shouldHideEvent(entry->d_name)) {
      return entry;
    }
  }
}

struct dirent64*
readdir64(DIR* dirp) {
  if (!Client::INITIALIZED || !Client::isFakeRM1Input()) {
    return Libc::readdir64(dirp);
  }
  while (true) {
    struct dirent64* entry = Libc::readdir64(dirp);
    if (entry == nullptr) {
      return nullptr;
    }
    if (!Input::shouldHideEvent(entry->d_name)) {
      return entry;
    }
  }
}
}
