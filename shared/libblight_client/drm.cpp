#include "drm.h"
#include "libc.h"
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
    if (!Client::isFbEnabled()) {
      return Libc::ioctl(card0, request, ptr);
    }
    switch (request) {
      case DRM_IOCTL_MODE_ATOMIC:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_ATOMIC");
        return 0;
      case DRM_IOCTL_MODE_CREATE_DUMB:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_CREATE_DUMB");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_MAP_DUMB:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_MAP_DUMB");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_DESTROY_DUMB:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_DESTROY_DUMB");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_SETCRTC:
        _DEBUG("%s", "ioctl /dev/dri/card0 DRM_IOCTL_MODE_SETCRTC");
        return 0;
      case DRM_IOCTL_GET_UNIQUE:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_GET_UNIQUE");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_GET_CLIENT:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_GET_CLIENT");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_GET_CAP:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_GET_CAP");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_SET_CLIENT_CAP:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_SET_CLIENT_CAP");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_IRQ_BUSID:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_IRQ_BUSID");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_GETRESOURCES:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_MODE_GETRESOURCES");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_GETENCODER:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_MODE_GETENCODER");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_GETCONNECTOR:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_MODE_GETCONNECTOR");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_GETPROPERTY:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_MODE_GETPROPERTY");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_ADDFB:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_MODE_ADDFB");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_GETPLANERESOURCES:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_MODE_GETPLANERESOURCES");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_GETPLANE:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_MODE_GETPLANE");
        return Libc::ioctl(card0, request, ptr);
      case DRM_IOCTL_MODE_OBJ_GETPROPERTIES:
        _DEBUG("ioctl /dev/dri/card0 DRM_IOCTL_MODE_OBJ_GETPROPERTIES");
        return Libc::ioctl(card0, request, ptr);
      default:
        _WARN(
          "UNHANDLED DRM IOCTL %lu %c %lu %lu %lu",
          _IOC_DIR(request),
          (char)_IOC_TYPE(request),
          _IOC_NR(request),
          _IOC_SIZE(request),
          request
        );
        return Libc::ioctl(card0, request, ptr);
    }
  }
}
#endif
