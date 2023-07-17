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

class GUIThread : public QThread{
    Q_OBJECT

protected:
    bool event(QEvent* event) override;

public:
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
    QList<PendingMarkerWait> m_pendingMarkerWaits;
    QList<CompletedMarker> m_completedMarkers;
    QMutex m_completedMutex;
    QTimer m_completedTimer;

    void repaintWindow(QPainter* painter, QRect* rect, Window* window);
    void redraw(RepaintRequest& event);
    QQueue<RepaintRequest> getRepaintEvents();
    void deletePendingWindows();
    void scheduleUpdate();
    void resolvePendingMarkerWaits();
};
