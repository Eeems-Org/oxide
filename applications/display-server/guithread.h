#pragma once
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
};
struct PendingMarkerWait{
    unsigned int marker;
    QString surface;
    std::function<void()> callback;
    QDeadlineTimer deadline;
};
struct CompletedMarker{
    unsigned int internalMarker;
    unsigned int marker;
    QString surface;
    bool waited;
    QDeadlineTimer deadline;
    QDeadlineTimer cleanupDeadline;
};

class WaitThread : public QThread{
    Q_OBJECT

protected:
    void run() override;

public:
    WaitThread(int frameBufferFd);
    ~WaitThread();
    bool isEmpty();
    void addWait(std::shared_ptr<Surface> surface, unsigned int marker, std::function<void()> callback);
    void addWait(unsigned int marker, std::function<void()> callback);
    bool isComplete(std::shared_ptr<Surface> surface, unsigned int marker);
    void addCompleted(QString surface, unsigned int marker, unsigned int internalMarker, bool waited);
    void shutdown();
    void removeWait(QString surface);

private:
    int m_frameBufferFd;
    QQueue<CompletedMarker> m_completedMarkers;
    QMutex m_completedMutex;
    QQueue<PendingMarkerWait> m_pendingMarkerWaits;
    QMutex m_pendingMutex;
    QWaitCondition m_pendingtWait;

    bool isPendingMarkerWaitDone(PendingMarkerWait pendingMarkerWait);
    void purgeOldCompletedItems();
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
    void addCompleted(
        QString surface,
        unsigned int marker,
        unsigned int internalMarker,
        bool waited
    );
    WaitThread* waitThread();
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

private:
    GUIThread();
    QQueue<RepaintRequest> m_repaintEvents;
    QMutex m_repaintMutex;
    QSemaphore m_repaintCount;
    QWaitCondition m_repaintWait;
    int m_frameBufferFd;
    QAtomicInteger<unsigned int> m_currentMarker;
    WaitThread* m_waitThread;

    void repaintSurface(QPainter* painter, QRect* rect, std::shared_ptr<Surface> surface);
    void redraw(RepaintRequest& event);
    void scheduleUpdate();
    bool inRepaintEvents(std::shared_ptr<Surface> surface);
};
