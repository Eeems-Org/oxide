#pragma once
#ifdef EPAPER
#include <QThread>
#include <QWaitCondition>
#include <QSemaphore>
#include <QQueue>
#include <liboxide/epaper.h>
#include <liboxide/threading.h>

#include "surface.h"

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
    static GUIThread* singleton(){
        static GUIThread* instance = nullptr;
        if(instance == nullptr){
            instance = new GUIThread();
            instance->m_screenGeometry = EPFrameBuffer::instance()->framebuffer()->rect();
            Oxide::startThreadWithPriority(instance, QThread::TimeCriticalPriority);
        }
        return instance;
    }
    ~GUIThread();
    bool isActive();
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
    void clearFrameBuffer();

private:
    GUIThread();
    QQueue<RepaintRequest> m_repaintEvents;
    QMutex m_repaintMutex;
    QSemaphore m_repaintCount;
    QWaitCondition m_repaintWait;
    int m_frameBufferFd;
    QAtomicInteger<unsigned int> m_currentMarker;

    void repaintSurface(QPainter* painter, QRect* rect, std::shared_ptr<Surface> surface);
    void redraw(RepaintRequest& event);
    void scheduleUpdate();
    bool inRepaintEvents(std::shared_ptr<Surface> surface);
    EPFrameBuffer::WaveformMode getWaveFormMode(
        const QRect& region,
        EPFrameBuffer::WaveformMode defaultValue
    );
};
#endif
