#include "qt.h"

#include <cstdlib>
#include <dlfcn.h>
#include <libblight/debug.h>
#include <libblight/system.h>
#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>

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
}

#if defined(__arm__)
static constexpr int kEpfAuxBufferOffset = 0x60;
static constexpr int kEpfMainBufferOffset = 0x50;
#elif defined(__aarch64__)
static constexpr int kEpfAuxBufferOffset = 0xa8;
static constexpr int kEpfMainBufferOffset = 0xc8;
#else
static constexpr int kEpfAuxBufferOffset = 0x00;
static constexpr int kEpfMainBufferOffset = 0x00;
#endif

static std::atomic<bool> hook_installed = false;
static std::atomic<void*> g_epf_candidate{ nullptr };

// Tracks persistently bad EPF candidates so we can clear them and
// let the default-ctor hook store a fresh (hopefully correct) one.
static std::atomic<void*> g_last_bad_candidate{ nullptr };
static std::atomic<int> g_bad_candidate_count{ 0 };

void
install_hook(void* target, void* hook)
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
        std::exit(EXIT_FAILURE);
    }
#if defined(__arm__)
    // 8 bytes: ldr pc,[pc,#-4] + addr
    *(uint32_t*)tramp = 0xe51ff004;
    *(uint32_t*)((char*)tramp + 4) = (uintptr_t)hook;
    int32_t offset = (intptr_t)tramp - (intptr_t)target - 8;
    *(uint32_t*)target = 0xEA000000 | ((offset >> 2) & 0x00FFFFFF);
    __builtin___clear_cache(target, (char*)target + 4);
#elif defined(__aarch64__)
    // 12 bytes: ldr x16,[pc,#8] + br x16 + addr
    *(uint32_t*)tramp = 0x58000050;              // ldr x16, #8
    *(uint32_t*)((char*)tramp + 4) = 0xd61f0200; // br x16
    *(uint64_t*)((char*)tramp + 8) = (uintptr_t)hook;
    int64_t offset = (intptr_t)tramp - (intptr_t)target;
    *(uint32_t*)target = 0x14000000 | ((offset >> 2) & 0x03FFFFFF);
    __builtin___clear_cache(target, (char*)target + 4);
#else
    _CRIT("%s", "Unsupported architecture");
    std::exit(EXIT_FAILURE);
#endif
}

int
get_vtable_offset()
{
    const char* env = getenv("OXIDE_EPF_VTABLE_OFFSET");
    if (env) {
        return std::stoi(env, nullptr, 0); // handles hex with 0x prefix
    }
    // TODO handle aarch64
    return 0x5c; // default for arm32 (vtable entry 23)
}

bool
validate_swapbuffers(void* func)
{
    if (!func) {
        return false;
    }
    Dl_info info;
    if (!dladdr(func, &info)) {
        return false;
    }
    if (!info.dli_fname) {
        return false;
    }
    std::string fname(info.dli_fname);
    if (fname.find("xochitl") == std::string::npos &&
        fname.find("libqsgepaper") == std::string::npos) {
        return false;
    }
#if defined(__arm__)
    // arm32 prologue for EPFramebuffer_swapBuffers_QRegion:
    // F0 5F 2D E9   push {r4-r11,lr}
    // 44 D0 4D E2   sub sp, sp, #0x44
    static const uint8_t expected[8] = { 0xF0, 0x5F, 0x2D, 0xE9,
                                         0x44, 0xD0, 0x4D, 0xE2 };
    if (memcmp(func, expected, 8) != 0) {
        _WARN(
            "swapBuffers prologue mismatch at %p (offset 0x%x) -- "
            "vtable offset may be wrong; set OXIDE_EPF_VTABLE_OFFSET",
            func,
            get_vtable_offset()
        );
        return false;
    }
#elif defined(__aarch64__)
    // TODO handle aarch64 version
#else
    _CRIT("%s", "Unsupported architecture");
    std::exit(EXIT_FAILURE);
#endif
    return true;
}

#if defined(__arm__)
void
_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_(
    void* this_ptr,
    char* data,
    int width,
    int height,
    int bytesPerLine,
    int format,
    void* cleanupFunction,
    void* cleanupInfo
)
#elif defined(__aarch64__)
void
_ZN6QImageC1EPhiixNS_6FormatEPFvPvES2_(
    void* this_ptr,
    char* data,
    int width,
    int height,
    long long bytesPerLine,
    int format,
    void* cleanupFunction,
    void* cleanupInfo
)
#endif
#if defined(__arm__) || defined(__aarch64__)
{
    static auto func =
#if defined(__arm__)
        (void (*)(void*, char*, int, int, int, int, void*, void*))dlsym(
            RTLD_NEXT, "_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_"
        );
#elif defined(__aarch64__)
        (void (*)(void*, char*, int, int, long long, int, void*, void*))dlsym(
            RTLD_NEXT, "_ZN6QImageC1EPhiixNS_6FormatEPFvPvES2_"
        );
#endif
    if (!func) {
        _CRIT(
            "%s",
            "Failed to resolve real QImage::QImage(unsigned char*, int, "
            "int, "
#if defined(__arm__)
            "long long"
#elif defined(__aarch64__)
            "int"
#endif
            ", QImage::Format, void (*)(void*), void*) via "
            "RTLD_NEXT"
        );
        std::exit(EXIT_FAILURE);
    }
    func(
        this_ptr,
        data,
        width,
        height,
        bytesPerLine,
        format,
        cleanupFunction,
        cleanupInfo
    );
}
#endif

void
_ZN6QImageC1Ev(void* this_ptr)
{
    static auto func = (void (*)(void*))dlsym(RTLD_NEXT, "_ZN6QImageC1Ev");
    if (!func) {
        _CRIT("%s", "Failed to resolve QImage::QImage() via RTLD_NEXT");
        std::exit(EXIT_FAILURE);
    }
    func(this_ptr);
    if (hook_installed) {
        return;
    }
    _DEBUG("QImage::QImage() default");
    // Default ctor: format not yet set. Try both known EPF offsets.
    // The vtable at (this - offset) is checked to confirm it belongs
    // to a xochitl QObject (fully dynamic via dladdr).
    for (int off : { kEpfMainBufferOffset, kEpfAuxBufferOffset }) {
        if (off == 0) {
            continue; // skip unsupported arch placeholder
        }
        void* candidate = (char*)this_ptr - off;
        void* vtable = *(void**)candidate;
        // Quick pre-filter: aligned, non-null, reasonable address
        if (!vtable || (uintptr_t)vtable & 3 ||
            (uintptr_t)vtable < 0x00010000) {
            continue;
        }
        Dl_info info;
        if (!dladdr(vtable, &info)) {
            continue;
        }
        if (!info.dli_fname) {
            continue;
        }
        std::string fname(info.dli_fname);
        if (fname.find("xochitl") == std::string::npos &&
            fname.find("libqsgepaper") == std::string::npos) {
            continue;
        }
        _DEBUG(
            "Default ctor at %p -> EPF candidate at %p "
            "(vtable=%p in %s)",
            this_ptr,
            candidate,
            vtable,
            info.dli_fname
        );
        g_epf_candidate.store(candidate, std::memory_order_release);
        break;
    }
}

void
hook_swapBuffers_QRect(
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

void
hook_swapBuffers_QRegion(
    void* this_ptr,
    const void* region,
    const void* contentMap,
    const void* screenModeMap,
    int flags
)
{
    _DEBUG("%s", "EPFramebuffer::swapBuffers(QRegion, ...)");
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
                    static const int mode_params[6][2] = { { 2, 7 }, { 1, 7 },
                                                           { 2, 7 }, { 2, 7 },
                                                           { 6, 7 }, { 1, 7 } };
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
void
_ZN7QObjectC2EP7QObject(void* self, void* parent)
{
    // Resolve the real QObject constructor (skip this library via
    // RTLD_NEXT)
    static auto ctor =
        (void (*)(void*, void*))dlsym(RTLD_NEXT, "_ZN7QObjectC2EP7QObject");
    if (!ctor) {
        _CRIT(
            "%s",
            "Failed to resolve real QObject::QObject(QObject*) via "
            "RTLD_NEXT"
        );
        std::exit(EXIT_FAILURE);
    }
    ctor(self, parent);
    if (hook_installed) {
        return;
    }
    _DEBUG("%s", "QObject::QObject(QObject*)");
    // Re-check the EPFramebuffer candidate (discovered via QImage hook).
    // Its vtable may have been updated from a temporary to the final
    // class vtable since the last time we checked.
    void* candidate = g_epf_candidate.load(std::memory_order_acquire);
    if (!candidate) {
        return; // QImage hook hasn't fired yet
    }
    int offset = get_vtable_offset();
    void* vtable = *(void**)candidate;
    _DEBUG("EPF candidate at %p, vtable=%p", candidate, vtable);
    if (!vtable) {
        _DEBUG("%s", "vtable is null, skipping");
        return;
    }
    void* func = *(void**)((char*)vtable + offset);
    _DEBUG("swapBuffers candidate function at %p", func);
    if (!validate_swapbuffers(func)) {
        _DEBUG(
            "validate_swapbuffers(%p) FAILED for candidate %p, vtable=%p",
            func,
            candidate,
            vtable
        );
        // Give the vtable time to become final.  If the same candidate
        // keeps failing many times it is almost certainly wrong (e.g. a
        // different xochitl QObject that happens to have a QImage at
        // offset +0x50 / +0x60).  Clear it so the default-ctor hook
        // can store a fresh candidate.
        void* expected = g_last_bad_candidate.load(std::memory_order_relaxed);
        if (candidate != expected) {
            g_last_bad_candidate.store(candidate, std::memory_order_relaxed);
            g_bad_candidate_count.store(1, std::memory_order_relaxed);
            return;
        }
        int n =
            g_bad_candidate_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n >= 10) {
            _WARN(
                "EPF candidate %p failed %d times -- clearing "
                "(likely wrong object)",
                candidate,
                n
            );
            g_epf_candidate.store(nullptr, std::memory_order_release);
            g_last_bad_candidate.store(nullptr, std::memory_order_relaxed);
            g_bad_candidate_count.store(0, std::memory_order_relaxed);
        }
        return; // try again next QObject
    }
    _DEBUG(
        "Found EPFramebuffer at %p, vtable=%p, swapBuffers_QRegion=%p",
        candidate,
        vtable,
        func
    );
    install_hook(func, (void*)&hook_swapBuffers_QRegion);
    _DEBUG("Hooked swapBuffers(QRegion,...)");
    // Also try to hook the QRect overload (one vtable slot before
    // on ARM32)
    void* qrect_func = *(void**)((char*)vtable + offset - sizeof(void*));
    if (validate_swapbuffers(qrect_func)) {
        install_hook(qrect_func, (void*)&hook_swapBuffers_QRect);
        _DEBUG("Hooked swapBuffers(QRect, ...)");
    }
    hook_installed = true;
    g_epf_candidate.store(nullptr, std::memory_order_release);
}
