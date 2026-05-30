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
    size_t len = 8;
#if defined(__aarch64__)
    len = 16;
#endif
    // Make sure all the bytes we overwrite are in writable pages
    while ((uintptr_t)page + ps < (uintptr_t)target + len) {
        ps += sysconf(_SC_PAGESIZE);
    }
    mprotect(page, ps, PROT_READ | PROT_WRITE | PROT_EXEC);
#if defined(__arm__)
    // Overwrite the first 8 bytes of target with:
    //   LDR PC, [PC, #-4]   (loads from target+4 into PC)
    //   <hook address>
    // No separate trampoline needed -- LDR PC can reach any 32-bit address.
    *(uint32_t*)target = 0xe51ff004;
    *(uint32_t*)((char*)target + 4) = (uintptr_t)hook;
    __builtin___clear_cache(target, (char*)target + 8);
#elif defined(__aarch64__)
    // Overwrite the first 16 bytes of target with:
    //   LDR X16, #8          (load 8 bytes from PC+8 into X16)
    //   BR X16               (branch to X16)
    //   <hook address, 8 bytes>
    *(uint32_t*)target = 0x58000050;
    *(uint32_t*)((char*)target + 4) = 0xd61f0200;
    *(uint64_t*)((char*)target + 8) = (uintptr_t)hook;
    __builtin___clear_cache(target, (char*)target + 16);
#else
    _CRIT("%s", "Unsupported architecture");
    std::exit(EXIT_FAILURE);
#endif
}

int
get_vtable_offset()
{
    const char* env = getenv("OXIDE_EPFRAMEBUFFER_VTABLE_OFFSET");
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
    // Check arm32 prologue: STMDB SP!, {reglist} + sub sp, sp, #0x44
    //
    // First instruction: STMDB SP!, {reglist} = 0xE92Dxxxx
    //   - bits 31-28: cond (AL=1110) -> 0xE
    //   - bits 27-23: 10010 -> STMDB
    //   - bit 21: 1 -> write-back (!)
    //   - bits 19-16: 1101 -> SP (R13)
    //   - lower 16 bits: register mask (bit N = register N)
    //
    // We only verify that LR (bit 14) is in the list; the specific
    // set of caller-saves (R4-R12) varies by compiler version.
    uint32_t first = *(uint32_t*)func;
    if ((first & 0xFFFF0000) != 0xE92D0000) {
        _WARN(
            "swapBuffers: first instruction not STMDB SP!, got 0x%08x", first
        );
        return false;
    }
    if (!(first & 0x4000)) {
        _WARN(
            "swapBuffers: LR (bit 14) not in push mask 0x%04x", first & 0xFFFF
        );
        return false;
    }
    // Second instruction: sub sp, sp, #0x44 = 0xE24DD044
    uint32_t second = *(uint32_t*)((char*)func + 4);
    if (second != 0xE24DD044) {
        _WARN(
            "swapBuffers: second instruction not sub sp,#0x44, got 0x%08x",
            second
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
    const void* /*contentMap_screenModeMap*/,
    int flags
)
{
    _DEBUG("%s", "EPFramebuffer::swapBuffers(QRegion, ...)");
    auto begin_fn = Qt::qregion_begin();
    auto end_fn = Qt::qregion_end();
    if (!begin_fn || !end_fn) {
        _DEBUG("hook: qregion_begin/end is NULL");
        return;
    }

    const Qt::QRectLayout* it = begin_fn(region);
    const Qt::QRectLayout* end = end_fn(region);
    if (!it || !end) {
        _DEBUG("hook: iterator or end is NULL");
        return;
    }

    for (; it != end; it++) {
        int waveform = 2;
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
static void
qobject_ctor_post_hook(void* self, void* parent)
{
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

void
_ZN7QObjectC2EPS_(void* self, void* parent)
{
    static auto ctor =
        (void (*)(void*, void*))dlsym(RTLD_NEXT, "_ZN7QObjectC2EPS_");
    if (!ctor) {
        _CRIT(
            "%s",
            "Failed to resolve real QObject::QObject(QObject*) [C2] via "
            "RTLD_NEXT"
        );
        std::exit(EXIT_FAILURE);
    }
    ctor(self, parent);
    qobject_ctor_post_hook(self, parent);
}

void
_ZN7QObjectC1EPS_(void* self, void* parent)
{
    static auto ctor =
        (void (*)(void*, void*))dlsym(RTLD_NEXT, "_ZN7QObjectC1EPS_");
    if (!ctor) {
        _CRIT(
            "%s",
            "Failed to resolve real QObject::QObject(QObject*) [C1] via "
            "RTLD_NEXT"
        );
        std::exit(EXIT_FAILURE);
    }
    ctor(self, parent);
    qobject_ctor_post_hook(self, parent);
}
