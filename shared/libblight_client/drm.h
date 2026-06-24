#pragma once
#ifdef __aarch64__
namespace DRM {
  extern int card0;
  bool is_drm(int fd);
  int ioctl(unsigned long request, char* ptr);
}
#endif
