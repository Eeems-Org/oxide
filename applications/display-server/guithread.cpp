#include "guithread.h"
#ifdef EPAPER
#include "connection.h"
#include "dbusinterface.h"

#include <epframebuffer.h>
#include <QAbstractEventDispatcher>
#include <QTimer>
#include <QPainter>
#include <fcntl.h>
#include <mxcfb.h>
#include <liboxide/debug.h>
#include <liboxide/threading.h>
#include <libblight/clock.h>

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
                    emit settled();
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
        instance = new GUIThread(EPFramebuffer::instance()->auxBuffer.rect());
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
    // TODO: Create an fd using memfd_create
    // truncate it to the appropriate size, then
    // replace the buffers in libqgsepaper instead.
    m_frameBufferFd = open("/dev/null", O_RDWR);
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
    Blight::WaveformMode waveform,
    unsigned int marker,
    bool global,
    std::function<void()> callback
){
    if(isInterruptionRequested() || dbusInterface->inExclusiveMode()){
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
            O_WARNING("Surface is not currently visible" << surface->id());
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
        O_WARNING("Region does not intersect with screen" << surface->id() << region);
        if(callback != nullptr){
            callback();
        }
        return;
    }
    auto visibleSurfaces = this->visibleSurfaces();
    QRegion repaintRegion(intersected);
    if(!global){
        // Don't repaint portions covered by another surface that doesn't have alpha channel
        auto i = visibleSurfaces.constEnd();
        while(i != visibleSurfaces.constBegin()){
            --i;
            auto _surface = *i;
            if(surface == _surface){
                break;
            }
            if(_surface->image()->hasAlphaChannel()){
                continue;
            }
            auto geometry = _surface->geometry();
            repaintRegion -= region
                .intersected(geometry)
                .translated(-geometry.topLeft())
                .intersected(m_screenRect);
        }
    }
    if(repaintRegion.isEmpty()){
        O_WARNING("Region is currently covered" << surface->id() << region);
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
}

void GUIThread::notify(){
    if(!dbusInterface->inExclusiveMode()){
        m_repaintWait.notify_one();
    }
}

void GUIThread::clearFrameBuffer(){
    auto instance = EPFramebuffer::instance();
    instance->auxBuffer.fill(Qt::white);
    instance->swapBuffers(
        m_screenGeometry,
        EPContentType::Color,
        EPScreenMode::QualityFull,
        EPFramebuffer::UpdateFlag::CompleteRefresh
    );
}

int GUIThread::framebuffer(){ return m_frameBufferFd; }

void GUIThread::repaintSurface(QPainter* painter, QRect* rect, std::shared_ptr<Surface> surface){
    const QRect surfaceGeometry = surface->geometry();
    const QRect surfaceGlobalRect = surfaceGeometry.translated(-m_screenGeometry.topLeft());
    const QRect imageRect = rect
        ->translated(-surfaceGlobalRect.left(), -surfaceGlobalRect.top())
        .intersected(surface->image()->rect());
    const QRect sourceRect = rect->intersected(surfaceGlobalRect);
    if(imageRect.isEmpty() || !imageRect.isValid() || sourceRect.isEmpty() || !sourceRect.isValid()){
        return;
    }
    O_DEBUG("Repaint surface" << surface->id() << sourceRect << imageRect);
    // TODO - See if there is a way to detect if there is just transparency in the region
    //        and don't mark this as repainted.
    painter->drawImage(sourceRect, *surface->image().get(), imageRect);
}

void GUIThread::redraw(RepaintRequest& event){
    Q_ASSERT(QThread::currentThread() == (QThread*)this);
    if(dbusInterface->inExclusiveMode()){
        O_DEBUG("In exclusive mode, skipping redraw");
        return;
    }
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
    Blight::ClockWatch cw;
    // Get visible region on the screen to repaint
    O_DEBUG("Repainting" << region.boundingRect());
    QImage *frameBuffer = &EPFramebuffer::instance()->auxBuffer;
    Qt::GlobalColor colour = frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::white;
    QPainter painter(frameBuffer);
    while(!painter.isActive()){
        eventDispatcher()->processEvents(QEventLoop::AllEvents);
        painter.begin(frameBuffer);
    }
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for(QRect rect : event.region){
        if(event.global || event.surface->image()->hasAlphaChannel()){
            painter.fillRect(rect, colour);
            for(auto& surface : visibleSurfaces()){
                if(surface != nullptr){
                    repaintSurface(&painter, &rect, surface);
                }
            }
        }else{
            repaintSurface(&painter, &rect, event.surface);
        }
        sendUpdate(rect, event.waveform, event.marker);
    }
    painter.end();
    O_DEBUG("Repaint" << region.boundingRect() << "done in" << region.rectCount() << "paints, and" << cw.elapsed() << "seconds");
}

void GUIThread::sendUpdate(const QRect& rect, Blight::WaveformMode waveform, unsigned int marker){
    auto mode = rect == m_screenRect
        ? EPScreenMode::QualityFull
        : EPScreenMode::QualityFast;
    auto flag = rect == m_screenRect
        ? EPFramebuffer::UpdateFlag::CompleteRefresh
        : EPFramebuffer::UpdateFlag::NoRefresh;
    EPFramebuffer::instance()->swapBuffers(
        m_screenGeometry,
        EPContentType::Mono,
        mode,
        flag
    );
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
