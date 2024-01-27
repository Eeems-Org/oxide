#include "guithread.h"
#ifdef EPAPER
#include "connection.h"
#include "dbusinterface.h"

#include <QAbstractEventDispatcher>
#include <QTimer>
#include <QPainter>
#include <fcntl.h>
#include <mxcfb.h>
#include <liboxide/debug.h>
#include <liboxide/threading.h>

void GUIThread::run(){
    O_DEBUG("Thread started");
    clearFrameBuffer();
    QTimer::singleShot(0, this, [this]{
        Q_ASSERT(QThread::currentThread() == (QThread*)this);
        m_repaintMutex.lock();
        forever{
            // New repaint request each loop as we have a shared pointer we need to clear
            RepaintRequest event;
            if(!m_repaintEvents.try_dequeue(event)){
                dbusInterface->processRemovedSurfaces();
                dbusInterface->processClosingConnections();
                if(!m_repaintEvents.try_dequeue(event)){
                    // Wait for up to 500ms before trying again
                    m_repaintWait.wait(&m_repaintMutex, 500);
                    auto found = m_repaintEvents.try_dequeue(event);
                    if(!found){
                        // Woken by something needing to cleanup connections/surfaces
                        continue;
                    }
                }
            }
            forever{
                redraw(event);
                if(event.callback != nullptr){
                    event.callback();
                }
                if(!m_repaintEvents.try_dequeue(event)){
                    break;
                }
            }
            eventDispatcher()->processEvents(QEventLoop::AllEvents);
            if(isInterruptionRequested()){
                O_DEBUG("Interruption requested, leaving loop");
                break;
            }
        }
        m_repaintMutex.unlock();
    });
    clearFrameBuffer();
    auto res = exec();
    O_DEBUG("Thread stopped with exit code:" << res);
}

GUIThread* GUIThread::singleton(){
    static GUIThread* instance = nullptr;
    if(instance == nullptr){
        instance = new GUIThread();
        instance->m_screenGeometry = EPFrameBuffer::instance()->framebuffer()->rect();
        Oxide::startThreadWithPriority(instance, QThread::TimeCriticalPriority);
    }
    return instance;
}

GUIThread::GUIThread() : QThread(){
    m_frameBufferFd = open("/dev/fb0", O_RDWR);
    if(m_frameBufferFd == -1){
        qFatal("Failed to open framebuffer");
    }
    moveToThread(this);
}

GUIThread::~GUIThread(){
    RepaintRequest event;
    while(m_repaintEvents.try_dequeue(event));
    requestInterruption();
    quit();
    wait();
}

void GUIThread::enqueue(
    std::shared_ptr<Surface> surface,
    QRect region,
    EPFrameBuffer::WaveformMode waveform,
    unsigned int marker,
    bool global,
    std::function<void()> callback
){
    if(isInterruptionRequested()){
        callback();
        return;
    }
    Q_ASSERT(global || surface != nullptr);
    m_repaintEvents.enqueue(RepaintRequest{
        .surface = surface,
        .region = region,
        .waveform = waveform,
        .marker = marker,
        .global = global,
        .callback = callback
    });
    notify();
}

void GUIThread::notify(){ m_repaintWait.notify_one(); }

void GUIThread::clearFrameBuffer(){
    EPFrameBuffer::instance()->framebuffer()->fill(Qt::white);
    EPFrameBuffer::sendUpdate(
        m_screenGeometry,
        EPFrameBuffer::Initialize,
        EPFrameBuffer::FullUpdate,
        true
    );
    EPFrameBuffer::sendUpdate(
        m_screenGeometry,
        EPFrameBuffer::HighQualityGrayscale,
        EPFrameBuffer::FullUpdate,
        true
    );
}

int GUIThread::framebuffer(){ return m_frameBufferFd; }

void GUIThread::repaintSurface(QPainter* painter, QRect* rect, std::shared_ptr<Surface> surface){
    const QRect surfaceGeometry = surface->geometry();
    const QRect surfaceRect = surfaceGeometry.translated(-m_screenGeometry.topLeft());
    const QRect surfaceIntersect = rect->translated(-surfaceRect.left(), -surfaceRect.top());
    if(surfaceIntersect.isEmpty()){
        return;
    }
    O_DEBUG("Repaint surface" << surface->id().toStdString().c_str() << surfaceIntersect);
    // TODO - See if there is a way to detect if there is just transparency in the region
    //        and don't mark this as repainted.
    painter->drawImage(*rect, *surface->image().get(), surfaceIntersect);
}

void GUIThread::redraw(RepaintRequest& event){
    Q_ASSERT(QThread::currentThread() == (QThread*)this);
    if(!event.global){
        if(event.surface == nullptr){
            O_WARNING("surface missing");
            return;
        }
        if(event.surface->isRemoved()){
            return;
        }
    }
    auto region = event.region;
    if(region.isEmpty()){
        O_WARNING("Empty repaint region" << region);
        return;
    }
    // Get the regions that need to be repainted
    const QPoint screenOffset = m_screenGeometry.topLeft();
    const QRect screenRect = m_screenGeometry.translated(-screenOffset);
    // Get visible region on the screen to repaint
    QRect rect;
    if(region.isEmpty()){
        O_WARNING("Empty repaint region provided");
        return;
    }
    if(event.global){
        rect = region;
    }else{
        auto surfaceGeometry = event.surface->geometry();
        rect = region
            .translated(surfaceGeometry.topLeft())
            .intersected(surfaceGeometry)
            .intersected(screenRect);
    }
    if(rect.isEmpty()){
        O_WARNING("Surface region does not intersect with screen:" << region);
        return;
    }
    O_DEBUG("Repainting" << rect);
    // Get surfaces in order of Z sort order, and filter out invalid surfaces
    auto visibleSurfaces = dbusInterface->visibleSurfaces();
    visibleSurfaces.erase(
        std::remove_if(
            visibleSurfaces.begin(),
            visibleSurfaces.end(),
            [](std::shared_ptr<Surface> surface){
                auto connection = surface->connection();
                if(!surface->has("system") && getpgid(connection->pgid()) < 0){
                    O_WARNING(surface->id() << "With no running process");
                    return true;
                }
                auto image = surface->image();
                if(image->isNull()){
                    O_WARNING(surface->id() << "Null framebuffer");
                    return true;
                }
                return false;
            }
        ),
        visibleSurfaces.end()
    );
    // TODO - explore using QPainter::clipRegion to see if it can speed things up
    QRegion repaintRegion(rect);
    if(!event.global){
        // Don't repaint portions covered by another surface
        auto i = visibleSurfaces.constEnd();
        while(i != visibleSurfaces.constBegin()) {
            --i;
            auto surface = *i;
            if(event.surface == surface){
                break;
            }
            auto geometry = surface->geometry();
            repaintRegion -= region
                .intersected(geometry)
                .translated(-geometry.topLeft())
                .intersected(screenRect);
        }
    }
    auto frameBuffer = EPFrameBuffer::instance()->framebuffer();
    Qt::GlobalColor colour = frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::white;
    QPainter painter(frameBuffer);
    while(!painter.isActive()){
        eventDispatcher()->processEvents(QEventLoop::AllEvents);
        painter.begin(frameBuffer);
    }
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    for(auto rect : repaintRegion){
        // Get the waveform required to draw the current data,
        // as we may be transitioning from grayscale to white.
        // The request may not know that and try to use Mono
        auto waveform = getWaveFormMode(rect, event.waveform);
        if(!event.global){
            repaintSurface(&painter, &rect, event.surface);
        }else{
            painter.fillRect(rect, colour);
            for(auto& surface : visibleSurfaces){
                if(surface != nullptr){
                    repaintSurface(&painter, &rect, surface);
                }
            }
        }
        // TODO - detect if there was no change to the repainted region and skip,
        //        Maybe hash the data before and compare after?
        //        Also properly handle when it was previously gray and needs to now be white
        // https://doc.qt.io/qt-5/qcryptographichash.html
        waveform = getWaveFormMode(rect, waveform);
        auto mode =  rect == screenRect
            ? EPFrameBuffer::FullUpdate
            : EPFrameBuffer::PartialUpdate;
        O_DEBUG("Sending screen update" << rect << waveform << mode);
        mxcfb_update_data data{
            .update_region = mxcfb_rect{
                .top = (unsigned int)rect.top(),
                .left = (unsigned int)rect.left(),
                .width = (unsigned int)rect.width(),
                .height = (unsigned int)rect.height(),
            },
            .waveform_mode = waveform,
            .update_mode = mode,
            .update_marker = ++m_currentMarker,
            // TODO allow this to be passed through
            .temp = 0x0018,
            .flags = 0,
            .dither_mode = 0,
            .quant_bit = 0,
            // TODO - explore this
            .alt_buffer_data = mxcfb_alt_buffer_data{},
        };
        ioctl(m_frameBufferFd, MXCFB_SEND_UPDATE, &data);
    }
    painter.end();
    O_DEBUG("Repaint" << rect << "done in" << repaintRegion.rectCount() << "paints");
}

EPFrameBuffer::WaveformMode GUIThread::getWaveFormMode(
    const QRect& rect,
    EPFrameBuffer::WaveformMode defaultValue
){
    if(defaultValue == EPFrameBuffer::HighQualityGrayscale){
        return defaultValue;
    }
    auto frameBuffer = EPFrameBuffer::framebuffer();
    auto fbRect = frameBuffer->rect();
    for(int x = rect.left(); x < rect.right(); x++){
        for(int y = rect.top(); y < rect.bottom(); y++){
            auto pos = QPoint(x, y);
            // This should not happen, but just in case,
            // ignore if the position is outside the screen
            if(!fbRect.contains(pos)){
                continue;
            }
            auto color = frameBuffer->pixelColor(pos);
            if(color == Qt::white || color == Qt::black || color == Qt::transparent){
                continue;
            }
            if(color == Qt::gray){
                return EPFrameBuffer::Grayscale;
            }
            return EPFrameBuffer::HighQualityGrayscale;
        }
    }
    return defaultValue;
}
#endif
