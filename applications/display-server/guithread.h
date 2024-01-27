#pragma once
#ifdef EPAPER
#include <QThread>
#include <QWaitCondition>
#include <QSemaphore>
#include <QQueue>
#include <liboxide/epaper.h>
#include <liboxide/threading.h>

#include "surface.h"
#include "concurrentqueue.h"

#define guiThread GUIThread::singleton()

struct RepaintRequest{
    std::shared_ptr<Surface> surface;
    QRect region;
    EPFrameBuffer::WaveformMode waveform;
    unsigned int marker;
    bool global;
    std::function<void()> callback;
};

class GUIThread : public QThread{
    Q_OBJECT

protected:
    void run() override;

public:
    static GUIThread* singleton();
    ~GUIThread();
    QRect m_screenGeometry;
    void shutdown();

public slots:
    void enqueue(
        std::shared_ptr<Surface> surface,
        QRect region,
        EPFrameBuffer::WaveformMode waveform,
        unsigned int marker,
        bool global = false,
        std::function<void()> callback = nullptr
    );
    void notify();
    void clearFrameBuffer();
    int framebuffer();

private:
    GUIThread();
    moodycamel::ConcurrentQueue<RepaintRequest> m_repaintEvents;
    int m_frameBufferFd;
    QAtomicInteger<unsigned int> m_currentMarker;
    QMutex m_repaintMutex;
    QWaitCondition m_repaintWait;

    void repaintSurface(QPainter* painter, QRect* rect, std::shared_ptr<Surface> surface);
    void redraw(RepaintRequest& event);
    void scheduleUpdate();
    EPFrameBuffer::WaveformMode getWaveFormMode(
        const QRect& region,
        EPFrameBuffer::WaveformMode defaultValue
    );
};
#endif
