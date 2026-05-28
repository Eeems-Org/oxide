#include "qt.h"
#include <dlfcn.h>

namespace Qt {
    qregion_begin_t qregion_begin()
    {
        static qregion_begin_t qregion_begin =
            (qregion_begin_t)dlsym(RTLD_DEFAULT, "_ZNK7QRegion5beginEv");
        return qregion_begin;
    }
    qregion_end_t qregion_end()
    {
        static qregion_end_t qregion_end =
            (qregion_end_t)dlsym(RTLD_DEFAULT, "_ZNK7QRegion3endEv");
        return qregion_end;
    }
    static bool rects_overlap(const QRectLayout* a, const QRectLayout* b)
    {
        return a->left < b->right && a->right > b->left && a->top < b->bottom &&
               a->bottom > b->top;
    }
}
