#pragma once
#include <cstdint>

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
    static bool rects_overlap(const QRectLayout* a, const QRectLayout* b);
}
