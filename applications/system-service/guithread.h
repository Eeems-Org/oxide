#pragma once
#include <QThread>
#include <epframebuffer.h>

#include "guiapi.h"

struct RepaintRequest{
    Window* window;
    QRect region;
    EPFrameBuffer::WaveformMode waveform;
    unsigned int marker;
    bool global;
    std::function<void()> callback = nullptr;
};
struct PendingMarkerWait{
    unsigned int marker;
    QString window;
    std::function<void()> callback;
    QDeadlineTimer deadline;
};
struct CompletedMarker{
    unsigned int internalMarker;
    unsigned int marker;
    QString window;
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
    void addWait(Window* window, unsigned int marker, std::function<void()> callback);
    void addWait(unsigned int marker, std::function<void()> callback);
    bool isComplete(Window* window, unsigned int marker);
    void addCompleted(QString window, unsigned int marker, unsigned int internalMarker, bool waited);
    void shutdown();
    void removeWait(QString window);

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
    GUIThread();
    ~GUIThread();
    bool isActive();
    QRect* m_screenGeometry;
    void addCompleted(QString window, unsigned int marker, unsigned int internalMarker, bool waited);
    void deleteWindowLater(Window* window);
    WaitThread* waitThread();
    void shutdown();

public slots:
    void enqueue(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool global = false, std::function<void()> callback = nullptr);

private:
    QQueue<RepaintRequest> m_repaintEvents;
    QMutex m_repaintMutex;
    QSemaphore m_repaintCount;
    QWaitCondition m_repaintWait;
    int m_frameBufferFd;
    QAtomicInteger<unsigned int> m_currentMarker;
    WaitThread* m_waitThread;

    void repaintWindow(QPainter* painter, QRect* rect, Window* window);
    void redraw(RepaintRequest& event);
    void scheduleUpdate();
    bool inRepaintEvents(Window* window);
};
