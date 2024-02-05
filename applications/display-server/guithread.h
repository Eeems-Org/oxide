#pragma once
#ifdef EPAPER
#include <QThread>
#include <QWaitCondition>
#include <QSemaphore>
#include <QQueue>
#include <epframebuffer.h>
#include <liboxide/threading.h>
#include <libblight/concurrentqueue.h>

#include "surface.h"

#define guiThread GUIThread::singleton()

struct RepaintRequest{
    std::shared_ptr<Surface> surface;
    QRegion region;
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

signals:
    void settled();

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
    int framebuffer();
    void sendUpdate(const QRect& rect, EPFrameBuffer::WaveformMode waveform, unsigned int marker);

private:
    GUIThread(QRect screenGeometry);
    moodycamel::ConcurrentQueue<RepaintRequest> m_repaintEvents;
    int m_frameBufferFd;
    QAtomicInteger<unsigned int> m_currentMarker;
    QMutex m_repaintMutex;
    QWaitCondition m_repaintWait;
    QRect m_screenGeometry;
    QPoint m_screenOffset;
    QRect m_screenRect;

    void clearFrameBuffer();
    void repaintSurface(QPainter* painter, QRect* rect, std::shared_ptr<Surface> surface);
    void redraw(RepaintRequest& event);
    QList<std::shared_ptr<Surface>> visibleSurfaces();
};
#endif
