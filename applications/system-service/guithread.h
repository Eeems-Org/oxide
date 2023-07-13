#pragma once
#include <QThread>
#include <epframebuffer.h>

#include "guiapi.h"

class RepaintNotifier;

class RepaintEvent : public QEvent{

public:
    static QEvent::Type eventType;
    RepaintEvent(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform);

    Window* window(){ return m_window; }
    const QRect& region(){ return m_region; }
    EPFrameBuffer::WaveformMode waveform(){ return m_waveform; }

private:
    static QEvent::Type registeredType();
    Window* m_window;
    QRect m_region;
    EPFrameBuffer::WaveformMode m_waveform;
};

class GUIThread : public QThread{
    Q_OBJECT

protected:
    bool event(QEvent* event) override;

public:
    void run() override;
    void repaintWindow(QPainter* painter, Window* window, QRect* rect);
    RepaintNotifier* m_repaintNotifier;
    QRect* m_screenGeometry;

private:
    void redraw(RepaintEvent* event);
};
