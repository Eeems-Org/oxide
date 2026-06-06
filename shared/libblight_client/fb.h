#pragma once
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
  bool init();
  bool is_fb(int fd);
  void ensure_surface();
  int send_update(mxcfb_update_data* update);
  int wait(mxcfb_update_marker_data* update);
  int get_vscreeninfo(fb_var_screeninfo* screenInfo);
  int get_fscreeninfo(fb_fix_screeninfo* screenInfo);
  int ioctl(unsigned long request, char* ptr);
  int exclusive_ioctl(unsigned long request, char* ptr);
  Blight::Format deviceFormat();
  unsigned int deviceXres();
  unsigned int deviceYres();
  unsigned int deviceStride();
  unsigned int deviceBitsPerPixel();
  int createBuffer();
  Blight::maybe_ackid_ptr_t repaint(
    int x,
    int y,
    int width,
    int height,
    Blight::WaveformMode waveform,
    Blight::UpdateMode updateMode,
    unsigned int marker,
    bool wait = false
  );
}
namespace swtfb {
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
} // namespace swtfb
