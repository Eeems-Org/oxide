#pragma once
// This file is missing from the SDK.
// It's a recreation based on the disassembly of the library.
#include <QFlags>
#include <QImage>
#include <QRect>
#include <tuple>

// libqsgepaper EPFramebufferTcon layout (returned by instance()):
//   auxBuffer at +0x60, mainBuffer at +0x70, total size 0xb8
#if defined(__arm__)
#define EPFR_SIZE 0xb8
#define EPFR_OFFSET_MAINBUFFER 0x70
#define EPFR_OFFSET_AUXBUFFER 0x60
#elif defined(__aarch64__)
#define EPFR_SIZE 0xb8
#define EPFR_OFFSET_MAINBUFFER 0x70
#define EPFR_OFFSET_AUXBUFFER 0x60
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

/*!
 * \brief Base class for the reMarkable framebuffer abstraction.
 *
 * EPFramebuffer manages a pair of QImages (front/main buffer and
 * back/auxiliary buffer) and provides swapBuffers() to submit
 * display updates. The library's instance() returns the platform-
 * specific subclass (EPFramebufferFusion).
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
   * (mainBuffer, auxBuffer); `old` is an optional pointer to a
   * third QImage used as the "previous frame" buffer for delta
   * computation.
   */
  void setBuffers(std::tuple<QImage, QImage>, QImage* old = nullptr);

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
};

/*!
 * \brief Intermediate subclass (stand-in for the library's internal
 *        EPFramebufferSwtcon).
 *
 * Swtcon functions like initialize() and sync() are called during
 * startup but are not needed by consumers after instance() has
 * returned.
 */
class EPFramebufferSwtcon : public EPFramebuffer {
public:
  /*! Open /dev/fb0 and set up the framebuffer controller. */
  void initialize(void);

  /*! Wait for pending updates to complete. */
  void sync(void);

protected:
  /*!
   * \brief Send a TCon update via the swtcon abstraction.
   *
   * Exists in libqsgepaper on rM2, rMPP, rMPPM, and rMPPure but NOT on rM1.
   * rM2 uses the swtcon abstraction directly; rMPP+ backs it with DRM/KMS.
   *
   * \param rect      Rectangle to update in pixels.
   * \param waveform  Waveform mode (e.g. 0x18 for GC16).
   * \param pixelMode Pixel mode.
   * \param flags     Update flags.
   *
   * \note rM1 uses sendTConUpdate() instead (MXCFB_SEND_UPDATE ioctl).
   */
  void update(
    QRect rect,
    int waveform,
    int pixelMode,
    QFlags<EPFramebuffer::UpdateFlag> flags
  );

  /*!
   * \brief Send an update to the TCon hardware.
   *
   * rM1 only. Uses the imx-epdc (mxcfb) framebuffer interface:
   * fills an mxcfb_update_data struct and issues MXCFB_SEND_UPDATE ioctl.
   *
   * \param rect      Rectangle to update in pixels.
   * \param waveform  Waveform mode (0x18 for GC16, 0x1000 for DU).
   * \param flags     Update flags (CompleteRefresh forces full update).
   */
  void sendTConUpdate(
    const QRect& rect,
    int waveform,
    QFlags<EPFramebuffer::UpdateFlag> flags
  );
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
 *   +0x60  QImage auxBuffer   (back-buffer copy)
 *   +0x70  QImage mainBuffer  (front-buffer copy)
 *   +0x84  update-marker counter
 *   +0x88  constants
 *   +0x90  QRegion
 *   +0x98  QFile  (/dev/fb0)
 *   +0xa8  QImage "old" buffer
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
    OPAQUE_B[EPFR_OFFSET_MAINBUFFER - sizeof(QImage) - EPFR_OFFSET_AUXBUFFER];

public:
  /*! Main (front) buffer - the composed screen content. */
  QImage mainBuffer;

private:
  char OPAQUE_C[EPFR_SIZE - EPFR_OFFSET_MAINBUFFER - sizeof(QImage)];
};
