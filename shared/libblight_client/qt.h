#pragma once
#include <cstdint>
#include <libblight/types.h>

namespace Qt {
    typedef struct
    {
        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;
    } QRectLayout;
    typedef const QRectLayout* (*qregion_begin_t)(const void* self);
    typedef const QRectLayout* (*qregion_end_t)(const void* self);
    qregion_begin_t qregion_begin();
    qregion_end_t qregion_end();
    bool rects_overlap(const QRectLayout* a, const QRectLayout* b);
    typedef void* (*epsm_region_t)(const void* map, int mode);
    epsm_region_t epsm_region();
    Blight::WaveformMode epsm_to_waveform(int screenMode);
    Blight::UpdateMode flags_to_update_mode(int flags);
    void hook(void* lib);
}
