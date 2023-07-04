#ifndef EPFRAMEBUFFER_H
#define EPFRAMEBUFFER_H

#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtGui/QImage>

#include <atomic>
#include <mutex>

#if defined(LIBOXIDE_LIBRARY)
#include "liboxide_global.h"
#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif
#else
#include <liboxide/liboxide_global.h>
#endif


class LIBOXIDE_EXPORT EPFrameBuffer : public QObject
{
    Q_OBJECT

    // Testing results:
    // 0: ~780ms, initialize to white
    // 1: ~170ms, mono, no flashing
    // 2: ~460ms, 15 grayscale, flashing all pixels white -> black pixels
    // 3: ~460ms, 15, grayscale, flashing all pixels white -> black pixels
    // 4: ~460ms, 4 grayscale, flashing all pixels white -> black pixels
    // 5: ~460ms, 4 grayscale, flashing all pixels white -> black pixels
    // 6: ~135ms, no flashing
    // 7: ~300ms, white, 2 gray, 1 black, no flashing, stuck black?
    // 8: ~435ms, fast flashing, all gray, highlight, several grays, lowest color - 1 gets stuck
    // 9: ~2365ms, lots of flashing, delta something?

    enum Waveform {
        /** Display initialization
          * All -> white
          * 2000ms
          * ~780ms stall in driver
          * Supposedly low ghosting, high ghosting in practice.
          */
        INIT = 0,

        /**
          * Monochrome menu, text input, pen input
          * All -> black/white
          * 260ms to complete
          * 170ms stall in driver
          * Low ghosting
          */
        DU = 1,

        /** High quality images
          * All -> all
          * 480ms to complete
          * 460ms stall in driver
          * Very low ghosting
          */
        GC16 = 2,

        /** Text with white background
          * 16 -> 16
          * 480ms to complete
          * 460ms stall in driver
          * Medium ghosting
          *
          * Local update when white to white, Global update when 16 gray levels
          * For drawing anti-aliased text with reduced flash. This waveform makes full screen update
          * - the entire display except pixels staying in white will update as the new image is
          * written.
          */
        GL16 = 3,

        /** Text with white background
          * All -> all
          * 480ms to complete
          * 460ms stall in driver
          * Low ghosting
          *
          * Global Update
          * For drawing anti-aliased text with reduced flash and image artifacts (used in
          * conjunction with an image preprocessing algorithm, very little ghosting). Drawing time
          * is around 600ms. This waveform makes full screen update - all the pixels are updated or
          * cleared. Use this for anti-aliased text.
          */
        GLR16 = 4,

        /** Text and graphics with white background
          * All -> all
          * 480ms to complete
          * 460ms stall in driver
          * Low ghosting
          *
          * Global Update
          * For drawing anti-aliased text with reduced flash and image artifacts (used in
          * conjunction with an image preprocessing algorithm, very little ghosting) even more
          * compared to the GLR16 mode. Drawing time is around 600ms. This waveform makes full
          * screen update - all the pixels are updated or cleared. Use this for anti-aliased text.
          */
        GLD16 = 5,

        /** Fast page flipping at reduced contrast
          * [0, 29, 30, 31] -> [0, 30]
          * 120ms to complete
          * 135ms stall in driver
          * Medium ghosting
          *
          * Local Update
          * For drawing black and white 2-color images. This waveform supports transitions from and
          * to black or white only. It cannot be used to update to any graytone other than black or
          * white. Drawing time is around 120ms. This waveform makes partial screen update - it
          * updates only changed pixels. Image quality is reduced in exchange for the quicker
          * response time. This waveform can be used for fast updates and simple animation.
          */
        A2 = 6,

        /** Anti-aliased text in menus, touch and pen input
          * All -> [0, 10, 20, 30]
          * 290ms to complete
          * 300ms stall in driver
          *
          * No flashing, black seems to be stuck afterwards?
          */
        DU4 = 7,

        /** Unknown1
          * 435ms stall in driver
          *
          * Fast flashing, all gray, highlight?
          * Multiple grays
          * Next to lowest color gets stuck
          */
        UNKNOWN = 8,

        /** Init?
          * 2365ms stall in driver
          *
          * Lots of flashing, seems to do some delta updates (inverting unchanged?)
          */
        INIT2 = 9
    };

public:
    static EPFrameBuffer *instance();

    /// Which waveforms we use for different kinds of updates
    enum WaveformMode {
        Initialize = INIT,
        Mono = DU,
        Grayscale = GL16,
        HighQualityGrayscale = GC16,
        Highlight = UNKNOWN
    };

    enum UpdateMode {
        PartialUpdate = 0x0,
        FullUpdate = 0x1
    };

    static QImage *framebuffer() {
        return &instance()->m_fb;
    }

    Q_INVOKABLE static void setForceFull(bool force) { instance()->m_forceFull = force; }
    static bool isForceFull() { return instance()->m_forceFull; }

    int lastUpdateId() const { return m_lastUpdateId; }

    void setSuspended(bool suspended) { m_suspended = suspended; }
    bool isSuspended() const {
        std::lock_guard<std::mutex> locker(fbMutex);
        return m_suspended;
    }

    mutable std::mutex fbMutex;

    qint64 timeSinceLastUpdate() const;

public slots:
    static void clearScreen();
    static void sendUpdate(QRect rect, WaveformMode waveform, UpdateMode mode, bool sync = false);
    static void waitForLastUpdate();

private:
    EPFrameBuffer();
    QImage m_fb;
    QFile m_deviceFile;
    bool m_forceFull = false;
    bool m_suspended = false;
    int m_lastUpdateId = 0;

    std::mutex m_timerLock;
    QElapsedTimer m_lastUpdateTimer;
};

#endif // EPFRAMEBUFFER_H
