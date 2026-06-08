#pragma once
// This file is missing from the SDK.
// It's a recreation based on the disassembly of the library.
#include <QFlags>
#include <QImage>
#include <QMetaObject>
#include <QRect>
#include <QRegion>
#include <QSize>

#include <dlfcn.h>
#include <functional>
#include <tuple>

// libqsgepaper EPFramebufferTcon layout (returned by instance()):
//   auxBuffer at +0x60, frameBuffer pointer at +0x6c, shadowBuffer at +0x70,
//   total size 0xb8
#if defined(__arm__)
#define EPFR_SIZE 0xb8
#define EPFR_OFFSET_SHADOWBUFFER 0x70
#define EPFR_OFFSET_AUXBUFFER 0x60
#define EPFR_OFFSET_FRAMEBUFFER 0x6c
#elif defined(__aarch64__)
#define EPFR_SIZE 0x110
#define EPFR_OFFSET_SHADOWBUFFER 0xc8
#define EPFR_OFFSET_AUXBUFFER 0xa8
#define EPFR_OFFSET_FRAMEBUFFER 0xc0
#else
#error "Unsupported architecture"
#endif

/*! Display modes for the framebuffer controller.
    Each value maps to a hardware waveform slot that controls the
    speed-vs-quality trade-off for a region of the screen. */
enum EPScreenMode {
  Pen = 0,       //!< Pen/handwriting overlay (fast, low-artifact)
  Mono = 1,      //!< Fast monochrome text update
  Animate = 2,   //!< Animation-optimized update (no ghost accumulation)
  Grayscale = 3, //!< High quality grayscale update
  Content = 4,   //!< General content (text, images, line art)
  Full = 5,      //!< Full-screen refresh / persistent UI
};

/*! Content type hint for the framebuffer controller. */
enum EPContentType {
  Monochrome = 0, //!< Monochrome content
  Color = 1,      //!< Color content
};

/*! Pixel update mode for EPFramebufferSwtcon::update.*/
enum PixelMode {
  DefaultMode = 7, //!< Default pixel update mode
#if defined(__arm__)
  ClearMode = 6, //!< Residual/cleanup pass for post-animation rest areas
  PenMode = 8,   //!< Pen/handwriting overlay update
  GrayMode = 12, //!< Grayscale pixel update
#elif defined(__aarch64__)
  Acep2PenMode = 13,     //!< Pen/handwriting update
  Acep2PenModeFast = 14, //!< Alternative pen update
#endif
  MonoMode = 15, //!< Monochrome pixel update
  InitMode = 17, //!< Full initialization update
};

/*!
 * \brief Maps content type hints to screen regions.
 *
 * Tracks which areas of the screen contain each content type. The map
 * stores four internal QRegion slots:
 *   0 = Monochrome content (EPContentType::Monochrome)
 *   1 = Color content (EPContentType::Color)
 *   2 = Reserved (unused)
 *   3 = Unclassified (full screen at construction)
 *
 * When setTypeForRect() is called, the rect is added to the slot
 * corresponding to the content type and subtracted from all other slots,
 * so every pixel belongs to exactly one slot at all times.
 *
 * Used together with EPScreenModeMap to submit a classified display
 * update via EPFramebuffer::swapBuffers().
 */
class EPContentMap {
public:
  /*!
   * \brief Construct a content map covering the full screen.
   * \param size  Pixel dimensions of the display.
   *
   * All pixels are initially assigned to the unclassified slot.
   * Call setTypeForRect() to classify regions as Monochrome or Color.
   */
  EPContentMap(QSize size);

  /*!
   * \brief Mark a rectangle as containing a specific content type.
   * \param rect  Rectangle on screen.
   * \param type  Content type.
   *
   * The rect is added to the slot for a type and subtracted from the
   * other three slots. This is the only way to populate the map; there
   * is no remove/clear.
   */
  void setTypeForRect(QRect rect, EPContentType type);

  /*! \brief Write debug dump to qDebug(). */
  void dump() const;

  static QMetaObject staticMetaObject;
};

/*!
 * \brief Maps EPScreenMode values to screen regions.
 *
 * Associates display modes with the regions of the screen that should be
 * updated using each mode. A translation table remaps EPScreenMode values into
 * internal slots, allowing the system integrator to reassign which mode
 * controls which hardware waveform path.
 *
 * The constructor takes an optional vector of Translation entries that
 * override the default identity mapping (mode N -> slot N).  After building
 * the table the full screen is assigned to the slot indicated by uiScreenMode.
 */
class EPScreenModeMap {
public:
  /*!
   * \brief A single mode-to-slot remapping.
   */
  struct Translation {
    EPScreenMode mode; //!< Screen mode to remap (lookup key into translation table).
    int slot;          //!< Target slot index (0-5) that receives the region.
  };

  /*!
   * \brief Construct a screen-mode map.
   * \param size          Pixel dimensions of the display.
   * \param translations  Zero or more Translation entries that
   *                      override default mode-to-slot mappings.
   *
   * The default mapping is identity (each EPScreenMode maps to its own slot).
   * After building the translation table the entire screen is assigned to a
   * uiScreenMode.
   *
   * The liquid-crystal platform string (e.g. "EPFB_UI_SCREEN_MODE") controls
   * which mode is used for the default UI region at construction time.
   */
  EPScreenModeMap(QSize size, std::vector<Translation>&& translations);

  /*!
   * \brief Assign a region to a screen mode.
   * \param region  Screen area to classify.
   * \param mode    Display mode for this area.
   *
   * The region is added to the (possibly translated) slot for a mode and
   * subtracted from all other slots.
   */
  void setModeForRegion(const QRegion& region, EPScreenMode mode);

  /*!
   * \brief Return the region currently assigned to a mode.
   * \param mode  EPScreenMode to query.
   * \return      Region of the screen using this mode (may be empty).
   *
   * The returned QRegion is a handle into the map and becomes invalid if the
   * map is modified.
   */
  QRegion region(EPScreenMode mode) const;

  /*!
   * \brief Check whether only one mode covers the full screen.
   * \param mode  EPScreenMode to test for.
   * \return      true if the entire screen uses a mode and no other mode has
   *              any region assigned.
   */
  bool isOnly(EPScreenMode mode) const;

  /*! \brief Write debug dump to qDebug(). */
  void dump() const;

  /*! \brief Default screen mode used for unclassified UI regions. */
  static const EPScreenMode uiScreenMode;
  static QMetaObject staticMetaObject;
};

/*!
 * \brief Base class for the reMarkable framebuffer abstraction.
 *
 * EPFramebuffer manages a triple of QImages (auxiliary,
 * render target, and shadow copy) and provides swapBuffers() to submit display
 * updates. The library's instance() returns the platform-specific subclass
 * (EPFramebufferFusion).
 */
class EPFramebuffer {
public:
  static QMetaObject staticMetaObject;
  /*! Flags that modify update behaviour. */
  enum UpdateFlag {
    NoRefresh = 0,       //!< Partial update
    CompleteRefresh = 1, //!< Full refresh
  };

  /*!
   * \brief Ghost control modes for the display.
   *
   * These modes control how the framebuffer handles ghost-cleanup and
   * deferred-update behaviour.
   */
  enum GhostControlMode {
    BlinkNow = 0,     //!< Show pending content immediately.
    BlinkLater = 1,   //!< Hide content and defer ghost removal.
    BleachNow = 2,    //!< Acep2-only: run a bleach/animation pass.
    FactoryReset = 3, //!< Flash/clear to white (factory-reset blink on Acep2).
  };

  /*!
   * \brief Show, hide or clear ghosted content on the display.
   *
   * \param mode  The ghost-control action to perform.
   */
  void ghostControl(GhostControlMode mode);

  /*!
   * \brief Store the front and back buffer QImages.
   *
   * Called once during initialization. `param_1` is a tuple of
   * (shadowBuffer, auxBuffer); `frameBuffer` is an optional pointer to a
   * third QImage used as the render-target buffer for display output.
   */
  void setBuffers(std::tuple<QImage, QImage>, QImage* frameBuffer = nullptr);

  /*!
   * \brief Submit a display update for a region.
   *
   * This writes the content of frameBuffer to auxBuffer before calling update()
   * or sendTConUpdate()
   *
   * \param region  Rectangle to update
   * \param epct    Content type hint
   * \param type    Waveform mode.
   * \param flags   Update flags.
   */
  void swapBuffers(
    QRect region,
    EPContentType epct,
    EPScreenMode type,
    QFlags<EPFramebuffer::UpdateFlag> flags
  );

  /*!
   * \brief Submit a classified display update using content and mode maps.
   *
   * Unlike the simpler swapBuffers(QRect, EPContentType, EPScreenMode, ...)
   * overload, this variant accepts per-pixel classification so the
   * backend can apply different waveforms to different regions of the
   * screen in a single update transaction.
   *
   * The implementation copies frameBuffer to auxBuffer, then builds
   * an internal TCon command from the content and mode maps, and
   * finally submits the update to the display hardware.
   *
   * \param region         Bounding rectangle of the dirty area
   * \param contentMap     Per-rectangle content classification. Dictates which
   *                       hardware waveform slots are used for different
   *                       content types within the update region.
   * \param screenModeMap  Per-region screen mode assignment. Controls the
   *                       quality-vs-speed trade-off for each part of the
   *                       display.
   * \param flags          Update flags.
   */
  void swapBuffers(
    const QRegion& region,
    const EPContentMap& contentMap,
    const EPScreenModeMap& screenModeMap,
    QFlags<UpdateFlag> flags
  );

  /*!
   * \brief Try to acquire the framebuffer singleton lock.
   * \return  If the lock was acquired.
   *
   * Creates (or replaces) a QLockFile at /tmp/epframebuffer.lock and
   * attempts to lock it. On failure a critical message is logged.
   */
  bool checkLockFile();

  /*!
   * \brief Return the global EPFramebuffer singleton.
   *
   * The returned pointer is to the platform-specific subclass
   * (EPFramebufferFusion) which reflects the actual
   * libqsgepaper EPFramebufferTcon layout.
   */
  static class EPFramebufferFusion* instance();

  /*!
   * \brief Wait for pending updates to complete.
   *
   * Dispatches to the platform-specific sync at runtime using dlsym
   * to avoid a link-time dependency on the library class name.
   *
   * - ARM32 (rM1/rM2): tries EPFramebufferSwtcon::sync() first,
   *   then falls back to EPFramebufferTcon::sync() for older units.
   * - aarch64 (rM3+):   only EPFramebufferSwtcon::sync() exists.
   */
  inline void sync() {
    static auto* fn = []() -> void (*)(void*) {
      void* sync = dlsym(RTLD_DEFAULT, "_ZN19EPFramebufferSwtcon4syncEv");
      if (sync == nullptr) {
        sync = dlsym(RTLD_DEFAULT, "_ZN19EPFramebufferTcon4syncEv");
      }
      return reinterpret_cast<void (*)(void*)>(sync);
    }();
    if (fn == nullptr) {
      qFatal("Unable to find EPFramebuffer::sync()");
    }
    fn(this);
  }

  /*!
   * \brief Send screen update
   *
   * This assumes that framebuffer content has been written to auxBuffer.
   * This also requires beginUpdateQueue() calls before and endUpdateQueue()
   * calls after all updates are ready
   *
   * \param rect      Rectangle to update in pixels.
   * \param waveform  EPScreenMode value.  sendTConUpdate uses raw codes: 0x18
   *                  for GC16 0x1000 for DU.
   * \param pixelMode  Pixel update mode.
   */
  inline void update(
    QRect rect,
    int waveform,
    PixelMode pixelMode,
    QFlags<EPFramebuffer::UpdateFlag> flags
  ) {
    static const auto fn = []()
      -> std::function<
        void(void*, QRect, int, PixelMode, QFlags<EPFramebuffer::UpdateFlag>)> {
      auto sendTConUpdate = reinterpret_cast<
        void (*)(void*, const QRect&, int, QFlags<EPFramebuffer::UpdateFlag>)>(
        dlsym(
          RTLD_DEFAULT,
          "_ZN17EPFramebufferTcon14sendTConUpdateERK5QRecti6QFlagsIN13EPFramebu"
          "ffer10UpdateFlagEE"
        )
      );
      if (sendTConUpdate != nullptr) {
        return [sendTConUpdate](
                 void* this_ptr,
                 QRect rect,
                 int waveform,
                 PixelMode pixelMode,
                 QFlags<EPFramebuffer::UpdateFlag> flags
               ) { sendTConUpdate(this_ptr, rect, waveform, flags); };
      }
      return reinterpret_cast<void (*)(
        void*, QRect, int, PixelMode, QFlags<EPFramebuffer::UpdateFlag>
      )>(
        dlsym(
          RTLD_DEFAULT, "_ZN19EPFramebufferSwtcon6updateE5QRecti9PixelModei"
        )
      );
    }();
    if (fn == nullptr) {
      qFatal(
        "Unable to find EPFramebuffer::update() or "
        "EPFramebuffer::sendTConUpdate()"
      );
    }
    fn(this, rect, waveform, pixelMode, flags);
  }

  /*!
   * \brief Perform crash-recovery cleanup.
   *
   * Calls a platform-specific handler that may, for example, reset the
   * display controller or release the lock file.
   */
  void handleCrash();

signals:
  /*!
   * \brief Signal emitted when a framebuffer update has been submitted.
   *
   * \param rect  Bounding rectangle that was updated, in pixel coordinates.
   */
  void framebufferUpdated(const QRect& rect);
};

/*!
 * \brief Intermediate subclass (stand-in for the library's internal
 *        EPFramebufferSwtcon).
 *
 * Swtcon functions like initialize() are called during startup but
 * are not needed by consumers after instance() has returned.
 */
class EPFramebufferSwtcon : public EPFramebuffer {
public:
  /*! Open /dev/fb0 and set up the framebuffer controller. */
  void initialize(void);
};

/*!
 * \brief EPFramebuffer subclass matching the libqsgepaper
 *        EPFramebufferTcon binary layout.
 *
 * Layout:
 * \code
 *   +0x00  QObject / vtable
 *   +0x20  EPContentMap
 *   +0x30  EPScreenModeMap
 *   +0x60  QImage auxBuffer          (compositing/screen target)
 *   +0x6c  QImage* frameBuffer       (render target, default = &auxBuffer)
 *   +0x70  QImage shadowBuffer       (shadow copy used by swap path)
 *   +0x7c  pending-full-update flag
 *   +0x80  reserved
 *   +0x84  update-marker counter
 *   +0x88  swtcon-field / constants
 *   +0x98  QRegion  (pending region)
 *   +0xa0  QFile    (/dev/fb0)
 *   +0xa8  QImage (TCon frame-buffer copy)
 *   +0xb8  end
 * \endcode
 */
class EPFramebufferFusion : public EPFramebufferSwtcon {
public:
  EPFramebufferFusion();

private:
  char OPAQUE_A[EPFR_OFFSET_AUXBUFFER];

public:
  /*! Auxiliary (back) buffer - the compositing target. */
  QImage auxBuffer;

private:
  char
    OPAQUE_B[EPFR_OFFSET_FRAMEBUFFER - EPFR_OFFSET_AUXBUFFER - sizeof(QImage)];

public:
  /*! Pointer to the QImage that serves as the render target. */
  QImage* frameBuffer;

private:
  char OPAQUE_C
    [EPFR_OFFSET_SHADOWBUFFER - EPFR_OFFSET_FRAMEBUFFER - sizeof(QImage*)];

public:
  /*! Shadow copy used by the internal swap path for mono conversions. */
  QImage shadowBuffer;

private:
  char OPAQUE_D[EPFR_SIZE - EPFR_OFFSET_SHADOWBUFFER - sizeof(QImage)];
};
