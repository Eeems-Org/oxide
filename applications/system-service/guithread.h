#pragma once
#include <QThread>
#include <epframebuffer.h>

#include "guiapi.h"

class RepaintNotifier;

class RepaintEvent : public QEvent{

public:
    static QEvent::Type eventType;
    RepaintEvent(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker);

    Window* window(){ return m_window; }
    const QRect& region(){ return m_region; }
    EPFrameBuffer::WaveformMode waveform(){ return m_waveform; }
    unsigned int marker(){ return m_marker; }

private:
    static QEvent::Type registeredType();
    Window* m_window;
    QRect m_region;
    EPFrameBuffer::WaveformMode m_waveform;
    unsigned int m_marker;
};

class GUIThread : public QThread{
    Q_OBJECT

protected:
    bool event(QEvent* event) override;

public:
    void run() override;
    void repaintWindow(QPainter* painter, Window* window, QRect* rect);
    void flush();
    RepaintNotifier* m_repaintNotifier;
    QRect* m_screenGeometry;

private:
    void redraw(RepaintEvent* event);
};
