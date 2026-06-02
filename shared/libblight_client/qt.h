#pragma once
#include <cstdint>
#include <libblight/types.h>

namespace Qt {
  typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
  } QRectLayout;
  typedef void (*qregion_constructor_t)(void* this_ptr, const void* rect);
  typedef void (*qregion_destructor_t)(void* this_ptr);
  typedef const QRectLayout* (*qregion_begin_t)(const void* this_ptr);
  typedef const QRectLayout* (*qregion_end_t)(const void* this_ptr);
  qregion_constructor_t qregion_constructor();
  qregion_destructor_t qregion_destructor();
  qregion_begin_t qregion_begin();
  qregion_end_t qregion_end();
  bool region_rect_overlaps(const QRectLayout* rect, const void* region);
  Blight::WaveformMode epsm_to_waveform(int screenMode);
  Blight::UpdateMode flags_to_update_mode(int flags);
}

extern "C" {
__attribute__((visibility("default"))) void
hook_swapBuffers_QRegion(
  void* this_ptr,
  const void* region,
  const void* screenModeStruct,
  int flags
);

__attribute__((visibility("default"))) void
_ZN19EPFramebufferSwtcon6updateE5QRecti9PixelModei(
  void* this_ptr,
  Qt::QRectLayout rect,
  int waveform,
  int pixelMode,
  int flags
);

__attribute__((visibility("default"))) void
_ZN7QObjectC2EPS_(void* this_ptr, void* parent);
__asm__(".symver _ZN7QObjectC2EPS_, _ZN7QObjectC2EPS_@Qt_6");

__attribute__((visibility("default"))) void
_ZN7QObjectC1EPS_(void* this_ptr, void* parent);
__asm__(".symver _ZN7QObjectC1EPS_, _ZN7QObjectC1EPS_@Qt_6");

__attribute__((visibility("default"))) void
_ZN6QImageC1Ev(void* this_ptr);
__asm__(".symver _ZN6QImageC1Ev, _ZN6QImageC1Ev@Qt_6");
}
