#pragma once
// This file is missing from the SDK.
// It's a recreation based on the disassembly of the library.
#include <QFlags>
#include <QImage>
#include <QRect>
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

/*! Waveform modes used by the framebuffer controller. */
enum EPScreenMode {
  QualityFastest = 0, //!< Monochrome, no dithering
  QualityFast = 1,    //!< Fast grayscale update
  Animate = 2,        //!< Animated content
  Quality3 = 3,       //!< Medium quality
  QualityFull = 4,    //!< Full quality grayscale
  Quality5 = 5,       //!< Alternative full quality
};

/*! Content type hint for the framebuffer controller. */
enum EPContentType {
  Mono = 0,  //!< Monochrome content
  Color = 1, //!< Color (16-level grayscale) content
};
/*! Pixel update mode for EPFramebufferSwtcon::update.
    Values observed from disassembly of the reMarkable libraries. */
enum PixelMode {
  DefaultMode = 7, //!< Default pixel update mode (used for most content)
  GrayMode = 12,   //!< Grayscale pixel update (0xc, second pass in fusion)
  MonoMode = 15,   //!< Monochrome pixel update (0xf, used in Acep2 STD regions)
  InitMode = 17,   //!< Full initialization update (0x11, waveform init)
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
  /*! Flags that modify update behaviour. */
  enum UpdateFlag {
    NoRefresh = 0,       //!< Partial update
    CompleteRefresh = 1, //!< Full refresh
  };

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
   * Triggers the framebuffer controller to redraw `region` on the
   * e-ink display using the given waveform and content type.
   *
   * \param region  Rectangle to update (in pixels).
   * \param epct    Content type hint (mono or color).
   * \param type    Waveform mode (quality vs speed trade-off).
   * \param flags   Update flags (partial vs full refresh).
   */
  void swapBuffers(
    QRect region,
    EPContentType epct,
    EPScreenMode type,
    QFlags<EPFramebuffer::UpdateFlag> flags
  );

  /*!
   * \brief Return the global EPFramebuffer singleton.
   *
   * The returned pointer is to the platform-specific subclass
   * (EPFramebufferFusion) which reflects the actual
   * libqsgepaper EPFramebufferTcon layout.
   */
  static class EPFramebufferFusion* instance();

#ifdef __arm__
  /*!
   * \brief Wait for pending updates to complete.
   *
   * Dispatches to the platform-specific sync at runtime:
   * - EPFramebufferSwtcon::sync() on rM2
   * - EPFramebufferTcon::sync() on rM1
   *
   * Using dlsym avoids a link-time dependency on a particular class
   * name, letting the same binary work across device generations.
   */
  inline void sync() {
    static auto* fn = []() -> void (*)(void*) {
      void* s = dlsym(RTLD_DEFAULT, "_ZN19EPFramebufferSwtcon4syncEv");
      if (s == nullptr) {
        s = dlsym(RTLD_DEFAULT, "_ZN19EPFramebufferTcon4syncEv");
      }
      return reinterpret_cast<void (*)(void*)>(s);
    }();
    if (fn == nullptr) {
      qFatal("Unable to find EPFramebuffer::sync()");
    }
    fn(this);
  }
#else
  /*!
   * \brief Wait for pending updates to complete.
   */
  void sync();
#endif
};

/*!
 * \brief Intermediate subclass (stand-in for the library's internal
 *        EPFramebufferSwtcon).
 *
 * Swtcon functions like initialize() are called during startup but
 * are not needed by consumers after instance() has returned.
 * sync() lives on the base EPFramebuffer class with runtime dispatch.
 */
class EPFramebufferSwtcon : public EPFramebuffer {
public:
  /*! Open /dev/fb0 and set up the framebuffer controller. */
  void initialize(void);

#if defined(__arm__)
  /*!
   * \brief Send screen update
   *
   * \param rect      Rectangle to update in pixels.
   * \param waveform  Waveform mode (0x18 for GC16, 0x1000 for DU).
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
#else
  /*!
   * \brief Send screen update
   *
   * \param rect      Rectangle to update in pixels.
   * \param waveform  Waveform mode (0x18 for GC16, 0x1000 for DU).
   * \param pixelMode  Pixel update mode.
   */
  void update(
    QRect rect,
    int waveform,
    PixelMode pixelMode,
    QFlags<EPFramebuffer::UpdateFlag> flags
  );
#endif
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
