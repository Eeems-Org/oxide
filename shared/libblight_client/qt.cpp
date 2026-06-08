#include "qt.h"
#include "fb.h"
#include "state.h"

#include <cstdint>
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
  qregion_constructor_t qregion_constructor() {
    static auto fn = reinterpret_cast<qregion_constructor_t>(
      dlsym(RTLD_DEFAULT, "_ZN7QRegionC1ERK5QRect")
    );
    return fn;
  }
  qregion_destructor_t qregion_destructor() {
    static auto fn = reinterpret_cast<qregion_destructor_t>(
      dlsym(RTLD_DEFAULT, "_ZN7QRegionD1Ev")
    );
    return fn;
  }
  qregion_begin_t qregion_begin() {
    static auto fn = reinterpret_cast<qregion_begin_t>(
      dlsym(RTLD_DEFAULT, "_ZNK7QRegion5beginEv")
    );
    return fn;
  }
  qregion_end_t qregion_end() {
    static auto fn = reinterpret_cast<qregion_end_t>(
      dlsym(RTLD_DEFAULT, "_ZNK7QRegion3endEv")
    );
    return fn;
  }
  bool region_rect_overlaps(const QRectLayout* rect, const void* region) {
    auto begin = qregion_begin();
    auto end = qregion_end();
    if (begin == nullptr || end == nullptr) {
      _CRIT("QRegion::begin or QRegion::end could not be resolved");
      std::_Exit(EXIT_FAILURE);
    }
    auto rit = begin(region);
    auto rend = end(region);
    if (rit == nullptr || rend == nullptr) {
      _DEBUG("region_rect_overlaps: qregion_begin/end is nullptr");
      return false;
    }
    for (; rit != rend; rit++) {
      if (
        rect->left < rit->right && rect->right > rit->left &&
        rect->top < rit->bottom && rect->bottom > rit->top
      ) {
        return true;
      }
    }
    return false;
  }
  Blight::WaveformMode epsm_to_waveform(int screenMode) {
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
  Blight::UpdateMode flags_to_update_mode(int flags) {
    return (flags & 1) ? Blight::UpdateMode::FullUpdate
                       : Blight::UpdateMode::PartialUpdate;
  }
}

// Runtime pointers to real QImage methods (resolved once via dlsym).
struct QImageFuncs {
#ifdef __arm__
  void (*constructor)(void*, char*, int, int, int, int, void*, void*);
  const unsigned char* (*constScanLine)(void*, int);
  int (*bytesPerLine)(void*);
#else
  void (*constructor)(void*, char*, int, int, long long, int, void*, void*);
  const unsigned char* (*constScanLine)(void*, int);
  long long (*bytesPerLine)(void*);
#endif
  void (*destructor)(void*);
  unsigned char* (*bits)(void*);
  int (*width)(void*);
  int (*height)(void*);
  int (*format)(void*);
  bool ok;
};

static QImageFuncs qImageFuncs;

/*!
 * \brief Get various QImage function pointers
 */
static void
resolve_qimage_funcs() {
  if (qImageFuncs.ok) {
    return;
  }
#if !defined(__arm__) && !defined(__aarch64__)
  _CRIT("%s", "Unsupported architecture");
  std::exit(EXIT_FAILURE);
#endif
  bool ok = true;
  qImageFuncs.constructor =
    reinterpret_cast<decltype(qImageFuncs.constructor)>(dlsym(
      RTLD_DEFAULT,
#ifdef __arm__
      "_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_"
#else
      "_ZN6QImageC1EPhiixNS_6FormatEPFvPvES2_"
#endif
    ));
  if (qImageFuncs.constructor == nullptr) {
    _WARN("%s", "Failed to resolve QImage::QImage");
    ok = false;
  }
  qImageFuncs.destructor = reinterpret_cast<decltype(qImageFuncs.destructor)>(
    dlsym(RTLD_DEFAULT, "_ZN6QImageD1Ev")
  );
  if (qImageFuncs.destructor == nullptr) {
    _WARN("%s", "Failed to resolve QImage::~QImage");
    ok = false;
  }
  qImageFuncs.bits = reinterpret_cast<decltype(qImageFuncs.bits)>(
    dlsym(RTLD_DEFAULT, "_ZNK6QImage4bitsEv")
  );
  if (qImageFuncs.bits == nullptr) {
    _WARN("%s", "Failed to resolve QImage::bits");
    ok = false;
  }
  qImageFuncs.constScanLine =
    reinterpret_cast<decltype(qImageFuncs.constScanLine)>(dlsym(
      RTLD_DEFAULT,
#ifdef __arm__
      "_ZNK6QImage13constScanLineEi"
#else
      "_ZNK6QImage8scanLineEi"
#endif
    ));
  if (qImageFuncs.constScanLine == nullptr) {
    _WARN("%s", "Failed to resolve QImage::constScanLine");
    ok = false;
  }
  qImageFuncs.width = reinterpret_cast<decltype(qImageFuncs.width)>(
    dlsym(RTLD_DEFAULT, "_ZNK6QImage5widthEv")
  );
  if (qImageFuncs.width == nullptr) {
    _WARN("%s", "Failed to resolve QImage::width");
    ok = false;
  }
  qImageFuncs.height = reinterpret_cast<decltype(qImageFuncs.height)>(
    dlsym(RTLD_DEFAULT, "_ZNK6QImage6heightEv")
  );
  if (qImageFuncs.height == nullptr) {
    _WARN("%s", "Failed to resolve QImage::height");
    ok = false;
  }
  qImageFuncs.bytesPerLine =
    reinterpret_cast<decltype(qImageFuncs.bytesPerLine)>(
      dlsym(RTLD_DEFAULT, "_ZNK6QImage12bytesPerLineEv")
    );
  if (qImageFuncs.bytesPerLine == nullptr) {
    _WARN("%s", "Failed to resolve QImage::bytesPerLine");
    ok = false;
  }
  qImageFuncs.format = reinterpret_cast<decltype(qImageFuncs.format)>(
    dlsym(RTLD_DEFAULT, "_ZNK6QImage6formatEv")
  );
  if (qImageFuncs.format == nullptr) {
    _WARN("%s", "Failed to resolve QImage::format");
    ok = false;
  }
  qImageFuncs.ok = ok;
}

static std::atomic<void*> framebufferMmap{nullptr};
static std::atomic<size_t> framebufferMmapSize{0};

/*!
 * \brief mmap the exclusive mode framebuffer
 */
static std::pair<void*, size_t>
mmap_framebuffer() {
  void* ptr = framebufferMmap.load(std::memory_order_acquire);
  if (ptr != nullptr) {
    return {ptr, framebufferMmapSize.load(std::memory_order_acquire)};
  }
  if (FB::frameBuffer < 0) {
    FB::frameBuffer = Blight::frameBuffer();
  }
  if (FB::frameBuffer < 0) {
    _WARN("%s", "redirect: Blight::frameBuffer() failed");
    return {nullptr, 0};
  }
  struct stat st;
  if (fstat(FB::frameBuffer, &st) < 0) {
    _WARN("%s", "redirect: fstat(FB::frameBuffer) failed");
    return {nullptr, 0};
  }
  auto len = static_cast<size_t>(st.st_size);
  ptr =
    mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, FB::frameBuffer, 0);
  auto err = errno;
  if (ptr == MAP_FAILED) {
    _WARN("redirect: mmap(FB::frameBuffer) failed: %s", std::strerror(err));
    return {nullptr, 0};
  }
  _DEBUG("redirect: mmap'd server framebuffer (%zu bytes)", len);
  framebufferMmap.store(ptr, std::memory_order_relaxed);
  framebufferMmapSize.store(len, std::memory_order_relaxed);
  return {ptr, len};
}

/*!
 * \brief Get the auxBuffer offset (used for EPFramebuffer detection)
 *
 * Both xochitl Tcon and libqsgepaper Tcon have a default-constructed QImage
 * at +0x60, so this offset works for detection on all platforms.
 */
int
auxBufferOffset() {
#if defined(__arm__)
  return 0x60;
#elif defined(__aarch64__)
  return 0xa8;
#else
  return 0x00;
#endif
}

/*!
 * \brief Get the shadowBuffer offset (used to read composed screen content)
 *
 * xochitl Tcon: shadow QImage at +0x50
 * libqsgepaper Tcon: shadow QImage at +0x70 (named shadowBuffer in header)
 *
 * Both default-construct the shadow QImage, so detection via {0x50,0x60}
 * works for xochitl and {0x60,0x70} works for libqsgepaper.
 */
int
shadowBufferOffset() {
#if defined(__arm__)
  return 0x50;
#elif defined(__aarch64__)
  if (Client::IS_XOCHITL) {
    return 0x88;
  }
  return 0xc8;
#else
  return 0x00;
#endif
}

/*!
 * \brief Get the vtable offset for EPFramebuffer::swapBuffers
 *
 * This is backendUpdate on aarch64
 */
int
swapBuffersOffset() {
  const char* offsetenv = getenv("OXIDE_EPFRAMEBUFFER_SWAPBUFFERS_OFFSET");
  if (offsetenv) {
    return std::stoi(offsetenv, nullptr, 0);
  } else {
#if defined(__arm__)
    return 0x5c; // vtable entry 23
#elif defined(__aarch64__)
    return 0xb8; // vtable entry 23
#else
    _CRIT("Unsupported architecture");
    std::exit(EXIT_FAILURE);
#endif
  }
}

/*!
 * \brief Get the vtable offset for EPFramebuffer::syncAfterUpdate
 */
int
syncAfterUpdateOffset() {
  const char* offsetenv = getenv("OXIDE_EPFRAMEBUFFER_SYNCAFTERUPDATE_OFFSET");
  if (offsetenv) {
    return std::stoi(offsetenv, nullptr, 0);
  } else {
#if defined(__arm__)
    return 0x60; // vtable entry 24
#elif defined(__aarch64__)
    return 0xc0; // vtable entry 24
#else
    _CRIT("Unsupported architecture");
    std::exit(EXIT_FAILURE);
#endif
  }
}

bool
dump_qimage_buffer(void* qimage, const std::string& path) {
  resolve_qimage_funcs();
  if (!qImageFuncs.ok) {
    errno = EBADFD;
    return false;
  }
  int height = qImageFuncs.height(qimage);
  int stride = qImageFuncs.bytesPerLine(qimage);
  ssize_t size = static_cast<ssize_t>(stride) * static_cast<ssize_t>(height);

  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd <= 0) {
    return false;
  }
  return ::write(fd, qImageFuncs.bits(qimage), size) == size;
}

static std::atomic<bool> hook_installed = false;
static std::atomic<void*> epframebufferCandidate{nullptr};
static std::atomic<void*> epframebufferInstance{nullptr};
static std::atomic<void*> lastBadCandidate{nullptr};
static std::atomic<void*> lastBadVtable{nullptr};
static std::atomic<int> badCandidateCount{0};

/*!
 * \brief Install a trampoline into a function to run our hook instead
 */
void
install_hook(void* target, void* hook) {
  auto ps = static_cast<long>(sysconf(_SC_PAGESIZE));
  auto page = reinterpret_cast<void*>(
    reinterpret_cast<uintptr_t>(target) & ~(static_cast<uintptr_t>(ps) - 1)
  );
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
  while (reinterpret_cast<uintptr_t>(page) + ps <
         reinterpret_cast<uintptr_t>(target) + len) {
    ps += sysconf(_SC_PAGESIZE);
  }
  mprotect(page, ps, PROT_READ | PROT_WRITE | PROT_EXEC);
#if defined(__arm__)
  // Overwrite the first 8 bytes of target with:
  //   LDR PC, [PC, #-4]   (loads from target+4 into PC)
  //   <hook address>
  // No separate trampoline needed -- LDR PC can reach any 32-bit address.
  *reinterpret_cast<uint32_t*>(target) = 0xe51ff004;
  *reinterpret_cast<uint32_t*>(static_cast<char*>(target) + 4) =
    reinterpret_cast<uintptr_t>(hook);
  __builtin___clear_cache(target, static_cast<char*>(target) + 8);
#elif defined(__aarch64__)
  // Overwrite the first 16 bytes of target with:
  //   LDR X16, #8          (load 8 bytes from PC+8 into X16)
  //   BR X16               (branch to X16)
  //   <hook address, 8 bytes>
  *reinterpret_cast<uint32_t*>(target) = 0x58000050;
  *reinterpret_cast<uint32_t*>(static_cast<char*>(target) + 4) = 0xd61f0200;
  *reinterpret_cast<uint64_t*>(static_cast<char*>(target) + 8) =
    reinterpret_cast<uintptr_t>(hook);
  __builtin___clear_cache(target, static_cast<char*>(target) + 16);
#endif
}

/*!
 * \brief Ensure that a pointer references a EPFramebuffer::swapBuffers(...)
 * method
 */
bool
validate_swapbuffers(void* func) {
  if (func == nullptr) {
    _WARN("swapBuffers: nullptr")
    return false;
  }
#if defined(__arm__)
  // Check arm32 prologue for the swapBuffers(QRegion) variant:
  //   stmdb sp!,{r4-r11,lr}   (0xE92D4FF0)
  //   sub sp, sp, #0x44       (0xE24DD044)
  auto data = reinterpret_cast<uint32_t*>(func);
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
  _WARN(
    "swapBuffers: first instruction 0x%08x doesn't match any known "
    "swapBuffers pattern",
    data[0]
  );
  return false;
#elif defined(__aarch64__)
  // Check for common aarch64 function prologue.
  // Most non-leaf functions start with:
  //   stp x29, x30, [sp, #-imm]!
  // Encoding: bits 31-24 = 0xA9, bits 23-22 = 10 (pre-index writeback).
  //
  // Some functions use Pointer Authentication (PAC), starting with:
  //   paciasp (0xD503233F)
  // followed by either:
  //   stp x29, x30, [sp, #-imm]!  (frame pointer)
  //   sub sp, sp, #imm             (no frame pointer)
  auto data = reinterpret_cast<uint32_t*>(func);
  if ((data[0] & 0xFFC00000) == 0xA9800000) {
    return true;
  }
  // PAC prologue: paciasp + stp or sub sp
  if (data[0] == 0xD503233F) {
    if ((data[1] & 0xFFC00000) == 0xA9800000) { // stp x29, x30,...
      return true;
    }
    if ((data[1] & 0xFF0003FF) == 0xD10003FF) { // sub sp, sp, #imm
      return true;
    }
  }
  _WARN("swapBuffers: PAC prologue not found")
  return false;
#else
  _CRIT("%s", "Unsupported architecture");
  std::exit(EXIT_FAILURE);
  return false;
#endif
}

// Runtime pointers to real QPainter methods (resolved once via dlsym).
struct QPainterFuncs {
  void (*constructor)(void*, void*);
  void (*destructor)(void*);
  void (*drawImage)(void*, const void*, const void*, const void*, int);
  bool ok;
};

static QPainterFuncs qPainterFuncs;

/*!
 * \brief Resolve QPainter function pointers via dlsym
 */
static void
resolve_qpainter_funcs() {
  if (qPainterFuncs.ok) {
    return;
  }
  bool ok = true;
  qPainterFuncs.constructor =
    reinterpret_cast<decltype(qPainterFuncs.constructor)>(
      dlsym(RTLD_DEFAULT, "_ZN8QPainterC1EP12QPaintDevice")
    );
  if (qPainterFuncs.constructor == nullptr) {
    _WARN("%s", "Failed to resolve QPainter::QPainter");
    ok = false;
  }
  qPainterFuncs.destructor =
    reinterpret_cast<decltype(qPainterFuncs.destructor)>(
      dlsym(RTLD_DEFAULT, "_ZN8QPainterD1Ev")
    );
  if (qPainterFuncs.destructor == nullptr) {
    _WARN("%s", "Failed to resolve QPainter::~QPainter");
    ok = false;
  }
  qPainterFuncs.drawImage =
    reinterpret_cast<decltype(qPainterFuncs.drawImage)>(dlsym(
      RTLD_DEFAULT,
      "_ZN8QPainter9drawImageERK6QRectFRK6QImageS2_"
      "6QFlagsIN2Qt19ImageConversionFlagEE"
    ));
  if (qPainterFuncs.drawImage == nullptr) {
    _WARN("%s", "Failed to resolve QPainter::drawImage");
    ok = false;
  }
  qPainterFuncs.ok = ok;
}

/*!
 * \brief Copy a QImage's pixel data into a destination buffer,
 *        converting pixel format via QPainter if needed.
 *
 * \param src       Pointer to the source QImage object
 * \param dst_data  Pointer to the destination raw pixel buffer
 * \param dst_fmt   BlightImageFormat of the destination
 * \param dst_w     Width of the destination in pixels
 * \param dst_h     Height of the destination in pixels
 * \param dst_stride Bytes per line of the destination
 * \return true on success, false on error (sets errno)
 */
static bool
copy_qimage_with_format_conversion(
  void* src,
  void* dst_data,
  int dst_fmt,
  int dst_w,
  int dst_h,
  int dst_stride
) {
  if (src == nullptr || dst_data == nullptr) {
    errno = EFAULT;
    return false;
  }
  resolve_qimage_funcs();
  if (!qImageFuncs.ok) {
    errno = EBADFD;
    return false;
  }
  int src_fmt = qImageFuncs.format(src);
  int src_w = qImageFuncs.width(src);
  int src_h = qImageFuncs.height(src);
  int src_stride = qImageFuncs.bytesPerLine(src);
  auto* src_bits = qImageFuncs.bits(src);
  if (
    src_fmt == dst_fmt && src_w == dst_w && src_h == dst_h &&
    src_stride == dst_stride
  ) {
    memcpy(
      dst_data,
      src_bits,
      static_cast<size_t>(dst_stride) * static_cast<size_t>(dst_h)
    );
    return true;
  }
  if (src_fmt == dst_fmt && src_w == dst_w && src_h == dst_h) {
    auto row_bytes =
      static_cast<size_t>(dst_stride) < static_cast<size_t>(src_stride)
        ? static_cast<size_t>(dst_stride)
        : static_cast<size_t>(src_stride);
    for (int y = 0; y < dst_h; ++y) {
      memcpy(
        static_cast<char*>(dst_data) + y * dst_stride,
        src_bits + y * src_stride,
        row_bytes
      );
    }
    return true;
  }
  resolve_qpainter_funcs();
  if (!qPainterFuncs.ok) {
    _WARN("%s", "copy_qimage_with_format_conversion: QPainter not available");
    errno = ENOSYS;
    return false;
  }
#if defined(__aarch64__)
  alignas(16) char dst_qimage_buf[128] = {};
  alignas(16) char painter_buf[128] = {};
#elif defined(__arm__)
  alignas(8) char dst_qimage_buf[128] = {};
  alignas(8) char painter_buf[128] = {};
#else
  char dst_qimage_buf[128] = {};
  char painter_buf[128] = {};
#endif
  qImageFuncs.constructor(
    dst_qimage_buf,
    static_cast<char*>(dst_data),
    dst_w,
    dst_h,
    dst_stride,
    dst_fmt,
    nullptr,
    nullptr
  );
  qPainterFuncs.constructor(painter_buf, dst_qimage_buf);
  Qt::QRectFLayout dst_rect = {
    0.0, 0.0, static_cast<double>(dst_w), static_cast<double>(dst_h)
  };
  Qt::QRectFLayout src_rect = {
    0.0, 0.0, static_cast<double>(src_w), static_cast<double>(src_h)
  };
  qPainterFuncs.drawImage(painter_buf, &dst_rect, src, &src_rect, 0);
  qPainterFuncs.destructor(painter_buf);
  qImageFuncs.destructor(dst_qimage_buf);
  return true;
}

/*!
 * \brief Repaint a region of the screen
 * \param rect Region to repaint
 * \param waveform Waveform to use for repainting
 * \param updateMode Update mode to use for repainting
 */
void
repaint(
  const Qt::QRectLayout* rect,
  Blight::WaveformMode waveform,
  Blight::UpdateMode updateMode,
  bool wait = false
) {
  _DEBUG(
    "repaint: (%d, %d) (%d, %d) %d %d",
    rect->left,
    rect->top,
    rect->right,
    rect->bottom,
    waveform,
    updateMode
  );
  FB::repaint(
    rect->left,
    rect->top,
    rect->right - rect->left,
    rect->bottom - rect->top,
    waveform,
    updateMode,
    0,
    wait
  );
}

/*!
 * \brief Dump the framebuffer, auxBuffer, and shadowBuffer to /tmp/*.raw
 */
void
dump_buffers() {
  if (!Client::isDumpFb()) {
    return;
  }
  void* epframebuffer = epframebufferInstance.load(std::memory_order_acquire);
  if (!epframebuffer) {
    _DEBUG("dump_buffers: epframebufferInstance not set yet, skipping");
    return;
  }
  void* data =
    Client::isFbEnabled() ? FB::buffer->data : mmap_framebuffer().first;
  if (data != nullptr) {
    int fd = open("/tmp/fb.raw", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd <= 0) {
      _WARN("Failed to open /tmp/fb.raw: %s", std::strerror(errno));
      return;
    }
    ::write(
      fd,
      data,
      Client::isFbEnabled()
        ? (Client::deviceType == Client::DeviceType::RM2 ? 26359808
                                                         : FB::buffer->size())
        : mmap_framebuffer().second
    );
    ::close(fd);
  }
  auto auxBuffer = static_cast<char*>(epframebuffer) + auxBufferOffset();
  auto shadowBuffer = static_cast<char*>(epframebuffer) + shadowBufferOffset();
  dump_qimage_buffer(auxBuffer, "/tmp/auxBuffer.raw");
  dump_qimage_buffer(shadowBuffer, "/tmp/shadowBuffer.raw");
}

/*!
 * \brief Hook to run on EPFramebuffer::swapBuffers(QRegion, ...) call
 *
 * on aarch64 this actually hooks EPFramebuffer::backendUpdate(QRegion, ...)
 * which is what swapBuffers calls to handle the screen.
 *
 * \param this_ptr EPFramebuffer instance
 * \param region QRegion instance defining what is being swapped
 * \param contentMap_screenModeMap information on what waveforms to use
 * \param flags Update flags
 */
void
hook_swapBuffers_QRegion(
  void* this_ptr,
  const void* region,
  const void* contentMap_screenModeMap,
  int flags
) {
  _DEBUG("EPFramebuffer::swapBuffers(QRegion, ...)");
  auto begin_fn = Qt::qregion_begin();
  auto end_fn = Qt::qregion_end();
  if (!begin_fn || !end_fn) {
    _DEBUG("hook: qregion_begin/end is NULL");
    return;
  }
  void* epframebuffer = epframebufferInstance.load(std::memory_order_acquire);
  if (!epframebuffer) {
    _DEBUG(
      "hook_swapBuffers_QRegion: epframebufferInstance not set yet,"
      " skipping"
    );
    return;
  }
  if (!copy_qimage_with_format_conversion(
        static_cast<char*>(epframebuffer) + shadowBufferOffset(),
        Client::isFbEnabled() ? FB::buffer->data : mmap_framebuffer().first,
        Client::isFbEnabled() ? FB::buffer->format : FB::deviceFormat(),
        Client::isFbEnabled() ? FB::buffer->width : FB::deviceXres(),
        Client::isFbEnabled() ? FB::buffer->height : FB::deviceYres(),
        Client::isFbEnabled() ? FB::buffer->stride : FB::deviceStride()
      )) {
    _WARN("Failed to convert image: %s", std::strerror(errno));
  }
  auto dit = begin_fn(region);
  auto dend = end_fn(region);
  if (dit && dend) {
    auto updateMode = Qt::flags_to_update_mode(flags);
    for (; dit != dend; dit++) {
      auto waveform = Blight::WaveformMode::HighQualityGrayscale;
      if (contentMap_screenModeMap) {
        auto regions = static_cast<void* const*>(contentMap_screenModeMap);
        if (
          regions[0] != nullptr && Qt::region_rect_overlaps(dit, &regions[0])
        ) {
          waveform = Blight::WaveformMode::Mono;
        } else if (
          regions[1] != nullptr && Qt::region_rect_overlaps(dit, &regions[1])
        ) {
          waveform = Blight::WaveformMode::Grayscale;
        } else if (
          regions[2] != nullptr && Qt::region_rect_overlaps(dit, &regions[2])
        ) {
          waveform = Blight::WaveformMode::Animate;
        } else if (
          regions[4] != nullptr && Qt::region_rect_overlaps(dit, &regions[4])
        ) {
          waveform = Blight::WaveformMode::HighQualityGrayscale;
        } else if (
          regions[5] != nullptr && Qt::region_rect_overlaps(dit, &regions[5])
        ) {
          waveform = Blight::WaveformMode::Grayscale;
        }
      }
      repaint(dit, waveform, updateMode);
    }
  }
  dump_buffers();
}

/*!
 * \brief No-op hook for EPFramebuffer::syncAfterUpdate()
 */
void
hook_syncAfterUpdate(void* this_ptr) {
  _ZN19EPFramebufferSwtcon4syncEv(this_ptr);
}

/*!
 * \brief Hook to run on  EPFramebufferSwtcon::update(QRect, int, PixelMode,
 * int) call
 *
 * All models but the rM1 use this function for triggering repaints with
 * libqsgepaper. Xochitl uses it's own internal method, which requires us to
 * hook swapBuffers instead.
 *
 * \param this_ptr   EPFramebufferSwtcon instance
 * \param rect       Rectangle to update
 * \param waveform   EPScreenMode value (maps through epsm_to_waveform)
 * \param pixelMode  PixelMode
 * \param flags      Update flags
 */
void
_ZN19EPFramebufferSwtcon6updateE5QRecti9PixelModei(
  void* this_ptr,
  Qt::QRectLayout rect,
  int waveform,
  int pixelMode,
  int flags
) {
  void* epframebuffer = nullptr;
  if (
    !epframebufferInstance.compare_exchange_strong(
      epframebuffer, this_ptr, std::memory_order_relaxed
    ) &&
    epframebuffer != this_ptr
  ) {
    _WARN(
      "epframebufferInstance does not match this_ptr: %p != %p",
      epframebuffer,
      this_ptr
    );
  }
  _DEBUG("EPFramebufferSwtcon::update(QRect, ...)");
  if (!copy_qimage_with_format_conversion(
        static_cast<char*>(this_ptr) + shadowBufferOffset(),
        Client::isFbEnabled() ? FB::buffer->data : mmap_framebuffer().first,
        Client::isFbEnabled() ? FB::buffer->format : FB::deviceFormat(),
        Client::isFbEnabled() ? FB::buffer->width : FB::deviceXres(),
        Client::isFbEnabled() ? FB::buffer->height : FB::deviceYres(),
        Client::isFbEnabled() ? FB::buffer->stride : FB::deviceStride()
      )) {
    _WARN("Failed to convert image: %s", std::strerror(errno));
  }
  repaint(
    &rect, Qt::epsm_to_waveform(waveform), Qt::flags_to_update_mode(flags)
  );
  dump_buffers();
}

void
_ZN19EPFramebufferSwtcon4syncEv(void* this_ptr) {
  void* epframebuffer = nullptr;
  if (
    !epframebufferInstance.compare_exchange_strong(
      epframebuffer, this_ptr, std::memory_order_relaxed
    ) &&
    epframebuffer != this_ptr
  ) {
    _WARN(
      "epframebufferInstance does not match this_ptr: %p != %p",
      epframebuffer,
      this_ptr
    );
  }
  _DEBUG("EPFramebufferSwtcon::sync()");
  if (!Client::isFbEnabled()) {
    Blight::waitForNoRepaints();
  }
}

/*!
 * \brief Hook to run on QObject::QObject(QObject*) call
 */
static void
hook_check_candidate() {
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
  auto vtable = *static_cast<void**>(candidate);
  _DEBUG("EPFramebuffer candidate at %p, vtable=%p", candidate, vtable);
  if (vtable == 0) {
    _DEBUG("%s", "vtable is null, skipping");
    return;
  }
  auto func_swapBuffers_qregion =
    *reinterpret_cast<void**>(static_cast<char*>(vtable) + swapBuffersOffset());
  _DEBUG("swapBuffers candidate function at %p", func_swapBuffers_qregion);
  if (!validate_swapbuffers(func_swapBuffers_qregion)) {
    _DEBUG(
      "validate_swapbuffers(%p) FAILED for QRegion candidate %p, "
      "vtable=%p",
      func_swapBuffers_qregion,
      candidate,
      vtable
    );
    // When the candidate address or its vtable changes, reset the
    // failure count. This handles multi-stage vtable initialization
    // (e.g. intermediate -> base -> Acep2) where early stages have
    // __cxa_pure_virtual at the swapBuffers slot.
    void* lastCandidate = lastBadCandidate.load(std::memory_order_relaxed);
    void* lastVtable = lastBadVtable.load(std::memory_order_relaxed);
    if (candidate != lastCandidate || vtable != lastVtable) {
      _WARN(
        "EPFramebuffer candidate %p or vtable %p changed", candidate, vtable
      );
      lastBadCandidate.store(candidate, std::memory_order_relaxed);
      lastBadVtable.store(vtable, std::memory_order_relaxed);
      badCandidateCount.store(1, std::memory_order_relaxed);
      return;
    }
    int n = badCandidateCount.fetch_add(1, std::memory_order_relaxed) + 1;
    _WARN("EPFramebuffer candidate check count %d", n);
    if (n >= 10) {
      _WARN(
        "EPFramebuffer candidate %p failed %d times with same vtable "
        "-- clearing (likely wrong object)",
        candidate,
        n
      );
      epframebufferCandidate.store(nullptr, std::memory_order_release);
      lastBadCandidate.store(nullptr, std::memory_order_relaxed);
      lastBadVtable.store(nullptr, std::memory_order_relaxed);
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
  if (Client::IS_XOCHITL) {
    install_hook(
      func_swapBuffers_qregion,
      reinterpret_cast<void*>(&hook_swapBuffers_QRegion)
    );
    _DEBUG("Hooked swapBuffers(QRegion,...)");
    auto func_syncAfterUpdate = *reinterpret_cast<void**>(
      static_cast<char*>(vtable) + syncAfterUpdateOffset()
    );
    if (func_syncAfterUpdate != nullptr) {
      install_hook(
        func_syncAfterUpdate, reinterpret_cast<void*>(&hook_syncAfterUpdate)
      );
      _DEBUG("Hooked syncAfterUpdate at %p", func_syncAfterUpdate);
    } else {
      _WARN("syncAfterUpdate vtable entry is null, cannot hook");
    }
  }
  void* empty = nullptr;
  epframebufferInstance.compare_exchange_strong(
    empty, candidate, std::memory_order_relaxed
  );
  hook_installed = true;
  epframebufferCandidate.store(nullptr, std::memory_order_release);
  lastBadCandidate.store(nullptr, std::memory_order_relaxed);
  lastBadVtable.store(nullptr, std::memory_order_relaxed);
  badCandidateCount.store(0, std::memory_order_relaxed);
}

/*!
 * \brief QObject::QObject(QObject*) constructor
 * \param this_ptr QObject instance being constructed
 * \param parent parent QObject instance
 */
void
_ZN7QObjectC2EPS_(void* this_ptr, void* parent) {
  static auto func = reinterpret_cast<void (*)(void*, void*)>(
    dlsym(RTLD_NEXT, "_ZN7QObjectC2EPS_")
  );
  if (!func) {
    _CRIT(
      "%s",
      "Failed to resolve real QObject::QObject(QObject*) [C2] via "
      "RTLD_NEXT"
    );
    std::exit(EXIT_FAILURE);
  }
  func(this_ptr, parent);
  hook_check_candidate();
}

/*!
 * \brief QObject::QObject(QObject*) constructor
 * \param this_ptr QObject instance being constructed
 * \param parent parent QObject instance
 */
void
_ZN7QObjectC1EPS_(void* this_ptr, void* parent) {
  static auto func = reinterpret_cast<void (*)(void*, void*)>(
    dlsym(RTLD_NEXT, "_ZN7QObjectC1EPS_")
  );
  if (!func) {
    _CRIT(
      "%s",
      "Failed to resolve real QObject::QObject(QObject*) [C1] via "
      "RTLD_NEXT"
    );
    std::exit(EXIT_FAILURE);
  }
  func(this_ptr, parent);
  hook_check_candidate();
}

/*!
 * \brief Alternative QImage::QImage() constructor
 * \param this_ptr QImage instance being constructed
 */
void
_ZN6QImageC1Ev(void* this_ptr) {
  static auto func =
    reinterpret_cast<void (*)(void*)>(dlsym(RTLD_NEXT, "_ZN6QImageC1Ev"));
  if (!func) {
    _CRIT("%s", "Failed to resolve QImage::QImage() via RTLD_NEXT");
    std::exit(EXIT_FAILURE);
  }
  func(this_ptr);
  if (hook_installed || Client::deviceType == Client::DeviceType::RM1) {
    return;
  }
  _DEBUG("QImage::QImage() default");
  for (int offset : {shadowBufferOffset(), auxBufferOffset()}) {
    if (offset == 0) {
      continue; // skip unsupported arch placeholder
    }
    void* candidate = static_cast<char*>(this_ptr) - offset;
    auto vtable = *static_cast<void**>(candidate);
    // Quick pre-filter: aligned, non-null, reasonable address
    if (
      vtable == nullptr || reinterpret_cast<uintptr_t>(vtable) & 3 ||
      reinterpret_cast<uintptr_t>(vtable) < 0x00010000
    ) {
      continue;
    }
    Dl_info info;
    if (
      !dladdr(vtable, &info) || info.dli_fname == nullptr || !*info.dli_fname
    ) {
      continue;
    }
    if (!Client::IS_XOCHITL) {
      char resolved[4096];
      if (realpath(info.dli_fname, resolved) == nullptr) {
        continue;
      }
      bool match =
        strcmp(resolved, "/usr/lib/plugins/scenegraph/libqsgepaper.so") == 0;
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
void*
__SHADOW_BUFFER() {
  void* epframebuffer = epframebufferInstance.load(std::memory_order_acquire);
  if (epframebuffer == nullptr) {
    return nullptr;
  }
  return static_cast<char*>(epframebuffer) + shadowBufferOffset();
}
