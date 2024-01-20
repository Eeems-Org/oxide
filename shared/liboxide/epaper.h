#pragma once

#if defined(LIBOXIDE_LIBRARY)
#include "liboxide_global.h"
#else
#include <liboxide/liboxide_global.h>
#endif
#ifdef EPAPER
#ifdef __arm__
Q_IMPORT_PLUGIN(QsgEpaperPlugin)
#endif
#include <epframebuffer.h>
#else
#include <QImage>
#include <QObject>

class LIBOXIDE_EXPORT EPFrameBuffer : public QObject{
    Q_OBJECT

public:
    enum WaveformMode {
        Initialize = 0,
        Mono = 1,
        Grayscale = 3,
        HighQualityGrayscale = 2,
        Highlight = 8
    };
    enum UpdateMode {
        PartialUpdate = 0x0,
        FullUpdate = 0x1
    };
    static EPFrameBuffer* instance(){
        static EPFrameBuffer* instance;
        if(instance == nullptr){
            instance = new EPFrameBuffer();
        }
        return instance;
    }
    static QImage* framebuffer() { return &instance()->m_fb; }
    Q_INVOKABLE static void setForceFull(bool force) { instance()->m_forceFull = force; }
    static bool isForceFull() { return instance()->m_forceFull; }
    int lastUpdateId() const { return m_lastUpdateId; }
    void setSuspended(bool suspended) { m_suspended = suspended; }
    bool isSuspended() const {
        std::lock_guard<std::mutex> locker(fbMutex);
        return m_suspended;
    }
    mutable std::mutex fbMutex;
    qint64 timeSinceLastUpdate() const{ return QDateTime::currentMSecsSinceEpoch(); }

public slots:
    static void clearScreen(){ instance()->m_fb.fill(Qt::white); }
    static void sendUpdate(QRect rect, WaveformMode waveform, UpdateMode mode, bool sync = false){
        Q_UNUSED(rect);
        Q_UNUSED(waveform);
        Q_UNUSED(mode);
        Q_UNUSED(sync);
    }
    static void waitForLastUpdate(){}

private:
    EPFrameBuffer() : m_fb(1000, 1000, QImage::Format_ARGB32_Premultiplied){}
    QImage m_fb;
    bool m_forceFull = false;
    bool m_suspended = false;
    int m_lastUpdateId = 0;
};
#endif
