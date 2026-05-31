#include "qt.h"
#include "fb.h"
#include "state.h"

#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <libblight.h>
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

static std::atomic<void*> framebufferMmap{ nullptr };
static std::atomic<size_t> framebufferMmapSize{ 0 };

/*!
 * \brief mmap the exclusive mode framebuffer
 */
static std::pair<void*, size_t>
mmap_framebuffer()
{
    void* ptr = framebufferMmap.load(std::memory_order_acquire);
    if (ptr != nullptr) {
        return { ptr, framebufferMmapSize.load(std::memory_order_acquire) };
    }
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
    ptr = mmap(
        nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, FB::frameBuffer, 0
    );
    auto err = errno;
    close(FB::frameBuffer);
    if (ptr == MAP_FAILED) {
        _WARN("redirect: mmap(FB::frameBuffer) failed: %s", std::strerror(err));
        return { nullptr, 0 };
    }
    _DEBUG("redirect: mmap'd server framebuffer (%zu bytes)", len);
    framebufferMmap.store(ptr, std::memory_order_relaxed);
    framebufferMmapSize.store(len, std::memory_order_relaxed);
    return { ptr, len };
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

bool
dump_qimage_buffer(void* qimage, std::string path)
{
    resolve_qimage_funcs();
    if (!qImageFuncs.ok) {
        errno = EBADFD;
        return false;
    }
    int height = qImageFuncs.height(qimage);
    int stride = qImageFuncs.bytesPerLine(qimage);
    size_t size = (size_t)stride * (size_t)height;

    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd <= 0) {
        return false;
    }
    return ::write(fd, qImageFuncs.bits(qimage), size) == size;
}

static std::atomic<bool> hook_installed = false;
static std::atomic<void*> epframebufferCandidate{ nullptr };
static std::atomic<void*> epframebufferInstance{ nullptr };
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
    // Check arm32 prologue for both swapBuffers variants:
    //
    // QRegion variant:
    //   stmdb sp!,{r4-r11,lr}   (0xE92D4FF0)
    //   sub sp, sp, #0x44       (0xE24DD044)
    //
    // QRect variant:
    //   cmp r1, #1              (0xE3510001)
    //   stmdb sp!,{r4-r8,lr}    (0xE92D41F0)
    //   mov r4, r1
    //   sub sp, sp, #0x30       (0xE24DD030)
    //
    // For QRegion: STMDB at instruction 0, SUB #0x44 at instruction 1.
    // For QRect:   CMP at instruction 0, STMDB at instruction 1,
    //              SUB #0x30 somewhere within the next 6 instructions.
    uint32_t* data = (uint32_t*)func;

    // Try the QRegion pattern: STMDB SP!,{...,LR} / SUB SP,#0x44
    if ((data[0] & 0xFFFF0000) == 0xE92D0000 && (data[0] & 0x4000)) {
        if (data[1] == 0xE24DD044) {
            return true;
        }
        _WARN(
            "swapBuffers: STMDB at pos 0 but second not sub sp,#0x44, "
            "got 0x%08x",
            data[1]
        );
        return false;
    }

    // Try the QRect pattern: CMP Rn,#imm / STMDB SP!,{...,LR} / ... / SUB
    // SP,#0x30 CMP with immediate encoding: bits 27-20 = 00110101 = 0x35 This
    // matches CMP (any register, any condition, any immediate).
    if ((data[0] & 0x0FF00000) == 0x03500000) {
        if ((data[1] & 0xFFFF0000) == 0xE92D0000 && (data[1] & 0x4000)) {
            // Find SUB SP, SP, #0x30 within the next 6 instructions
            for (int i = 2; i < 8; i++) {
                if (data[i] == 0xE24DD030) {
                    return true;
                }
            }
            _WARN(
                "swapBuffers: CMP+STMDB pattern but no sub sp,#0x30 found "
                "within 8 instructions"
            );
            return false;
        }
        _WARN(
            "swapBuffers: CMP at pos 0 but second not STMDB SP!,{...,LR}, "
            "got 0x%08x",
            data[1]
        );
        return false;
    }

    _WARN(
        "swapBuffers: first instruction 0x%08x doesn't match any known "
        "swapBuffers pattern",
        data[0]
    );
    return false;
#elif defined(__aarch64__)
    // TODO handle aarch64 version
#else
    _CRIT("%s", "Unsupported architecture");
    std::exit(EXIT_FAILURE);
#endif
    return true;
}

bool
copy_qimage_to_buffer(void* qimage, void* buffer, size_t size)
{
    resolve_qimage_funcs();
    if (!qImageFuncs.ok) {
        errno = EBADFD;
        return false;
    }
    if (size <= 0) {
        size = (size_t)qImageFuncs.bytesPerLine(qimage) *
               (size_t)qImageFuncs.height(qimage);
    }
    memcpy(buffer, qImageFuncs.bits(qimage), size);
    return true;
}

void
repaint(
    const Qt::QRectLayout* rect,
    Blight::WaveformMode waveform,
    Blight::UpdateMode updateMode
)
{
    _DEBUG(
        "repaint: (%d, %d) (%d, %d) %d %d",
        rect->left,
        rect->top,
        rect->right,
        rect->bottom,
        waveform,
        updateMode
    );
    if (!Client::HANDLE_FB) {
        Blight::exclusiveModeRepaint(
            rect->left,
            rect->top,
            rect->right - rect->left,
            rect->bottom - rect->top,
            waveform,
            updateMode
        );
        return;
    }
    FB::ensure_surface();
    auto maybe = FB::connection->repaint(
        FB::buffer,
        rect->left,
        rect->top,
        rect->right - rect->left,
        rect->bottom - rect->top,
        waveform,
        updateMode,
        0
    );
    if (maybe.has_value()) {
        maybe.value()->wait();
    }
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
    void* epframebuffer = epframebufferInstance.load(std::memory_order_acquire);
    void* auxBuffer = (char*)epframebuffer + auxBufferOffset;
    void* mainBuffer = (char*)epframebuffer + mainBufferOffset;
    copy_qimage_to_buffer(
        mainBuffer,
        Client::HANDLE_FB ? FB::buffer->data : mmap_framebuffer().first,
        0
    );
    repaint(&rect, waveform, updateMode);
    if (Client::DUMP_FB) {
        int fd = open("/tmp/fb.raw", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd <= 0) {
            _WARN("Failed to open /tmp/fb.raw: %s", std::strerror(errno));
            return;
        }
        ::write(
            fd,
            Client::HANDLE_FB ? FB::buffer->data : mmap_framebuffer().first,
            Client::HANDLE_FB ? (Client::deviceType == Client::DeviceType::RM2
                                     ? 26359808
                                     : FB::buffer->size())
                              : mmap_framebuffer().second
        );
        ::close(fd);
        dump_qimage_buffer(auxBuffer, "/tmp/auxBuffer.raw");
        dump_qimage_buffer(mainBuffer, "/tmp/mainBuffer.raw");
    }
}

/*!
 * \brief Hook to run on EPFramebuffer::swapBuffers(QRegion, ...) call
 */
void
hook_swapBuffers_QRegion(
    void* this_ptr,
    const void* region,
    const void* contentMap_screenModeMap,
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
    void* epframebuffer = epframebufferInstance.load(std::memory_order_acquire);
    void* auxBuffer = (char*)epframebuffer + auxBufferOffset;
    void* mainBuffer = (char*)epframebuffer + mainBufferOffset;
    copy_qimage_to_buffer(
        mainBuffer,
        Client::HANDLE_FB ? FB::buffer->data : mmap_framebuffer().first,
        0
    );
    // The third parameter (contentMap_screenModeMap) is a struct with 6
    // QRegions (offsets 0x00-0x14) and 6 corresponding screenMode ints (offsets
    // 0x18-0x2c). The original function uses indices [0] (pen-eligible), [1]
    // (mono), [2] (animation), each processed with its own screenMode-derived
    // waveform.  We iterate each non-empty sub-region directly using its
    // screenMode.
    bool hasContentMap = false;
    auto updateMode = Qt::flags_to_update_mode(flags);
    if (contentMap_screenModeMap) {
        auto* regions = (void* const*)contentMap_screenModeMap;
        auto* screenModes =
            (int const*)((char const*)contentMap_screenModeMap + 0x18);
        for (int i = 0; i < 3; i++) {
            if (regions[i] != nullptr) {
                hasContentMap = true;
                auto waveform = Qt::epsm_to_waveform(screenModes[i]);
                auto rit = begin_fn(&regions[i]);
                auto rend = end_fn(&regions[i]);
                for (; rit != rend; rit++) {
                    repaint(rit, waveform, updateMode);
                }
            }
        }
    }
    if (!hasContentMap) {
        for (; it != end; it++) {
            repaint(it, Blight::WaveformMode::HighQualityGrayscale, updateMode);
        }
    }
    if (Client::DUMP_FB) {
        int fd = open("/tmp/fb.raw", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd <= 0) {
            _WARN("Failed to open /tmp/fb.raw: %s", std::strerror(errno));
            return;
        }
        ::write(
            fd,
            Client::HANDLE_FB ? FB::buffer->data : mmap_framebuffer().first,
            Client::HANDLE_FB ? (Client::deviceType == Client::DeviceType::RM2
                                     ? 26359808
                                     : FB::buffer->size())
                              : mmap_framebuffer().second
        );
        ::close(fd);
        dump_qimage_buffer(auxBuffer, "/tmp/auxBuffer.raw");
        void* mainBuffer = (char*)epframebuffer + mainBufferOffset;
        dump_qimage_buffer(mainBuffer, "/tmp/mainBuffer.raw");
    }
}

/*!
 * \brief Hook to run on QObject::QObject(QObject*) call
 */
static void
hook_check_candidate()
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
    _DEBUG("EPFramebuffer candidate at %p, vtable=%p", candidate, vtable);
    if (vtable == 0) {
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
                "EPFramebuffer candidate %p failed %d times -- clearing "
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
            func_swapBuffers_qrect,
            candidate,
            vtable
        );
    }
    epframebufferInstance.store(candidate, std::memory_order_release);
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
    hook_check_candidate();
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
    hook_check_candidate();
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
    // Default ctor: format not yet set. Try both known EPFramebuffer offsets.
    // The vtable at (this - offset) is checked to confirm it belongs
    // to a xochitl QObject (fully dynamic via dladdr).
    for (int offset : { mainBufferOffset, auxBufferOffset }) {
        if (offset == 0) {
            continue; // skip unsupported arch placeholder
        }
        void* candidate = (char*)this_ptr - offset;
        void* vtable = *(void**)candidate;
        // Quick pre-filter: aligned, non-null, reasonable address
        if (vtable == nullptr || (uintptr_t)vtable & 3 ||
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
            "Default ctor at %p -> EPFramebuffer candidate at %p "
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
