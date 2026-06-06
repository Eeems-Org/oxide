#include "drm.h"
#include "state.h"

#include <drm/drm.h>
#include <libblight/debug.h>
#include <sys/ioctl.h>

#ifdef __aarch64__
namespace DRM {
  int card0 = -1;
  bool is_drm(int fd) {
    return card0 > 0 && card0 == fd;
  }
  int ioctl(unsigned long request, char* ptr) {
    switch (request) {
      case DRM_IOCTL_MODE_ATOMIC:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_ATOMIC");
        return -1;
      case DRM_IOCTL_MODE_CREATE_DUMB:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_CREATE_DUMB");
        return -1;
      case DRM_IOCTL_MODE_MAP_DUMB:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_MAP_DUMB");
        return -1;
      case DRM_IOCTL_MODE_DESTROY_DUMB:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_DESTROY_DUMB");
        return -1;
      default:
        _WARN(
          "UNHANDLED DRM IOCTL %lu %c %lu %lu %lu",
          _IOC_DIR(request),
          (char)_IOC_TYPE(request),
          _IOC_NR(request),
          _IOC_SIZE(request),
          request
        );
        return 0;
    }
  }
}
#endif
