#pragma once
#include <cstdint>
#include <libblight/connection.h>
#include <libblight/types.h>
#include <linux/fb.h>
#include <mxcfb.h>

namespace FB {
  extern Blight::shared_buf_t buffer;
  extern Blight::Connection* connection;
  extern int frameBuffer;
  extern int epframebufferLockFd;
  extern int epdLockFd;
  extern int msgq;
  extern float visibleYRatio;
  extern float visibleXRatio;
  bool init();
  bool is_fb(int fd);
  void ensure_surface();
  int send_update(mxcfb_update_data* update);
  int wait(mxcfb_update_marker_data* update);
  int get_vscreeninfo(fb_var_screeninfo* screenInfo);
  int get_fscreeninfo(fb_fix_screeninfo* screenInfo);
  int ioctl(unsigned long request, char* ptr);
  Blight::Format deviceFormat();
  unsigned int deviceXres();
  unsigned int deviceYres();
  unsigned int deviceActualWidth();
  unsigned int deviceActualHeight();
  unsigned int deviceStride();
  unsigned int deviceBitsPerPixel();
  double deviceScale();
  int createBuffer();
  Blight::maybe_ackid_ptr_t repaint(
    int x,
    int y,
    int width,
    int height,
    Blight::WaveformMode waveform,
    Blight::ContentType contentType,
    Blight::UpdateMode updateMode,
    unsigned int marker,
    bool wait = false
  );
  Blight::WaveformMode mxcfb_to_blight_waveform(int waveform);
}
namespace swtfb {
  enum MSG_TYPE { INIT_t = 1, UPDATE_t, XO_t, WAIT_t };

  struct xochitl_data {
    int x1;
    int y1;
    int x2;
    int y2;

    int waveform;
    int flags;
  };
  struct wait_sem_data {
    char sem_name[512];
  };
  struct swtfb_update {
    long mtype;
    struct {
      union {
        xochitl_data xochitl_update;
        struct mxcfb_update_data update;
        wait_sem_data wait_update;
      };
    } mdata;
  };
}
struct fb_fix_screeninfo_32 {
  uint8_t id[16];
  uint32_t smem_start;
  uint32_t smem_len;
  uint32_t type;
  uint32_t type_aux;
  uint32_t visual;
  uint16_t xpanstep;
  uint16_t ypanstep;
  uint16_t ywrapstep;
  uint32_t line_length;
  uint32_t mmio_start;
  uint32_t mmio_len;
  uint32_t accel;
  uint16_t capabilities;
  uint16_t reserved[2];
};
extern "C" __attribute__((visibility("default"))) Blight::shared_buf_t
__BLIGHT_BUFFER();
