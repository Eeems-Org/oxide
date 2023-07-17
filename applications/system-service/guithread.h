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
};
struct PendingMarkerWait{
    unsigned int marker;
    QString window;
    std::function<void()> callback;
    QElapsedTimer age;
};
struct CompletedMarker{
    unsigned int internalMarker;
    unsigned int marker;
    QString window;
    QElapsedTimer age;
    bool waited = false;
};

class WaitThread : public QThread{
    Q_OBJECT

protected:
    bool event(QEvent* event) override;

public:
    WaitThread(int frameBufferFd);
    QQueue<CompletedMarker> m_completedMarkers;
    QQueue<PendingMarkerWait> m_pendingMarkerWaits;
    QMutex m_completedMutex;
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

public:
    GUIThread();
    bool isActive();
    QRect* m_screenGeometry;
    QMutex m_mutex;
    QQueue<RepaintRequest> m_repaintEvents;
    QQueue<Window*> m_deleteQueue;
    void addWait(Window* window, unsigned int marker, std::function<void()> callback);
    void addWait(unsigned int marker, std::function<void()> callback);
    void addWait(std::function<void()> callback);
    bool isComplete(Window* window, unsigned int marker);

public slots:
    void enqueue(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool global = false);

private:
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
