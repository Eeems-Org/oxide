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
            m_repaintMutex.unlock();
            // New repaint request each loop as we have a shared pointer we need to clear
            RepaintRequest event;
            if(!m_repaintEvents.try_dequeue(event)){
                dbusInterface->processRemovedSurfaces();
                dbusInterface->processClosingConnections();
                if(!m_repaintEvents.try_dequeue(event)){
                    // Wait for up to 500ms before trying again
                    m_repaintMutex.lock();
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
        instance = new GUIThread(EPFrameBuffer::instance()->framebuffer()->rect());
        Oxide::startThreadWithPriority(instance, QThread::TimeCriticalPriority);
    }
    return instance;
}

GUIThread::GUIThread(QRect screenGeometry)
: QThread(),
  m_screenGeometry{screenGeometry},
  m_screenOffset{screenGeometry.topLeft()},
  m_screenRect{m_screenGeometry.translated(-m_screenOffset)}
{
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
        if(callback != nullptr){
            callback();
        }
        return;
    }
    Q_ASSERT(global || surface != nullptr);
    QRect intersected;
    if(global){
        intersected = region;
    }else{
        if(!surface->visible()){
            O_WARNING("Surface is not currently visible:" << surface->id());
            if(callback != nullptr){
                callback();
            }
            return;
        }
        auto surfaceGeometry = surface->geometry();
        intersected = region
            .translated(surfaceGeometry.topLeft())
            .intersected(surfaceGeometry)
            .intersected(m_screenRect);
    }
    if(intersected.isEmpty()){
        O_WARNING("Region does not intersect with screen:" << region);
        if(callback != nullptr){
            callback();
        }
        return;
    }
    m_repaintMutex.lock();
    auto visibleSurfaces = this->visibleSurfaces();
    QRegion repaintRegion(intersected);
    if(!global){
        // Don't repaint portions covered by another surface
        auto i = visibleSurfaces.constEnd();
        while(i != visibleSurfaces.constBegin()) {
            --i;
            auto _surface = *i;
            if(surface == _surface){
                break;
            }
            auto geometry = _surface->geometry();
            repaintRegion -= region
                .intersected(geometry)
                .translated(-geometry.topLeft())
                .intersected(m_screenRect);
        }
    }
    if(repaintRegion.isEmpty()){
        O_WARNING("Region is not currently visible:" << region);
        if(callback != nullptr){
            callback();
        }
        return;
    }
    m_repaintEvents.enqueue(RepaintRequest{
        .surface = surface,
        .region = repaintRegion,
        .waveform = waveform,
        .marker = marker,
        .global = global,
        .callback = callback
    });
    notify();
    m_repaintMutex.unlock();
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
    auto& region = event.region;
    if(region.isEmpty()){
        O_WARNING("Empty repaint region" << region);
        return;
    }
    // Get visible region on the screen to repaint
    O_DEBUG("Repainting" << region.boundingRect());
    auto frameBuffer = EPFrameBuffer::instance()->framebuffer();
    Qt::GlobalColor colour = frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::white;
    QPainter painter(frameBuffer);
    while(!painter.isActive()){
        eventDispatcher()->processEvents(QEventLoop::AllEvents);
        painter.begin(frameBuffer);
    }
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    for(QRect rect : event.region){
        // Get the waveform required to draw the current data,
        // as we may be transitioning from grayscale to white.
        // The request may not know that and try to use Mono
        auto waveform = getWaveFormMode(rect, event.waveform);
        if(event.global){
            painter.fillRect(rect, colour);
            for(auto& surface : visibleSurfaces()){
                if(surface != nullptr){
                    repaintSurface(&painter, &rect, surface);
                }
            }
        }else{
            repaintSurface(&painter, &rect, event.surface);
        }
        sendUpdate(rect, waveform);
    }
    painter.end();
    O_DEBUG("Repaint" << region.boundingRect() << "done in" << region.rectCount() << "paints");
}

void GUIThread::sendUpdate(const QRect& rect, EPFrameBuffer::WaveformMode previousWaveform){
    // TODO - detect if there was no change to the repainted region and skip,
    //        Maybe hash the data before and compare after?
    //        Also properly handle when it was previously gray and needs to now be white
    // https://doc.qt.io/qt-5/qcryptographichash.html
    auto waveform = getWaveFormMode(rect, previousWaveform);
    auto mode = rect == m_screenRect
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

QList<std::shared_ptr<Surface>> GUIThread::visibleSurfaces(){
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
    return visibleSurfaces;
}
#endif
