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
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace Qt {
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
}

bool
safe_read(void* addr, void* buffer, size_t size) {
  struct iovec local = {.iov_base = buffer, .iov_len = size};
  struct iovec remote = {.iov_base = addr, .iov_len = size};
  return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == size;
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

/*!
 * \brief Get the frameBuffer offset (used for EPFramebuffer detection)
 */
int
frameBufferOffset() {
#if defined(__arm__)
  return Client::IS_XOCHITL ? 0x50 : 0x60;
#elif defined(__aarch64__)
  return 0xa8;
#else
  return 0x00;
#endif
}

/*!
 * \brief Get the previousBuffer offset (used to read composed screen content)
 */
int
previousBufferOffset() {
#if defined(__arm__)
  return Client::IS_XOCHITL ? 0x60 : 0x70;
#elif defined(__aarch64__)
  return Client::IS_XOCHITL ? 0x88 : 0xc8;
#else
  return 0x00;
#endif
}

/*!
 * \brief Get the renderTarget offset for EPFramebuffer
 */
int
renderTargetOffset() {
#if defined(__arm__)
  return Client::IS_XOCHITL ? 0x5c : 0x6c;
#elif defined(__aarch64__)
  return Client::IS_XOCHITL ? 0xa0 : 0xc0;
#else
  return 0x00;
#endif
}

/*!
 * \brief Get the vtable offset for EPFramebuffer::swapBuffers
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

/*!
 * \brief Get the vtable offset for EPFramebuffer::qt_metacall
 */
int
qtMetacallOffset() {
  const char* offsetenv = getenv("OXIDE_EPFRAMEBUFFER_METACALLL_OFFSET");
  if (offsetenv) {
    return std::stoi(offsetenv, nullptr, 0);
  }
#if defined(__arm__)
  return 0x08; // vtable entry 2
#elif defined(__aarch64__)
  return 0x10; // vtable entry 2
#else
  _CRIT("Unsupported architecture");
  std::exit(EXIT_FAILURE);
#endif
}

/*!
 * \brief Get the vtable offset for EPFramebufferAcep2::ghostControl
 */
int
ghostControlOffset() {
  const char* offsetenv = getenv("OXIDE_EPFRAMEBUFFER_GHOSTCONTROL_OFFSET");
  if (offsetenv) {
    return std::stoi(offsetenv, nullptr, 0);
  }
#if defined(__arm__)
  return 0x58; // vtable entry 22
#elif defined(__aarch64__)
  return 0xb0; // vtable entry
#else
  _CRIT("Unsupported architecture");
  std::exit(EXIT_FAILURE);
#endif
}

void
hook_ghostControl(void* this_ptr, int mode) {
  _DEBUG("EPFramebuffer::ghostControl(%d)", mode);
  if (Client::isGhostControlEnabled()) {
    Blight::ghostControl((Blight::GhostControlMode)mode);
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
static std::mutex candidatesMutex;
static std::vector<void*> epframebufferCandidates;
static std::atomic<void*> epframebufferInstance{nullptr};

static void* originalQtMetacall = nullptr;

template<typename T>
T
cast_function(const char* name) {
  T func = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
  if (func == nullptr) {
    _CRIT("Failed to resolve %s via RTLD_NEXT", name);
    std::_Exit(EXIT_FAILURE);
  }
  return func;
}

void**
vtable_ptr(void* vtable, int offset) {
  void* value = nullptr;
  void* addr = static_cast<char*>(vtable) + offset;
  return safe_read(addr, &value, sizeof(value)) ? reinterpret_cast<void**>(addr)
                                                : nullptr;
}

struct QMetaMethod {
  const void* mobj; // const QMetaObject*
  const uint* data; // Data::d
};

struct alignas(8) QByteArray {
  void* d;
  const char* ptr;
  long long size;
};

void
printMetaMethodsAndExit(void* metaObject) {
  static auto QMetaObject_className =
    cast_function<const char* (*)(void*)>("_ZNK11QMetaObject9classNameEv");
  static auto QMetaObject_method =
    cast_function<QMetaMethod (*)(void*, int)>("_ZNK11QMetaObject6methodEi");
  static auto QMetaObject_methodOffset =
    cast_function<int (*)(void*)>("_ZNK11QMetaObject12methodOffsetEv");
  static auto QMetaObject_methodCount =
    cast_function<int (*)(void*)>("_ZNK11QMetaObject11methodCountEv");
  static auto QMetaMethod_methodSignature =
    cast_function<QByteArray (*)(QMetaMethod*)>(
      "_ZNK11QMetaMethod15methodSignatureEv"
    );
  static auto QByteArray_toStdString =
    cast_function<std::string (*)(QByteArray*)>(
      "_ZNK10QByteArray11toStdStringB5cxx11Ev"
    );
  _DEBUG(
    "Available metacall functions for %s:", QMetaObject_className(metaObject)
  );
  for (int i = QMetaObject_methodOffset(metaObject);
       i < QMetaObject_methodCount(metaObject);
       ++i) {
    QMetaMethod method = QMetaObject_method(metaObject, i);
    QByteArray signature = QMetaMethod_methodSignature(&method);
    _DEBUG("%d: %s", i, QByteArray_toStdString(&signature).c_str());
  }
  std::_Exit(EXIT_FAILURE);
}

void*
epframebufferMetaObject() {
  static void* metaObject = nullptr;
  if (metaObject != nullptr) {
    return metaObject;
  }
  void* epframebuffer = epframebufferInstance.load(std::memory_order_acquire);
  if (!epframebuffer) {
    _DEBUG("epframebuffer missing");
    std::_Exit(EXIT_FAILURE);
  }
  auto vtable = *reinterpret_cast<void***>(epframebuffer);
  if (vtable == nullptr) {
    _CRIT("%s", "Failed to get vtable");
    std::_Exit(EXIT_FAILURE);
  }
  metaObject = reinterpret_cast<void* (*)(void*)>(vtable[0])(epframebuffer);
  if (metaObject == nullptr) {
    _CRIT("%s", "Failed to get QMetaObject");
    std::_Exit(EXIT_FAILURE);
  }
  return metaObject;
}

/*!
 * \brief Replacement for EPFramebuffer::qt_metacall()
 */
int
hook_qt_metacall(void* this_ptr, int type, int index, void** argv) {
  _DEBUG("EPFramebuffer::qt_metacall(%d, %d, ...)", type, index);
  auto func =
    reinterpret_cast<int (*)(void*, int, int, void**)>(originalQtMetacall);
  if (func == nullptr) {
    _CRIT("Original qt_metacall is nullptr");
    std::_Exit(EXIT_FAILURE);
  }
  if (type == 0) { // InvokeMetaMethod
    static int scheduleGhostRemovalIndex = -2;
    void* metaObject = epframebufferMetaObject();
    if (scheduleGhostRemovalIndex == -2) {
      static auto QMetaObject_indexOfMethod =
        cast_function<int (*)(void*, const char*)>(
          "_ZNK11QMetaObject13indexOfMethodEPKc"
        );
      scheduleGhostRemovalIndex =
        QMetaObject_indexOfMethod(metaObject, "scheduleGhostRemoval()");
    }
    if (scheduleGhostRemovalIndex == -1) {
      auto QMetaObject_className =
        cast_function<const char* (*)(void*)>("_ZNK11QMetaObject9classNameEv");
      _CRIT(
        "Failed to get %s::scheduleGhostRemoval() index",
        QMetaObject_className(metaObject)
      );
      printMetaMethodsAndExit(metaObject);
    }
    static int clearGhostingIndex = -2;
    if (clearGhostingIndex == -2) {
      static auto QMetaObject_indexOfMethod =
        cast_function<int (*)(void*, const char*)>(
          "_ZNK11QMetaObject13indexOfMethodEPKc"
        );
      clearGhostingIndex =
        QMetaObject_indexOfMethod(metaObject, "clearGhosting()");
    }
    if (clearGhostingIndex == -1) {
      auto QMetaObject_className =
        cast_function<const char* (*)(void*)>("_ZNK11QMetaObject9classNameEv");
      _CRIT(
        "Failed to get %s::clearGhosting() index",
        QMetaObject_className(metaObject)
      );
      printMetaMethodsAndExit(metaObject);
    }
    if (index == scheduleGhostRemovalIndex) {
      int mode = *reinterpret_cast<int*>(argv[1]);
      _DEBUG("EPFramebuffer::scheduleGhostRemoval()");
      // Blight::ghostControl(Blight::GhostControlMode::BlinkLater);
      return -1;
    }
    if (index == clearGhostingIndex) {
      int mode = *reinterpret_cast<int*>(argv[1]);
      _DEBUG("EPFramebuffer::clearGhosting()");
      // Blight::ghostControl(Blight::GhostControlMode::BlinkNow);
      return -1;
    }
  }
  return func(this_ptr, type, index, argv);
}

/*!
 * \brief Install a trampoline into a function to run our hook instead
 */
void
install_hook(void* target, void* hook) {
  auto ps = static_cast<long>(sysconf(_SC_PAGESIZE));
  auto page = reinterpret_cast<void*>(
    reinterpret_cast<uintptr_t>(target) & ~(static_cast<uintptr_t>(ps) - 1)
  );
  size_t len;
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
    _WARN("swapBuffers: nullptr");
    return false;
  }
#if defined(__arm__)
  if (reinterpret_cast<uintptr_t>(func) & 2) {
    _WARN("swapBuffers: address %p is not 2-byte aligned", func);
    return false;
  }
#elif defined(__aarch64__)
  if (reinterpret_cast<uintptr_t>(func) & 3) {
    _WARN("swapBuffers: address %p is not 4-byte aligned", func);
    return false;
  }
#else
  _CRIT("%s", "Unsupported architecture");
  std::exit(EXIT_FAILURE);
  return false;
#endif
  Dl_info info;
  if (dladdr(func, &info) == 0) {
    _WARN("swapBuffers: address %p not in process address space", func);
    return false;
  }
  uint32_t data[2];
  if (!safe_read(func, &data, sizeof(data))) {
    _WARN("swapBuffers: address %p not readable", func);
    return false;
  }
#if defined(__arm__)
  // Check arm32 prologue for the swapBuffers(QRegion) variant:
  //   stmdb sp!,{r4-r11,lr}   (0xE92D4FF0)
  //   sub sp, sp, #0x44       (0xE24DD044)
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
  _WARN("swapBuffers: PAC prologue not found");
  return false;
#endif
}

/*!
 * \brief Runtime pointers to real QPainter methods (resolved once via dlsym).
 */
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
 * \brief Repaint a region of the screen
 * \param rect Region to repaint
 * \param waveform Waveform to use for repainting
 * \param updateMode Update mode to use for repainting
 */
void
repaint(
  const Qt::QRectLayout* rect,
  Blight::WaveformMode waveform,
  Blight::ContentType contentType,
  Blight::UpdateMode updateMode,
  bool wait = false
) {
  auto width = rect->right - rect->left + 1;
  auto height = rect->bottom - rect->top + 1;
  _DEBUG(
    "repaint: (%d, %d) %dx%d %d %d %d",
    rect->left,
    rect->top,
    width,
    height,
    waveform,
    contentType,
    updateMode
  );
  FB::repaint(
    rect->left,
    rect->top,
    width,
    height,
    waveform,
    contentType,
    updateMode,
    0,
    wait
  );
}

/*!
 * \brief Dump the framebuffer, frameBuffer, and previousBuffer to /tmp/*.raw
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
  if (FB::buffer->data != nullptr) {
    int fd = open("/tmp/fb.raw", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd <= 0) {
      _WARN("Failed to open /tmp/fb.raw: %s", std::strerror(errno));
      return;
    }
    ::write(
      fd,
      FB::buffer->data,
      Client::deviceType == Client::DeviceType::RM2 ? 26359808
                                                    : FB::buffer->size()
    );
    ::close(fd);
  }
  auto frameBuffer = static_cast<char*>(epframebuffer) + frameBufferOffset();
  auto previousBuffer =
    static_cast<char*>(epframebuffer) + previousBufferOffset();
  dump_qimage_buffer(frameBuffer, "/tmp/frameBuffer.raw");
  dump_qimage_buffer(previousBuffer, "/tmp/previousBuffer.raw");
}

void
update_buffers(const Qt::QRectLayout& rect) {
  resolve_qimage_funcs();
  if (!qImageFuncs.ok) {
    _WARN("%s", "qpainter_drawImage: QImage not available");
    return;
  }
  resolve_qpainter_funcs();
  if (!qPainterFuncs.ok) {
    _WARN("%s", "qpainter_drawImage: QPainter not available");
    return;
  }
  void* epframebuffer = epframebufferInstance.load(std::memory_order_acquire);
  if (!epframebuffer) {
    _DEBUG("update_buffers: epframebufferInstance not set yet, skipping");
    return;
  }
  auto* previousBuffer =
    static_cast<char*>(epframebuffer) + previousBufferOffset();
  if (previousBuffer == nullptr) {
    _DEBUG("update_buffers: previousBuffer not set yet, skipping");
    return;
  }
  auto* renderTarget = *reinterpret_cast<char**>(
    static_cast<char*>(epframebuffer) + renderTargetOffset()
  );
  if (renderTarget == nullptr) {
    _DEBUG("update_buffers: renderTarget not set yet, skipping");
    return;
  }
#if defined(__aarch64__)
  alignas(16) char qimage[128] = {};
#elif defined(__arm__)
  alignas(8) char qimage[128] = {};
#else
  char qimage[128] = {};
#endif
  qImageFuncs.constructor(
    qimage,
    reinterpret_cast<char*>(FB::buffer->data),
    FB::buffer->width,
    FB::buffer->height,
    FB::buffer->stride,
    FB::buffer->format,
    nullptr,
    nullptr
  );
#if defined(__aarch64__)
  alignas(16) char painter[128] = {};
#elif defined(__arm__)
  alignas(8) char painter[128] = {};
#else
  char painter[128] = {};
#endif
  qPainterFuncs.constructor(painter, qimage);
  Qt::QRectFLayout rectF{
    static_cast<double>(rect.left),
    static_cast<double>(rect.top),
    static_cast<double>(rect.right - rect.left + 1),
    static_cast<double>(rect.bottom - rect.top + 1)
  };
  qPainterFuncs.drawImage(painter, &rectF, renderTarget, &rectF, 0);
  qPainterFuncs.destructor(painter);
  qImageFuncs.destructor(qimage);
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
  if (begin_fn == nullptr || end_fn == nullptr) {
    _DEBUG("swapBuffers: qregion_begin/end is NULL");
    return;
  }
  auto dit = begin_fn(region);
  auto dend = end_fn(region);
  if (dit && dend) {
    auto updateMode = (Blight::UpdateMode)flags;
    for (; dit != dend; dit++) {
      if (contentMap_screenModeMap) {
        auto regions = static_cast<void* const*>(contentMap_screenModeMap);
        auto slot_remap = reinterpret_cast<const int32_t*>(regions + 6);
        for (int modeId = Blight::WaveformMode::UltraFast;
             modeId <= Blight::WaveformMode::Full;
             modeId++) {
          int slot = slot_remap[modeId];
          auto region = regions[slot];
          if (region == nullptr || !Qt::region_rect_overlaps(dit, &region)) {
            continue;
          }
          Blight::ContentType contentType;
          auto waveform = (Blight::WaveformMode)modeId;
          switch (waveform) {
            case Blight::WaveformMode::UltraFast:
            case Blight::WaveformMode::Fast:
            case Blight::WaveformMode::UI:
              contentType = Blight::ContentType::Monochrome;
              break;
            case Blight::WaveformMode::Animate:
            case Blight::WaveformMode::Content:
            case Blight::WaveformMode::Full:
              contentType = Blight::ContentType::Color;
              break;
          }
          update_buffers(*dit);
          repaint(dit, waveform, contentType, updateMode);
        }
      }
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
/*!
 * \brief Hook for EPFramebuffer::ghostControl(GhostControlMode)
 *
 * Called when the ePaper compositor requests ghost compensation actions.
 * Forwards to the original and logs the call.
 */
void
_ZN13EPFramebuffer12ghostControlENS_16GhostControlModeE(
  void* this_ptr,
  int mode
) {
  if (!Client::isFbEnabled()) {
    static auto func = reinterpret_cast<void (*)(void*, int)>(dlsym(
      RTLD_NEXT, "_ZN13EPFramebuffer12ghostControlENS_16GhostControlModeE"
    ));
    if (!func) {
      _CRIT(
        "%s", "Failed to resolve real EPFramebuffer::ghostControl via RTLD_NEXT"
      );
      std::exit(EXIT_FAILURE);
    }
    func(this_ptr, mode);
    return;
  }
  _DEBUG("EPFramebuffer::ghostControl(mode=%d)", mode);
  if (Client::isGhostControlEnabled()) {
    Blight::ghostControl((Blight::GhostControlMode)mode);
  }
}

#if defined(__aarch64__)
void
_ZN18EPFramebufferAcep212ghostControlEN13EPFramebuffer16GhostControlModeE(
  void* this_ptr,
  int mode
) {
  if (!Client::isFbEnabled()) {
    static auto func = reinterpret_cast<void (*)(void*, int)>(dlsym(
      RTLD_NEXT,
      "_ZN18EPFramebufferAcep212ghostControlEN13EPFramebuffer16GhostControlMode"
      "E"
    ));
    if (!func) {
      _CRIT(
        "%s",
        "Failed to resolve real EPFramebufferAcep2::ghostControl via RTLD_NEXT"
      );
      std::exit(EXIT_FAILURE);
    }
    func(this_ptr, mode);
    return;
  }
  _DEBUG("EPFramebufferAcep2::ghostControl(mode=%d)", mode);
  if (Client::isGhostControlEnabled()) {
    Blight::ghostControl((Blight::GhostControlMode)mode);
  }
}
#endif

void
_ZN19EPFramebufferSwtcon6updateE5QRecti9PixelModei(
  void* this_ptr,
  Qt::QRectLayout rect,
  int waveform,
  Blight::ContentType contentType,
  int flags
) {
  if (!Client::isFbEnabled()) {
    static auto func = reinterpret_cast<
      void (*)(void*, Qt::QRectLayout, int, Blight::ContentType, int)>(
      dlsym(RTLD_NEXT, "_ZN19EPFramebufferSwtcon6updateE5QRecti9PixelModei")
    );
    if (!func) {
      _CRIT(
        "%s", "Failed to resolve real EPFramebufferSwtcon::update via RTLD_NEXT"
      );
      std::exit(EXIT_FAILURE);
    }
    func(this_ptr, rect, waveform, contentType, flags);
    return;
  }
  _DEBUG("EPFramebufferSwtcon::update(QRect, ...)");
  update_buffers(rect);
  repaint(
    &rect,
    (Blight::WaveformMode)waveform,
    contentType,
    (Blight::UpdateMode)flags
  );
  dump_buffers();
}

void
_ZN19EPFramebufferSwtcon4syncEv(void* this_ptr) {
  if (!Client::isFbEnabled()) {
    static auto func = reinterpret_cast<void (*)(void*)>(
      dlsym(RTLD_NEXT, "_ZN19EPFramebufferSwtcon4syncEv")
    );
    if (!func) {
      _CRIT(
        "%s", "Failed to resolve real EPFramebufferSwtcon::sync via RTLD_NEXT"
      );
      std::exit(EXIT_FAILURE);
    }
    func(this_ptr);
    return;
  }
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
  Blight::waitForNoRepaints();
}

void
install_hook_ptr(void** target_ptr, void* hook) {
  auto ps = static_cast<long>(sysconf(_SC_PAGESIZE));
  auto page = reinterpret_cast<void*>(
    reinterpret_cast<uintptr_t>(target_ptr) & ~(static_cast<uintptr_t>(ps) - 1)
  );
  while (reinterpret_cast<uintptr_t>(page) + ps <
         reinterpret_cast<uintptr_t>(target_ptr) + sizeof(void*)) {
    ps += sysconf(_SC_PAGESIZE);
  }
  mprotect(page, ps, PROT_READ | PROT_WRITE);
  *target_ptr = hook;
}

/*!
 * \brief Hook to run on QObject::QObject(QObject*) call
 */
static void
hook_check_candidate() {
  if (
    !Client::isFbEnabled() || hook_installed ||
    Client::deviceType == Client::DeviceType::RM1
  ) {
    return;
  }
  std::lock_guard<std::mutex> lock(candidatesMutex);
  for (void* candidate : epframebufferCandidates) {
    if (candidate == nullptr) {
      continue;
    }
    auto vtable = *static_cast<void**>(candidate);
    _DEBUG("EPFramebuffer candidate at %p, vtable=%p", candidate, vtable);
    if (!vtable) {
      _DEBUG("%s", "vtable is null, skipping");
      continue;
    }
    void* func_swapBuffers_qregion = nullptr;
    if (!safe_read(
          static_cast<char*>(vtable) + swapBuffersOffset(),
          &func_swapBuffers_qregion,
          sizeof(func_swapBuffers_qregion)
        )) {
      _DEBUG(
        "EPFramebuffer candidate vtable offset invalid candidate %p, "
        "vtable=%p",
        candidate,
        vtable
      );
      continue;
    }
    _DEBUG("swapBuffers candidate function at %p", func_swapBuffers_qregion);
    if (!validate_swapbuffers(func_swapBuffers_qregion)) {
      _DEBUG(
        "validate_swapbuffers(%p) FAILED for QRegion candidate %p, vtable=%p",
        func_swapBuffers_qregion,
        candidate,
        vtable
      );
      continue;
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
      _DEBUG(
        "Hooked swapBuffers(QRegion,...) at %p with offset=0x%x",
        func_swapBuffers_qregion,
        swapBuffersOffset()
      );
      void** func_syncAfterUpdate = vtable_ptr(vtable, syncAfterUpdateOffset());
      if (func_syncAfterUpdate != nullptr && *func_syncAfterUpdate != nullptr) {
        install_hook(
          *func_syncAfterUpdate, reinterpret_cast<void*>(&hook_syncAfterUpdate)
        );
        _DEBUG(
          "Hooked syncAfterUpdate at %p with offset=0x%x",
          func_syncAfterUpdate,
          syncAfterUpdateOffset()
        );
      } else {
        _WARN("syncAfterUpdate vtable entry is null, cannot hook");
      }
      void** func_ghostControl = vtable_ptr(vtable, ghostControlOffset());
      if (func_ghostControl != nullptr && *func_ghostControl != nullptr) {
        install_hook_ptr(
          func_ghostControl, reinterpret_cast<void*>(hook_ghostControl)
        );
        _DEBUG(
          "Hooked ghostControl at %p with offset=0x%x",
          func_ghostControl,
          ghostControlOffset()
        );
      } else {
        _WARN("ghostControl vtable entry is null, skipping hook");
      }
      void** func_metacall = vtable_ptr(vtable, qtMetacallOffset());
      if (func_metacall != nullptr && *func_metacall != nullptr) {
        originalQtMetacall = *func_metacall;
        install_hook_ptr(
          func_metacall, reinterpret_cast<void*>(hook_qt_metacall)
        );
        _DEBUG(
          "Hooked qt_metacall at %p with offset=0x%x and original=%p",
          func_metacall,
          qtMetacallOffset(),
          originalQtMetacall
        );
      } else {
        _WARN("qt_metacall vtable entry is null, skipping hook");
      }
    }
    void* empty = nullptr;
    epframebufferInstance.compare_exchange_strong(
      empty, candidate, std::memory_order_relaxed
    );
    hook_installed = true;
    epframebufferCandidates.clear();
    return;
  }
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
  if (
    !Client::isFbEnabled() || hook_installed ||
    Client::deviceType == Client::DeviceType::RM1
  ) {
    return;
  }
  _DEBUG("QImage::QImage() default");
  for (int offset : {previousBufferOffset(), frameBufferOffset()}) {
    if (offset == 0x00) {
      continue; // skip unsupported arch placeholder
    }
    void* candidate = static_cast<char*>(this_ptr) - offset;
    auto vtable = *static_cast<void**>(candidate);
    // Quick pre-filter: aligned, non-null, reasonable address
    if (
      vtable == nullptr ||
      reinterpret_cast<uintptr_t>(vtable) & (sizeof(void*) - 1) ||
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
    std::lock_guard<std::mutex> lock(candidatesMutex);
    if (
      std::none_of(
        epframebufferCandidates.begin(),
        epframebufferCandidates.end(),
        [candidate](void* c) { return c == candidate; }
      )
    ) {
      epframebufferCandidates.push_back(candidate);
    }
  }
}
void*
__PREVIOUS_BUFFER() {
  void* epframebuffer = epframebufferInstance.load(std::memory_order_acquire);
  if (epframebuffer == nullptr) {
    return nullptr;
  }
  return static_cast<char*>(epframebuffer) + previousBufferOffset();
}
