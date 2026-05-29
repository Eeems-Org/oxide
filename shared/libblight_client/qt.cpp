#include "qt.h"

#include <dlfcn.h>
#include <libblight/debug.h>
#include <libblight/system.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C"
{
    __attribute__((visibility("default"))) void hook_swapBuffers_QRect(
        void* this_ptr,
        Qt::QRectLayout rect,
        int contentType,
        int screenMode,
        int flags
    )
    {
        _DEBUG(
            "EPFramebuffer::swapBuffers({%i, %i, %i, %i}, contentType=%i, "
            "screenMode=%i, flags=%i)",
            rect.left,
            rect.top,
            rect.right,
            rect.bottom,
            contentType,
            screenMode,
            flags
        );
        Blight::exclusiveModeRepaint(
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            Qt::epsm_to_waveform(screenMode),
            Qt::flags_to_update_mode(flags)
        );
    }
    __attribute__((visibility("default"))) void hook_swapBuffers_QRegion(
        void* this_ptr,
        const void* region,
        const void* contentMap,
        const void* screenModeMap,
        int flags
    )
    {
        _DEBUG("%s", "EPFramebuffer::swapBuffers(QRegion...)");
        const Qt::QRectLayout* it = Qt::qregion_begin()(region);
        const Qt::QRectLayout* end = Qt::qregion_end()(region);
        for (; it != end; it++) {
            // Use EPScreenModeMap to determine waveform per rect
            int waveform = 2, pixel = 7; // defaults matching swapBuffers_impl
            bool found = false;
            for (int m = 0; m < 6 && !found; m++) {
                void* mr = Qt::epsm_region()(screenModeMap, m);
                if (!mr)
                    continue;
                const Qt::QRectLayout* mi = Qt::qregion_begin()(mr);
                const Qt::QRectLayout* me = Qt::qregion_end()(mr);
                for (; mi != me; mi++) {
                    if (Qt::rects_overlap(it, mi)) {
                        // Hardcoded from swapBuffers_impl disassembly:
                        // {waveform, pixel} per mode
                        static const int mode_params[6][2] = {
                            { 2, 7 }, { 1, 7 }, { 2, 7 },
                            { 2, 7 }, { 6, 7 }, { 1, 7 }
                        };
                        waveform = mode_params[m][0];
                        pixel = mode_params[m][1];
                        found = true;
                        break;
                    }
                }
            }
            Blight::exclusiveModeRepaint(
                it->left,
                it->top,
                it->right - it->left,
                it->bottom - it->top,
                (Blight::WaveformMode)waveform,
                Qt::flags_to_update_mode(flags)
            );
        }
    }
}
namespace Qt {
    qregion_begin_t qregion_begin()
    {
        static qregion_begin_t fn =
            (qregion_begin_t)dlsym(RTLD_DEFAULT, "_ZNK7QRegion5beginEv");
        return fn;
    }
    qregion_end_t qregion_end()
    {
        static qregion_end_t fn =
            (qregion_end_t)dlsym(RTLD_DEFAULT, "_ZNK7QRegion3endEv");
        return fn;
    }
    bool rects_overlap(const QRectLayout* a, const QRectLayout* b)
    {
        return a->left < b->right && a->right > b->left && a->top < b->bottom &&
               a->bottom > b->top;
    }
    epsm_region_t epsm_region()
    {
        static epsm_region_t fn = (epsm_region_t)dlsym(
            RTLD_DEFAULT, "_ZNK15EPScreenModeMap6regionE12EPScreenMode"
        );
        return fn;
    }
    Blight::WaveformMode epsm_to_waveform(int screenMode)
    {
        switch (screenMode) {
            case 0:
                return Blight::WaveformMode::Mono;
            case 1:
                return Blight::WaveformMode::Mono;
            case 2:
                return Blight::WaveformMode::Animate;
            case 3:
                return Blight::WaveformMode::HighQualityGrayscale;
            case 4:
                return Blight::WaveformMode::Grayscale;
            case 5:
                return Blight::WaveformMode::Grayscale;
            default:
                return Blight::WaveformMode::Mono;
        }
    }
    Blight::UpdateMode flags_to_update_mode(int flags)
    {
        return (flags & 1) ? Blight::UpdateMode::FullUpdate
                           : Blight::UpdateMode::PartialUpdate;
    }
#ifdef __arm__
    void write_abs_jump(void* tramp, void* hook)
    {
        // 8 bytes: ldr pc,[pc,#-4] + addr
        *(uint32_t*)tramp = 0xe51ff004;
        *(uint32_t*)((char*)tramp + 4) = (uintptr_t)hook;
    }
#else // aarch64
    void write_abs_jump(void* tramp, void* hook)
    {
        // 12 bytes: ldr x16,[pc,#8] + br x16 + addr
        *(uint32_t*)tramp = 0x58000050;              // ldr x16, #8
        *(uint32_t*)((char*)tramp + 4) = 0xd61f0200; // br x16
        *(uint64_t*)((char*)tramp + 8) = (uintptr_t)hook;
    }
#endif
    void write_branch(void* target, void* dest)
    {
#ifdef __arm__
        int32_t offset = (intptr_t)dest - (intptr_t)target - 8;
        *(uint32_t*)target = 0xEA000000 | ((offset >> 2) & 0x00FFFFFF);
#else
        int64_t offset = (intptr_t)dest - (intptr_t)target;
        *(uint32_t*)target = 0x14000000 | ((offset >> 2) & 0x03FFFFFF);
#endif
        __builtin___clear_cache(target, (char*)target + 4);
    }
    void install_hook(void* target, void* hook)
    {
        long ps = sysconf(_SC_PAGESIZE);
        void* page = (void*)((uintptr_t)target & ~(ps - 1));
        mprotect(page, ps, PROT_READ | PROT_WRITE | PROT_EXEC);
        // Allocate trampoline near target
        void* tramp = mmap(
            (void*)((uintptr_t)target + 0x20000),
            4096,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
            -1,
            0
        );
        if (tramp == MAP_FAILED) {
            tramp = mmap(
                NULL,
                4096,
                PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1,
                0
            );
        }
        if (tramp == MAP_FAILED) {
            _CRIT("%s", "Failed to mmap trampoline for hook!");
            return;
        }
        write_abs_jump(tramp, hook); // trampoline -> hook
        write_branch(target, tramp); // original -> trampoline
    }
    static pthread_once_t lib_patched = PTHREAD_ONCE_INIT;
    static void* gsgepaper_lib = nullptr;
    void _hook()
    {
        void* qrect_sb = dlsym(
            gsgepaper_lib,
            "_ZN13EPFramebuffer11swapBuffersE5QRect13EPContentType12EPScree"
            "nMod"
            "e6QFlagsINS_10UpdateFlagEE"
        );
        if (qrect_sb) {
            _DEBUG("Hooking %s", "hook_swapBuffers_QRect");
            install_hook(qrect_sb, (void*)&hook_swapBuffers_QRect);
        }
        void* qregion_sb = dlsym(
            gsgepaper_lib,
            "_ZN13EPFramebuffer11swapBuffersERK7QRegionRK12EPContentMapRK15"
            "EPSc"
            "reenModeMap6QFlagsINS_10UpdateFlagEE"
        );
        if (qregion_sb) {
            _DEBUG("Hooking %s", "hook_swapBuffers_QRegion");
            install_hook(qregion_sb, (void*)&hook_swapBuffers_QRegion);
        }
    }
    void hook(void* lib)
    {
        gsgepaper_lib = lib;
        pthread_once(&lib_patched, _hook);
    }
}
