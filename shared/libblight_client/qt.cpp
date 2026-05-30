#include "qt.h"
#include "fb.h"
#include "state.h"

#include <cstdlib>
#include <dlfcn.h>
#include <libblight/debug.h>
#include <libblight/system.h>
#include <sys/mman.h>
#include <sys/stat.h>
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

// Runtime pointers to real QImage methods (resolved once via dlsym).
struct QFuncs
{
    void (*ctor)(void*, char*, int, int, int, int, void*, void*);
    void (*dtor)(void*);
    unsigned char* (*bits)(void*);
    int (*width)(void*);
    int (*height)(void*);
    int (*bytesPerLine)(void*);
    int (*format)(void*);
    bool ok;
};

static QFuncs qImageFuncs;

/*!
 * \brief Get various QImage function pointers
 */
static void
resolve_qimage_funcs()
{
    if (qImageFuncs.ok) {
        return;
    }
    qImageFuncs.ctor = (decltype(qImageFuncs.ctor))dlsym(
        RTLD_DEFAULT, "_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_"
    );
    qImageFuncs.dtor =
        (decltype(qImageFuncs.dtor))dlsym(RTLD_DEFAULT, "_ZN6QImageD1Ev");
    qImageFuncs.bits =
        (decltype(qImageFuncs.bits))dlsym(RTLD_DEFAULT, "_ZNK6QImage4bitsEv");
    qImageFuncs.width =
        (decltype(qImageFuncs.width))dlsym(RTLD_DEFAULT, "_ZNK6QImage5widthEv");
    qImageFuncs.height = (decltype(qImageFuncs.height))dlsym(
        RTLD_DEFAULT, "_ZNK6QImage6heightEv"
    );
    qImageFuncs.bytesPerLine = (decltype(qImageFuncs.bytesPerLine))dlsym(
        RTLD_DEFAULT, "_ZNK6QImage12bytesPerLineEv"
    );
    qImageFuncs.format = (decltype(qImageFuncs.format))dlsym(
        RTLD_DEFAULT, "_ZNK6QImage6formatEv"
    );
    qImageFuncs.ok = qImageFuncs.ctor && qImageFuncs.dtor && qImageFuncs.bits;
    if (!qImageFuncs.ok) {
        _WARN(
            "Failed to resolve QImage runtime functions: "
            "ctor=%p dtor=%p bits=%p",
            (void*)qImageFuncs.ctor,
            (void*)qImageFuncs.dtor,
            (void*)qImageFuncs.bits
        );
    }
}

/*!
 * \brief mmap the exclusive mode framebuffer
 */
static std::pair<void*, size_t>
mmap_framebuffer()
{
    if (FB::frameBuffer < 0) {
        FB::frameBuffer = Blight::frameBuffer();
    }
    if (FB::frameBuffer < 0) {
        _WARN("%s", "redirect: Blight::frameBuffer() failed");
        return { nullptr, 0 };
    }
    struct stat st;
    if (fstat(FB::frameBuffer, &st) < 0) {
        _WARN("%s", "redirect: fstat(FB::frameBuffer) failed");
        close(FB::frameBuffer);
        return { nullptr, 0 };
    }
    size_t len = (size_t)st.st_size;
    void* p = mmap(
        nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, FB::frameBuffer, 0
    );
    close(FB::frameBuffer);
    if (p == MAP_FAILED) {
        _WARN("%s", "redirect: mmap(FB::frameBuffer) failed");
        return { nullptr, 0 };
    }
    _DEBUG("redirect: mmap'd server framebuffer (%zu bytes)", len);
    return { p, len };
}

#if defined(__arm__)
static constexpr int auxBufferOffset = 0x60;
static constexpr int mainBufferOffset = 0x50;
#elif defined(__aarch64__)
static constexpr int auxBufferOffset = 0xa8;
static constexpr int mainBufferOffset = 0xc8;
#else
static constexpr int auxBufferOffset = 0x00;
static constexpr int mainBufferOffset = 0x00;
#endif
// QImage layout: vtable pointer followed by QImageData* d_ptr.
// Offset of d_ptr depends on pointer size.
#if defined(__arm__)
static constexpr int d_ptrOffset = 4;
#elif defined(__aarch64__)
static constexpr int d_ptrOffset = 8;
#else
static constexpr int d_ptrOffset = 0;
#endif

/*!
 * \brief Swap the buffer for a QImage
 */
bool
swap_qimage_buffer(void* qimage, Blight::Format format, void* data, size_t size)
{
    resolve_qimage_funcs();
    if (!qImageFuncs.ok) {
        errno = EBADFD;
        return false;
    }
    int imageFormat = qImageFuncs.format(qimage);
    if (imageFormat != format) {
        errno = EINVAL;
        return false;
    }
    int width = qImageFuncs.width(qimage);
    int height = qImageFuncs.height(qimage);
    int stride = qImageFuncs.bytesPerLine(qimage);
    unsigned char* old_data = qImageFuncs.bits(qimage);
    if (old_data == nullptr || width <= 0 || height <= 0 || stride <= 0) {
        errno = EBADF;
        return false;
    }
    size_t needed = (size_t)stride * (size_t)height;
    if (needed > size) {
        errno = EOVERFLOW;
        return false;
    }
    memcpy(data, old_data, needed);
    char tmp_buf[32];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    qImageFuncs.ctor(
        tmp_buf, (char*)data, width, height, stride, format, nullptr, nullptr
    );
    void** dptr_a = (void**)((char*)qimage + d_ptrOffset);
    void** dptr_b = (void**)((char*)tmp_buf + d_ptrOffset);
    void* tmp = *dptr_a;
    *dptr_a = *dptr_b;
    *dptr_b = tmp;
    qImageFuncs.dtor(tmp_buf);
    return true;
}

static std::atomic<bool> qImageRedirected{ false };

/*!
 * \brief Swap the QImage buffers for auxBuffer and mainBuffer to blight buffers
 */
static void
redirect_epframebuffer_qimages(void* epframebuffer)
{
    if (mainBufferOffset == 0x00) {
        return;
    }
    if (qImageRedirected.load(std::memory_order_acquire)) {
        return;
    }
    resolve_qimage_funcs();
    if (!qImageFuncs.ok) {
        return;
    }

    void* mainBuffer = (char*)epframebuffer + mainBufferOffset;
    if (!Client::HANDLE_FB) {
        static std::pair<void*, size_t> s_mmap{ nullptr, 0 };
        if (!s_mmap.first) {
            s_mmap = mmap_framebuffer();
        }
        if (!s_mmap.first) {
            _WARN("%s", "redirect: falling back (display may show stale data)");
            qImageRedirected.store(true, std::memory_order_release);
            return;
        }
        void* fb_data = s_mmap.first;
        size_t fb_capacity = s_mmap.second;
        // Exclusive-mode: redirect the 8-bit auxBuffer so that after
        // xochitl's swapBuffers converts mainBuffer->auxBuffer, the 8-bit
        // data lands directly in the shared memfd where the server reads it.
        void* auxBuffer = (char*)epframebuffer + auxBufferOffset;
        bool success = swap_qimage_buffer(
            auxBuffer, Blight::Format::Format_Grayscale8, fb_data, fb_capacity
        );
        int height = qImageFuncs.height(auxBuffer);
        int stride = qImageFuncs.bytesPerLine(auxBuffer);
        size_t size = (size_t)stride * (size_t)height;
        if (!success) {
            switch (errno) {
                case EBADFD:
                    _DEBUG("%s", "redirect: Unable to get QImage functions");
                    break;
                case EINVAL:
                    _DEBUG(
                        "redirect: format %d, expected Format_Grayscale8 – "
                        "skipping redirect",
                        qImageFuncs.format(auxBuffer)
                    );
                    break;
                case EBADF:
                    _DEBUG("%s", "redirect: invalid auxBuffer dimensions");
                    break;
                case EOVERFLOW: {
                    _WARN(
                        "redirect: framebuffer too small (%zu bytes) for "
                        "auxBuffer "
                        "(%dx%d stride=%d = %zu bytes)",
                        fb_capacity,
                        qImageFuncs.width(auxBuffer),
                        height,
                        stride,
                        size
                    );
                    break;
                }
            }
        }
        _INFO(
            "redirect: auxBuffer now points at shared framebuffer "
            "(%dx%d stride=%d)",
            qImageFuncs.width(auxBuffer),
            qImageFuncs.height(auxBuffer),
            qImageFuncs.bytesPerLine(auxBuffer)
        );
        auto format = FB::deviceFormat();
        if (!swap_qimage_buffer(
                mainBuffer,
                format,
                &static_cast<char*>(fb_data)[size],
                fb_capacity - size
            )) {
            switch (errno) {
                case EBADFD:
                    _DEBUG("%s", "redirect: Unable to get QImage functions");
                    break;
                case EINVAL:
                    _DEBUG(
                        "redirect: format %d, expected %d – "
                        "skipping redirect",
                        qImageFuncs.format(mainBuffer),
                        format
                    );
                    break;
                case EBADF:
                    _DEBUG("%s", "redirect: invalid mainBuffer dimensions");
                    break;
                case EOVERFLOW: {
                    int height = qImageFuncs.height(mainBuffer);
                    int stride = qImageFuncs.bytesPerLine(mainBuffer);
                    _WARN(
                        "redirect: framebuffer too small (%zu bytes) for "
                        "mainBuffer "
                        "(%dx%d stride=%d = %zu bytes)",
                        fb_capacity,
                        qImageFuncs.width(mainBuffer),
                        height,
                        stride,
                        (size_t)stride * (size_t)height
                    );
                    break;
                }
            }
        }
        qImageRedirected.store(true, std::memory_order_release);
        return;
    }
    if (FB::buffer == nullptr || FB::buffer->data == nullptr) {
        _DEBUG("%s", "redirect: FB::buffer not ready yet, will retry");
        return;
    }
    void* fb_data = FB::buffer->data;
    size_t fb_capacity = FB::buffer->size();
    auto format = qImageFuncs.format(mainBuffer);
    if (!swap_qimage_buffer(
            mainBuffer,
            static_cast<Blight::Format>(format),
            fb_data,
            fb_capacity
        )) {
        switch (errno) {
            case EBADFD:
                _DEBUG("%s", "redirect: Unable to get QImage functions");
                break;
            case EINVAL:
                _DEBUG(
                    "redirect: format %d, expected %d – "
                    "skipping redirect",
                    qImageFuncs.format(mainBuffer),
                    format
                );
                break;
            case EBADF:
                _DEBUG("%s", "redirect: invalid mainBuffer dimensions");
                break;
            case EOVERFLOW: {
                int height = qImageFuncs.height(mainBuffer);
                int stride = qImageFuncs.bytesPerLine(mainBuffer);
                _WARN(
                    "redirect: framebuffer too small (%zu bytes) for "
                    "mainBuffer "
                    "(%dx%d stride=%d = %zu bytes)",
                    fb_capacity,
                    qImageFuncs.width(mainBuffer),
                    height,
                    stride,
                    (size_t)stride * (size_t)height
                );
                break;
            }
        }
    }
    _INFO(
        "redirect: mainBuffer now points at blight buffer"
        "(%dx%d stride=%d)",
        qImageFuncs.width(mainBuffer),
        qImageFuncs.height(mainBuffer),
        qImageFuncs.bytesPerLine(mainBuffer)
    );
    qImageRedirected.store(true, std::memory_order_release);
}

static std::atomic<bool> hook_installed = false;
static std::atomic<void*> epframebufferCandidate{ nullptr };
static std::atomic<void*> lastBadCandidate{ nullptr };
static std::atomic<int> badCandidateCount{ 0 };

/*!
 * \brief Install a trampoline into a function to run our hook instead
 */
void
install_hook(void* target, void* hook)
{
    long ps = sysconf(_SC_PAGESIZE);
    void* page = (void*)((uintptr_t)target & ~(ps - 1));
    size_t len = 8;
#if defined(__arm__)
    len = 8;
#elif defined(__aarch64__)
    len = 16;
#else
    _CRIT("%s", "Unsupported architecture");
    std::exit(EXIT_FAILURE);
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
#endif
}

/*!
 * \brief Ensure that a pointer references a EPFramebuffer::swapBuffers(...)
 * method
 */
bool
validate_swapbuffers(void* func)
{
    if (func == nullptr) {
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

/*!
 * \brief Hook to run on EPFramebuffer::swapBuffers(QRect, ...) call
 */
void
hook_swapBuffers_QRect(
    void* this_ptr,
    Qt::QRectLayout rect,
    int contentType,
    int screenMode,
    int flags
)
{
    _DEBUG("%S", "EPFramebuffer::swapBuffers(QRect, ...)");
    auto waveform = Qt::epsm_to_waveform(screenMode);
    auto updateMode = Qt::flags_to_update_mode(flags);
    _DEBUG(
        "repaint: (%d, %d) (%d, %d) %d",
        rect.left,
        rect.top,
        rect.right,
        rect.bottom,
        waveform,
        updateMode
    );
    if (Client::HANDLE_FB) {
        auto maybe = FB::connection->repaint(
            FB::buffer,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            waveform,
            updateMode,
            0
        );
        if (maybe.has_value()) {
            maybe.value()->wait();
        }
    } else {
        Blight::exclusiveModeRepaint(
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            waveform,
            updateMode
        );
    }
}

/*!
 * \brief Hook to run on EPFramebuffer::swapBuffers(QRegion, ...) call
 */
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
        // TODO get and convert screen mode to waveform
        Blight::WaveformMode waveform =
            Blight::WaveformMode::HighQualityGrayscale;
        auto updateMode = Qt::flags_to_update_mode(flags);
        _DEBUG(
            "repaint: (%d, %d) (%d, %d) %d",
            it->left,
            it->top,
            it->right,
            it->bottom,
            waveform,
            updateMode
        );
        if (Client::HANDLE_FB) {
            auto maybe = FB::connection->repaint(
                FB::buffer,
                it->left,
                it->top,
                it->right - it->left,
                it->bottom - it->top,
                waveform,
                updateMode,
                0
            );
            if (maybe.has_value()) {
                maybe.value()->wait();
            }
        } else {
            Blight::exclusiveModeRepaint(
                it->left,
                it->top,
                it->right - it->left,
                it->bottom - it->top,
                waveform,
                updateMode
            );
        }
    }
}

/*!
 * \brief Hook to run on QObject::QObject(QObject*) call
 */
static void
hook_qobject_constructor(void* self, void* parent)
{
    if (hook_installed || Client::deviceType == Client::DeviceType::RM1) {
        return;
    }
    _DEBUG("%s", "QObject::QObject(QObject*)");
    // Re-check the EPFramebuffer candidate (discovered via QImage hook).
    // Its vtable may have been updated from a temporary to the final
    // class vtable since the last time we checked.
    void* candidate = epframebufferCandidate.load(std::memory_order_acquire);
    if (candidate == nullptr) {
        return; // QImage hook hasn't fired yet
    }
    int offset;
    const char* offsetenv = getenv("OXIDE_EPFRAMEBUFFER_VTABLE_OFFSET");
    if (offsetenv) {
        offset = std::stoi(offsetenv, nullptr, 0);
    } else {
#if defined(__arm__)
        offset = 0x5c; //(vtable entry 23)
#elif defined(__aarch64__)
// TODO add aarch64 offset
#else
        _CRIT("Unsupported architecture");
        std::exit(EXIT_FAILURE);
#endif
    }
    void* vtable = *(void**)candidate;
    _DEBUG("EPF candidate at %p, vtable=%p", candidate, vtable);
    if (!vtable) {
        _DEBUG("%s", "vtable is null, skipping");
        return;
    }
    void* func_swapBuffers_qregion = *(void**)((char*)vtable + offset);
    _DEBUG("swapBuffers candidate function at %p", func_swapBuffers_qregion);
    if (!validate_swapbuffers(func_swapBuffers_qregion)) {
        _DEBUG(
            "validate_swapbuffers(%p) FAILED for QRegion candidate %p, "
            "vtable=%p",
            func_swapBuffers_qregion,
            candidate,
            vtable
        );
        void* expected = lastBadCandidate.load(std::memory_order_relaxed);
        if (candidate != expected) {
            lastBadCandidate.store(candidate, std::memory_order_relaxed);
            badCandidateCount.store(1, std::memory_order_relaxed);
            return;
        }
        int n = badCandidateCount.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n >= 10) {
            _WARN(
                "EPF candidate %p failed %d times -- clearing "
                "(likely wrong object)",
                candidate,
                n
            );
            epframebufferCandidate.store(nullptr, std::memory_order_release);
            lastBadCandidate.store(nullptr, std::memory_order_relaxed);
            badCandidateCount.store(0, std::memory_order_relaxed);
        }
        return; // try again next QObject
    }
    _DEBUG(
        "Found EPFramebuffer at %p, vtable=%p, swapBuffers_QRegion=%p",
        candidate,
        vtable,
        func_swapBuffers_qregion
    );
    redirect_epframebuffer_qimages(candidate);
    install_hook(func_swapBuffers_qregion, (void*)&hook_swapBuffers_QRegion);
    _DEBUG("Hooked swapBuffers(QRegion,...)");
    void* func_swapBuffers_qrect =
        *(void**)((char*)vtable + offset - sizeof(void*));
    if (validate_swapbuffers(func_swapBuffers_qrect)) {
        install_hook(func_swapBuffers_qrect, (void*)&hook_swapBuffers_QRect);
        _DEBUG("Hooked swapBuffers(QRect, ...)");
    } else {
        _DEBUG(
            "validate_swapbuffers(%p) FAILED for QRect candidate %p, vtable=%p",
            func_swapBuffers_qregion,
            candidate,
            vtable
        );
    }
    hook_installed = true;
    epframebufferCandidate.store(nullptr, std::memory_order_release);
}

/*!
 * \brief QObject::QObject(QObject*) constructor
 */
void
_ZN7QObjectC2EPS_(void* self, void* parent)
{
    static auto func =
        (void (*)(void*, void*))dlsym(RTLD_NEXT, "_ZN7QObjectC2EPS_");
    if (!func) {
        _CRIT(
            "%s",
            "Failed to resolve real QObject::QObject(QObject*) [C2] via "
            "RTLD_NEXT"
        );
        std::exit(EXIT_FAILURE);
    }
    func(self, parent);
    hook_qobject_constructor(self, parent);
}

/*!
 * \brief QObject::QObject(QObject*) constructor
 */
void
_ZN7QObjectC1EPS_(void* self, void* parent)
{
    static auto func =
        (void (*)(void*, void*))dlsym(RTLD_NEXT, "_ZN7QObjectC1EPS_");
    if (!func) {
        _CRIT(
            "%s",
            "Failed to resolve real QObject::QObject(QObject*) [C1] via "
            "RTLD_NEXT"
        );
        std::exit(EXIT_FAILURE);
    }
    func(self, parent);
    hook_qobject_constructor(self, parent);
}

/*!
 * \brief Alternative QImage::QImage() constructor
 */
void
_ZN6QImageC1Ev(void* this_ptr)
{
    static auto func = (void (*)(void*))dlsym(RTLD_NEXT, "_ZN6QImageC1Ev");
    if (!func) {
        _CRIT("%s", "Failed to resolve QImage::QImage() via RTLD_NEXT");
        std::exit(EXIT_FAILURE);
    }
    func(this_ptr);
    if (hook_installed || Client::deviceType == Client::DeviceType::RM1) {
        return;
    }
    _DEBUG("QImage::QImage() default");
    // Default ctor: format not yet set. Try both known EPF offsets.
    // The vtable at (this - offset) is checked to confirm it belongs
    // to a xochitl QObject (fully dynamic via dladdr).
    for (int offset : { mainBufferOffset, auxBufferOffset }) {
        if (offset == 0) {
            continue; // skip unsupported arch placeholder
        }
        void* candidate = (char*)this_ptr - offset;
        void* vtable = *(void**)candidate;
        // Quick pre-filter: aligned, non-null, reasonable address
        if (!vtable || (uintptr_t)vtable & 3 ||
            (uintptr_t)vtable < 0x00010000) {
            continue;
        }
        Dl_info info;
        if (!dladdr(vtable, &info) || info.dli_fname == nullptr ||
            !*info.dli_fname) {
            continue;
        }
        if (!Client::IS_XOCHITL) {
            char* resolved = realpath(info.dli_fname, nullptr);
            if (resolved == nullptr) {
                continue;
            }
            bool match =
                strcmp(
                    resolved, "/usr/lib/plugins/scenegraph/libqsgepaper.so"
                ) == 0;
            free(resolved);
            if (!match) {
                continue;
            }
        }
        _DEBUG(
            "Default ctor at %p -> EPF candidate at %p "
            "(vtable=%p in %s)",
            this_ptr,
            candidate,
            vtable,
            info.dli_fname
        );
        epframebufferCandidate.store(candidate, std::memory_order_release);
        break;
    }
}
