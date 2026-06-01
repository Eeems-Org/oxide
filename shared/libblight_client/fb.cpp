#include "fb.h"

#include "libc.h"
#include "state.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/mman.h>

#include <libblight.h>
#include <libblight/clock.h>
#include <libblight/debug.h>
#include <libblight/system.h>
#include <libblight/types.h>

namespace FB {
  Blight::shared_buf_t buffer = nullptr;
  Blight::Connection* connection = nullptr;
  int frameBuffer = -1;
  int epframebufferLockFd = -1;
  int epdLockFd = -1;
  int msgq = -1;
  void init() {
    std::ios_base::Init i;
    std::ifstream device_id_file{"/sys/devices/soc0/machine"};
    std::string device_id;
    std::getline(device_id_file, device_id);
    if (
      device_id == "reMarkable 1.0" || device_id == "reMarkable Prototype 1"
    ) {
      Client::deviceType = Client::DeviceType::RM1;
    } else if (device_id == "reMarkable 2.0") {
      Client::deviceType = Client::DeviceType::RM2;
    } else if (device_id == "reMarkable Ferrari") {
      Client::deviceType = Client::DeviceType::RMPP;
    } else if (device_id == "reMarkable Chiapa") {
      Client::deviceType = Client::DeviceType::RMPPM;
    } else if (device_id == "reMarkable Tatsu") {
      Client::deviceType = Client::DeviceType::RMPPURE;
    } else {
      Client::deviceType = Client::DeviceType::UNKNOWN;
    }
  }
  bool is_fb(int fd) {
    return Client::HANDLE_FB && buffer->fd > 0 && buffer->fd == fd;
  }
  void ensure_surface() {
    if (!buffer->surface) {
      Blight::addSurface(buffer);
    }
    if (!buffer->surface) {
      _CRIT("Failed to create surface: %s", std::strerror(errno));
      std::exit(errno);
    }
  }
  int send_update(mxcfb_update_data* update) {
    _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SEND_UPDATE")
    Blight::ClockWatch cw;
    ensure_surface();
    auto region = update->update_region;
    auto maybe = connection->repaint(
      buffer,
      region.left,
      region.top,
      region.width,
      region.height,
      (Blight::WaveformMode)update->waveform_mode,
      (Blight::UpdateMode)update->update_mode,
      update->update_marker
    );
    if (maybe.has_value()) {
      maybe.value()->wait();
    }
    _DEBUG("ioctl /dev/fb0 MXCFB_SEND_UPDATE done: %f", cw.elapsed());
    // TODO - notify on rM2 for screensharing
    return 0;
  }
  int wait(mxcfb_update_marker_data* update) {
    _DEBUG("%s", "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE");
    Blight::ClockWatch cw;
    connection->waitForMarker(update->update_marker);
    _DEBUG(
      "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE done: %f", cw.elapsed()
    );
    return 0;
  }
  int get_vscreeninfo(
    fb_var_screeninfo* screenInfo,
    int width,
    int height,
    int stride
  ) {
    screenInfo->xres = width;
    screenInfo->yres = height;
    screenInfo->xoffset = 0;
    screenInfo->yoffset = 0;
    screenInfo->xres_virtual = width;
    screenInfo->yres_virtual = height;
    screenInfo->bits_per_pixel = stride / width * 8;
    // TODO - determine the following from the buffer format
    switch (Client::deviceType) {
      case Client::DeviceType::RM1:
        // Format_RGB16 / RGB565
        screenInfo->grayscale = 0;
        screenInfo->red.offset = 11;
        screenInfo->red.length = 5;
        screenInfo->red.msb_right = 0;
        screenInfo->green.offset = 5;
        screenInfo->green.length = 6;
        screenInfo->green.msb_right = 0;
        screenInfo->blue.offset = 0;
        screenInfo->blue.length = 5;
        screenInfo->blue.msb_right = 0;
        screenInfo->transp.offset = 0;
        screenInfo->transp.length = 0;
        screenInfo->transp.msb_right = 0;
        break;
      default:
        // Format_RGB888 / RGB888
        screenInfo->grayscale = 0;
        screenInfo->red.offset = 16;
        screenInfo->red.length = 8;
        screenInfo->red.msb_right = 0;
        screenInfo->green.offset = 8;
        screenInfo->green.length = 8;
        screenInfo->green.msb_right = 0;
        screenInfo->blue.offset = 0;
        screenInfo->blue.length = 8;
        screenInfo->blue.msb_right = 0;
        screenInfo->transp.offset = 0;
        screenInfo->transp.length = 0;
        screenInfo->transp.msb_right = 0;
        break;
    }
    // TODO what should this even be?
    screenInfo->nonstd = 0;
    screenInfo->activate = FB_ACTIVATE_FORCE;
    // It would be cool to have the actual mm width/height here
    screenInfo->height = 0;
    screenInfo->width = 0;
    screenInfo->accel_flags = 0;
    // screenInfo->pixclock = 28800; // Stick with what is reported
    screenInfo->left_margin = 0;
    screenInfo->right_margin = 0;
    screenInfo->upper_margin = 0;
    screenInfo->lower_margin = 0;
    screenInfo->hsync_len = 1;
    screenInfo->vsync_len = 1;
    screenInfo->sync = 0;
    screenInfo->vmode = 0;
    screenInfo->rotate = 0;
    screenInfo->colorspace = 0;
    screenInfo->reserved[0] = 0;
    screenInfo->reserved[1] = 0;
    screenInfo->reserved[2] = 0;
    screenInfo->reserved[3] = 0;
    return 0;
  }
  int get_fscreeninfo(fb_fix_screeninfo* screenInfo) {
    constexpr char fb_id[] = "mxcfb";
    memcpy(screenInfo->id, fb_id, sizeof(fb_id));
    // TODO determine all the values dynamically
    screenInfo->smem_len = buffer->size();
    screenInfo->smem_start = (unsigned long)buffer->data;
    screenInfo->type = 0;
    screenInfo->type_aux = 0;
    screenInfo->visual = FB_VISUAL_TRUECOLOR;
    screenInfo->xpanstep = 8;
    screenInfo->ypanstep = 0;
    screenInfo->ywrapstep = 5772;
    screenInfo->line_length = buffer->stride;
    screenInfo->mmio_start = 0;
    screenInfo->mmio_len = 0;
    screenInfo->accel = 0;
    screenInfo->reserved[0] = 0;
    screenInfo->reserved[1] = 0;
    return 0;
  }
  void print_vscreeninfo(fb_var_screeninfo* screenInfo) {
    _DEBUG(
      "\n"
      "res: %dx%d\n"
      "res_virtual: %dx%d\n"
      "offset: %d, %d\n"
      "bits_per_pixel: %d\n"
      "grayscale: %d\n"
      "red: %d, %d, %d\n"
      "green: %d, %d, %d\n"
      "blue: %d, %d, %d\n"
      "tansp: %d, %d, %d\n"
      "nonstd: %d\n"
      "activate: %d\n"
      "size: %dx%dmm\n"
      "accel_flags: %d\n"
      "pixclock: %d\n"
      "margins: %d %d %d %d\n"
      "hsync_len: %d\n"
      "vsync_len: %d\n"
      "sync: %d\n"
      "vmode: %d\n"
      "rotate: %d\n"
      "colorspace: %d\n"
      "reserved: %d, %d, %d, %d\n",
      screenInfo->xres,
      screenInfo->yres,
      screenInfo->xres_virtual,
      screenInfo->yres_virtual,
      screenInfo->xoffset,
      screenInfo->yoffset,
      screenInfo->bits_per_pixel,
      screenInfo->grayscale,
      screenInfo->red.offset,
      screenInfo->red.length,
      screenInfo->red.msb_right,
      screenInfo->green.offset,
      screenInfo->green.length,
      screenInfo->green.msb_right,
      screenInfo->blue.offset,
      screenInfo->blue.length,
      screenInfo->blue.msb_right,
      screenInfo->transp.offset,
      screenInfo->transp.length,
      screenInfo->transp.msb_right,
      screenInfo->nonstd,
      screenInfo->activate,
      screenInfo->height,
      screenInfo->width,
      screenInfo->accel_flags,
      screenInfo->pixclock,
      screenInfo->left_margin,
      screenInfo->right_margin,
      screenInfo->upper_margin,
      screenInfo->lower_margin,
      screenInfo->hsync_len,
      screenInfo->vsync_len,
      screenInfo->sync,
      screenInfo->vmode,
      screenInfo->rotate,
      screenInfo->colorspace,
      screenInfo->reserved[0],
      screenInfo->reserved[1],
      screenInfo->reserved[2],
      screenInfo->reserved[3]
    );
  }
  void print_offset(fb_var_screeninfo* screenInfo) {
    _DEBUG(
      "\n"
      "offset: %d, %d\n",
      screenInfo->xoffset,
      screenInfo->yoffset
    );
  }
  int ioctl(unsigned long request, char* ptr) {
    switch (request) {
      // Look at linux/fb.h and mxcfb.h for more possible request types
      // https://www.kernel.org/doc/html/latest/fb/api.html
      case MXCFB_SEND_UPDATE: {
        return send_update(reinterpret_cast<mxcfb_update_data*>(ptr));
      }
      case MXCFB_WAIT_FOR_UPDATE_COMPLETE: {
        return wait(reinterpret_cast<mxcfb_update_marker_data*>(ptr));
      }
      case FBIOGET_VSCREENINFO: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_FSCREENINFO");
        // TODO - handle getting information on rMPP/rMPPM
        int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
        if (fd == -1) {
          return -1;
        }
        if (Libc::ioctl(fd, request, ptr) == -1) {
          return -1;
        }
        Libc::close(fd);
        return get_vscreeninfo(
          reinterpret_cast<fb_var_screeninfo*>(ptr),
          buffer->width,
          buffer->height,
          buffer->stride
        );
      }
      case FBIOGET_FSCREENINFO: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_VSCREENINFO");
        // TODO - handle getting information on rMPP/rMPPM
        int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
        if (fd == -1) {
          return -1;
        }
        if (Libc::ioctl(fd, request, ptr) == -1) {
          return -1;
        }
        Libc::close(fd);
        return get_fscreeninfo(reinterpret_cast<fb_fix_screeninfo*>(ptr));
      }
      case FBIOPUT_VSCREENINFO: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOPUT_VSCREENINFO");
        // TODO - handle updating fb
        // TODO - Explore allowing some screen info updating
        // TODO allow resizing?
        print_vscreeninfo(reinterpret_cast<fb_var_screeninfo*>(ptr));
        return 0;
      }
      case MXCFB_SET_AUTO_UPDATE_MODE:
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_AUTO_UPDATE_MODE");
        return 0;
      case MXCFB_SET_UPDATE_SCHEME:
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_UPDATE_SCHEME");
        return 0;
      case MXCFB_ENABLE_EPDC_ACCESS:
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_ENABLE_EPDC_ACCESS");
        return 0;
      case MXCFB_DISABLE_EPDC_ACCESS:
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_DISABLE_EPDC_ACCESS");
        return 0;
      case FBIOPAN_DISPLAY: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOPAN_DISPLAY");
        print_offset(reinterpret_cast<fb_var_screeninfo*>(ptr));
        return 0;
      }
      case FBIOBLANK: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOBLANK");
        int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
        if (fd == -1) {
          return -1;
        }
        return Libc::ioctl(fd, request, ptr);
      }
      default:
        _WARN(
          "UNHANDLED Fb IOCTL %lu %c %lu %lu %lu",
          _IOC_DIR(request),
          (char)_IOC_TYPE(request),
          _IOC_NR(request),
          _IOC_SIZE(request),
          request
        );
        return 0;
    }
  }
  int exclusive_ioctl(unsigned long request, char* ptr) {
    switch (request) {
      // Look at linux/fb.h and mxcfb.h for more possible request types
      // https://www.kernel.org/doc/html/latest/fb/api.html
      case MXCFB_SEND_UPDATE: {
        // TODO handle region updates
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SEND_UPDATE");
        Blight::ClockWatch cw;
        mxcfb_update_data* update = reinterpret_cast<mxcfb_update_data*>(ptr);
        auto region = update->update_region;
        Blight::exclusiveModeRepaint(
          region.left,
          region.top,
          region.width,
          region.height,
          (Blight::WaveformMode)update->waveform_mode,
          (Blight::UpdateMode)update->update_mode
        );
        _DEBUG("ioctl /dev/fb0 MXCFB_SEND_UPDATE done: %f", cw.elapsed())
        return 0;
      }
      case MXCFB_WAIT_FOR_UPDATE_COMPLETE: {
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE");
        Blight::ClockWatch cw;
        Blight::waitForNoRepaints();
        _DEBUG(
          "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE done: %f", cw.elapsed()
        )
        return 0;
      }
      case FBIOGET_FSCREENINFO: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_FSCREENINFO");
        // TODO - handle getting information on rMPP/rMPPM
        int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
        if (fd == -1) {
          return -1;
        }
        int res = Libc::ioctl(fd, request, ptr);
        Libc::close(fd);
        return res;
      }
      case FBIOGET_VSCREENINFO: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_VSCREENINFO");
        // TODO - handle getting information on rMPP/rMPPM
        int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
        if (fd == -1) {
          return -1;
        }
        if (Libc::ioctl(fd, request, ptr) == -1) {
          return -1;
        }
        Libc::close(fd);
        std::tuple<int, int, int> info = Blight::frameBufferInfo();
        if (std::get<0>(info) == -1) {
          return -1;
        }
        return get_vscreeninfo(
          reinterpret_cast<fb_var_screeninfo*>(ptr),
          std::get<0>(info),
          std::get<1>(info),
          std::get<2>(info)
        );
      }
      case FBIOPUT_VSCREENINFO: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOPUT_VSCREENINFO");
        // TODO - handle updating fb
        // TODO - Explore allowing some screen info updating
        // TODO allow resizing?
        print_vscreeninfo(reinterpret_cast<fb_var_screeninfo*>(ptr));
        return 0;
      }
      case MXCFB_SET_AUTO_UPDATE_MODE:
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_AUTO_UPDATE_MODE");
        return 0;
      case MXCFB_SET_UPDATE_SCHEME:
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_UPDATE_SCHEME");
        return 0;
      case MXCFB_ENABLE_EPDC_ACCESS:
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_ENABLE_EPDC_ACCESS");
        return 0;
      case MXCFB_DISABLE_EPDC_ACCESS:
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_DISABLE_EPDC_ACCESS");
        return 0;
      case FBIOPAN_DISPLAY: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOPAN_DISPLAY");
        print_offset(reinterpret_cast<fb_var_screeninfo*>(ptr));
        // if (Client::deviceType == Client::DeviceType::RM2) {
        //     Blight::exclusiveModeRepaintFull();
        // }
        return 0;
      }
      case FBIOBLANK: {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOBLANK");
        int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
        if (fd == -1) {
          return -1;
        }
        return Libc::ioctl(fd, request, ptr);
      }
      default:
        _WARN(
          "UNHANDLED Fb IOCTL %lu %c %lu %lu %lu",
          _IOC_DIR(request),
          (char)_IOC_TYPE(request),
          _IOC_NR(request),
          _IOC_SIZE(request),
          request
        );
        return 0;
    }
  }
  Blight::Format deviceFormat() {
    if (Client::FAKE_RM1 || Client::RM1_FB) {
      return Blight::Format::Format_RGB16;
    }
    switch (Client::deviceType) {
      case Client::DeviceType::RM1:
        return Blight::Format::Format_RGB16;
      default:
        return Blight::Format::Format_RGB32;
    }
  }
  int deviceXres() {
    if (Client::FAKE_RM1 || Client::RM1_FB) {
      return 1404;
    }
    switch (Client::deviceType) {
      case Client::DeviceType::RM1:
      case Client::DeviceType::RM2:
      case Client::DeviceType::RMPPURE:
        return 1404;
      case Client::DeviceType::RMPP:
        return 1620;
      case Client::DeviceType::RMPPM:
        return 954;
      default:
        return 0;
    }
  }
  int deviceYres() {
    if (Client::FAKE_RM1 || Client::RM1_FB) {
      return 1872;
    }
    switch (Client::deviceType) {
      case Client::DeviceType::RM1:
      case Client::DeviceType::RM2:
      case Client::DeviceType::RMPPURE:
        return 1872;
      case Client::DeviceType::RMPP:
        return 2160;
      case Client::DeviceType::RMPPM:
        return 1696;
      default:
        return 0;
    }
  }
  int deviceStride() {
    return deviceXres() * deviceBitsPerPixel() / 8;
  }
  int deviceBitsPerPixel() {
    if (Client::FAKE_RM1 || Client::RM1_FB) {
      return 16;
    }
    switch (Client::deviceType) {
      case Client::DeviceType::RM1:
        return 16;
      default:
        return 32;
    }
  }
  void createBuffer() {
    if (Client::deviceType == Client::DeviceType::RM2) {
      // rM2 requires a much larger buffer than is actually used
      auto buf = new Blight::buf_t{
        .fd = -1,
        .x = 0,
        .y = 0,
        .width = deviceXres(),
        .height = deviceYres(),
        .stride = deviceStride(),
        .format = deviceFormat(),
        .data = nullptr,
        .uuid = Blight::buf_t::new_uuid(),
        .surface = 0
      };
      buf->fd = memfd_create(buf->uuid.c_str(), MFD_ALLOW_SEALING);
      if (buf->fd == -1) {
        return;
      }
      // Magic larger number that rM2 uses based on virtual sizes
      if (ftruncate(buf->fd, 26359808)) {
        return;
      }
      void* data = mmap(
        NULL,
        buf->size(),
        PROT_READ | PROT_WRITE,
        MAP_SHARED_VALIDATE,
        buf->fd,
        0
      );
      if (data == MAP_FAILED || data == nullptr) {
        return;
      }
      buf->data = reinterpret_cast<Blight::data_t>(data);
      buffer = Blight::shared_buf_t(buf);
    } else {
      auto maybe = Blight::createBuffer(
        0, 0, deviceXres(), deviceYres(), deviceStride(), deviceFormat()
      );
      if (!maybe.has_value()) {
        return;
      }
      buffer = maybe.value();
    }
    // We don't ever plan on resizing, and we shouldn't let anything
    // try
    int flags = fcntl(buffer->fd, F_GET_SEALS);
    fcntl(
      buffer->fd, F_ADD_SEALS, flags | F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW
    );
    // Initialize the buffer with white (all bytes = 0xFF)
    memset(FB::buffer->data, 0xFF, FB::buffer->size());
  }
}
