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
    bool event(QEvent* event) override;

public:
    WaitThread(int frameBufferFd);
    ~WaitThread();
    bool isEmpty();
    void addWait(Window* window, unsigned int marker, std::function<void()> callback);
    void addWait(unsigned int marker, std::function<void()> callback);
    bool isComplete(Window* window, unsigned int marker);
    void addCompleted(QString window, unsigned int marker, unsigned int internalMarker, bool waited);
    QQueue<CompletedMarker> m_completedMarkers;
    QQueue<PendingMarkerWait> m_pendingMarkerWaits;
    QMutex m_completedMutex;
    QMutex m_pendingMutex;
    int m_frameBufferFd;

private:
    bool m_processing;

    bool isPendingMarkerWaitDone(PendingMarkerWait pendingMarkerWait);
    void purgeOldCompletedItems();
    QList<PendingMarkerWait> getPendingMarkers();
    QQueue<CompletedMarker> getCompletedMarkers();
};

class GUIThread : public QThread{
    Q_OBJECT

protected:
    bool event(QEvent* event) override;
    void run() override;

public:
    GUIThread();
    ~GUIThread();
    bool isActive();
    QRect* m_screenGeometry;
    QMutex m_deleteQueueMutex;
    QQueue<RepaintRequest> m_repaintEvents;
    QQueue<Window*> m_deleteQueue;
    void addCompleted(QString window, unsigned int marker, unsigned int internalMarker, bool waited);
    WaitThread* waitThread();

public slots:
    void enqueue(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool global = false, std::function<void()> callback = nullptr);

private:
    QMutex m_mutex;
    bool m_processing;
    int m_frameBufferFd;
    QAtomicInteger<unsigned int> m_currentMarker;
    WaitThread* m_waitThread;

    void repaintWindow(QPainter* painter, QRect* rect, Window* window);
    void redraw(RepaintRequest& event);
    QQueue<RepaintRequest> getRepaintEvents();
    void deletePendingWindows();
    void scheduleUpdate();
};
