#pragma once
#include <QThread>
#include <epframebuffer.h>

#include "guiapi.h"
#include "repaintnotifier.h"

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
    void timerEvent(QTimerEvent* event) override;

public:
    GUIThread();
    void flush();
    bool active();
    RepaintNotifier* m_repaintNotifier;
    QRect* m_screenGeometry;
    QMutex m_mutex;
    QQueue<RepaintRequest> m_repaintEvents;
    QQueue<Window*> m_deleteQueue;

public slots:
    void enqueue(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool global = false);

private:
    void repaintWindow(QPainter* painter, QRect* rect, Window* window);
    void redraw(RepaintRequest& event);
    bool m_processing;
};
