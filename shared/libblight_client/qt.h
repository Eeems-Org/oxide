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
}

extern "C"
{
    __attribute__((visibility("default"))) void hook_swapBuffers_QRect(
        void* this_ptr,
        Qt::QRectLayout rect,
        int contentType,
        int screenMode,
        int flags
    );
    __attribute__((visibility("default"))) void hook_swapBuffers_QRegion(
        void* this_ptr,
        const void* region,
        const void* screenModeStruct,
        int flags
    );

    __attribute__((visibility("default"))) void
    _ZN7QObjectC2EPS_(void* self, void* parent);
    __asm__(".symver _ZN7QObjectC2EPS_, _ZN7QObjectC2EPS_@Qt_6");

    __attribute__((visibility("default"))) void
    _ZN7QObjectC1EPS_(void* self, void* parent);
    __asm__(".symver _ZN7QObjectC1EPS_, _ZN7QObjectC1EPS_@Qt_6");

#if defined(__arm__)
    __attribute__((visibility("default"))) void
    _ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_(
        void* this_ptr,
        char* data,
        int width,
        int height,
        int bytesPerLine,
        int format,
        void* cleanupFunction,
        void* cleanupInfo
    );
    __asm__(".symver _ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_ , "
            "_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_@Qt_6");

    __attribute__((visibility("default"))) void _ZN6QImageC1Ev(void* this_ptr);
    __asm__(".symver _ZN6QImageC1Ev, _ZN6QImageC1Ev@Qt_6");
#elif defined(__aarch64__)
    __attribute__((visibility("default"))) void
    _ZN6QImageC1EPhiixNS_6FormatEPFvPvES2_(
        void* this_ptr,
        char* data,
        int width,
        int height,
        long long bytesPerLine,
        int format,
        void* cleanupFunction,
        void* cleanupInfo
    );
    __asm__(".symver _ZN6QImageC1EPhiixNS_6FormatEPFvPvES2_ , "
            "_ZN6QImageC1EPhiixNS_6FormatEPFvPvES2_@Qt_6");

    __attribute__((visibility("default"))) void _ZN6QImageC1Ev(void* this_ptr);
    __asm__(".symver _ZN6QImageC1Ev, _ZN6QImageC1Ev@Qt_6");
#endif
}
