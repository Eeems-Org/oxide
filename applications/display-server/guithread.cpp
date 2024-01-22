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

void WaitThread::run(){
    O_DEBUG("Thread started");
    QTimer::singleShot(0, this, [this]{
        Q_ASSERT(QThread::currentThread() == (QThread*)this);
        forever{
            m_pendingMutex.lock();
            if(m_pendingMarkerWaits.isEmpty()){
                m_pendingtWait.wait(&m_pendingMutex);
            }
            QList<PendingMarkerWait> requeue;
            QList<std::function<void()>> callbacks;
            while(!m_pendingMarkerWaits.isEmpty()){
                auto pendingMarkerWait = m_pendingMarkerWaits.dequeue();
                m_pendingMutex.unlock();
                if(!isPendingMarkerWaitDone(pendingMarkerWait)){
                    requeue.append(pendingMarkerWait);
                    m_pendingMutex.lock();
                    continue;
                }
                if(pendingMarkerWait.surface.isEmpty()){
                    O_DEBUG("Global marker wait completed" << pendingMarkerWait.marker);
                }else{
                    O_DEBUG(
                        "Window marker wait completed"
                        << pendingMarkerWait.surface
                        << pendingMarkerWait.marker
                    );
                }
                callbacks.append(pendingMarkerWait.callback);
                m_pendingMutex.lock();
            }
            m_pendingMarkerWaits.append(requeue);
            m_pendingMutex.unlock();
            purgeOldCompletedItems();
            for(auto& callback : callbacks){
                callback();
            }
            eventDispatcher()->processEvents(QEventLoop::AllEvents);
            if(isInterruptionRequested()){
                O_DEBUG("Interruption requested, leaving loop");
                break;
            }
        }
    });
    auto res = exec();
    O_DEBUG("Thread stopped with exit code:" << res);
}

WaitThread::WaitThread(int frameBufferFd) : QThread(), m_frameBufferFd{frameBufferFd}{
    setObjectName("ioctl");
    moveToThread(this);
}

WaitThread::~WaitThread(){
    m_pendingMutex.lock();
    for(auto& pendingWaitMarker : qAsConst(m_pendingMarkerWaits)){
        pendingWaitMarker.callback();
    }
    m_pendingMarkerWaits.clear();
    m_pendingMutex.unlock();
    m_completedMutex.lock();
    m_completedMarkers.clear();
    m_completedMutex.unlock();
}

bool WaitThread::isEmpty(){ return m_completedMarkers.isEmpty() && m_completedMarkers.isEmpty(); }

void WaitThread::addWait(
    std::shared_ptr<Surface> surface,
    unsigned int marker,
    std::function<void()> callback
){
    if(
        (
            surface != nullptr
            && marker != 0
            && isComplete(surface, marker)
        )
        || isInterruptionRequested()
    ){
        callback();
        return;
    }
    m_completedMutex.lock();
    PendingMarkerWait pendingMarkerWait{
        .marker = marker,
        .surface = surface == nullptr ? "" : surface->id(),
        .callback = callback,
        .deadline = QDeadlineTimer(1000 * 7, Qt::PreciseTimer),
    };
    m_pendingMarkerWaits.append(pendingMarkerWait);
    m_pendingtWait.notify_all();
    m_completedMutex.unlock();
}

void WaitThread::addWait(
    unsigned int marker,
    std::function<void()> callback
){ addWait(nullptr, marker, callback); }

bool WaitThread::isComplete(std::shared_ptr<Surface> surface, unsigned int marker){
    if(isInterruptionRequested()){
        return true;
    }
    QMutexLocker locker(&m_completedMutex);
    Q_UNUSED(locker);
    auto identifier = surface->id();
    for(auto& item : qAsConst(m_completedMarkers)){
        if(item.surface == identifier && item.marker == marker){
            return item.waited;
        }
    }
    return false;
}

void WaitThread::addCompleted(
    QString surface,
    unsigned int marker,
    unsigned int internalMarker,
    bool waited
){
    m_completedMutex.lock();
    CompletedMarker completedMarker{
        .internalMarker = internalMarker,
        .marker = marker,
        .surface = surface,
        .waited = waited,
        .deadline = QDeadlineTimer(1000 * 7, Qt::PreciseTimer),
        .cleanupDeadline = QDeadlineTimer(1000 * 14, Qt::PreciseTimer),
    };
    m_completedMarkers.append(completedMarker);
    m_completedMutex.unlock();
    m_pendingtWait.notify_all();
}

void WaitThread::shutdown(){
    O_DEBUG("Stopping thread" << this);
    requestInterruption();
    m_pendingtWait.notify_all();
    quit();
    QDeadlineTimer deadline(6000);
    if(!wait(deadline)){
        O_WARNING("Terminated thread" << this);
        terminate();
        wait();
    }
}

void WaitThread::removeWait(QString surface){
    m_completedMutex.lock();
    QMutableListIterator<PendingMarkerWait> i(m_pendingMarkerWaits);
    while(i.hasNext()){
        auto pendingMarkerWait = i.next();
        if(pendingMarkerWait.surface == surface){
            i.remove();
        }
    }
    m_pendingtWait.notify_all();
    m_completedMutex.unlock();
}

bool WaitThread::isPendingMarkerWaitDone(PendingMarkerWait pendingMarkerWait){
    // Marker should never be 0, but just in case
    if(pendingMarkerWait.marker == 0){
        return true;
    }
    if(pendingMarkerWait.deadline.isForever()){
        O_WARNING("Invalid MXCFB_WAIT_FOR_UPDATE_COMPLETE marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.surface);
        return true;
    }
    // Somehow this marker slipped through the cracks, lets assume it's done after 7 seconds
    if(pendingMarkerWait.deadline.hasExpired()){
        O_WARNING("Stale MXCFB_WAIT_FOR_UPDATE_COMPLETE marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.surface);
        return true;
    }
    QMutexLocker locker(&m_completedMutex);
    Q_UNUSED(locker);
    for(auto& completedItem : m_completedMarkers){
        if(pendingMarkerWait.surface != completedItem.surface || pendingMarkerWait.marker != completedItem.marker){
            continue;
        }
        if(completedItem.deadline.hasExpired()){
            O_WARNING("Stale completed item for marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.surface);
            completedItem.waited = true;
            return true;
        }
        if(completedItem.internalMarker == 0){
            O_WARNING("Unused MXCFB_WAIT_FOR_UPDATE_COMPLETE marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.surface);
            completedItem.waited = true;
            return true;
        }
        if(!completedItem.waited){
            O_DEBUG("Waiting for" << pendingMarkerWait.marker << "marker");
            mxcfb_update_marker_data data{
                .update_marker = completedItem.internalMarker,
                .collision_test = 0,
            };
            ioctl(m_frameBufferFd, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &data);
            completedItem.waited = true;
        }
        return completedItem.waited;
    }
    return false;
}

void WaitThread::purgeOldCompletedItems(){
    QMutexLocker locker(&m_completedMutex);
    Q_UNUSED(locker);
    QMutableListIterator i(m_completedMarkers);
    while(i.hasNext()){
        CompletedMarker completedItem = i.next();
        if(completedItem.cleanupDeadline.hasExpired()){
            i.remove();
        }
    }
}

void GUIThread::run(){
    O_DEBUG("Thread started");
    QTimer::singleShot(0, this, [this]{
        Q_ASSERT(QThread::currentThread() == (QThread*)this);
        forever{
            m_repaintMutex.lock();
            if(!m_repaintCount.available()){
                m_repaintWait.wait(&m_repaintMutex);
            }
            while(m_repaintCount.available()){
                m_repaintCount.acquire();
                auto event = m_repaintEvents.dequeue();
                m_repaintMutex.unlock();
                redraw(event);
                m_repaintMutex.lock();
            }
            m_repaintMutex.unlock();
            eventDispatcher()->processEvents(QEventLoop::AllEvents);
            if(isInterruptionRequested()){
                O_DEBUG("Interruption requested, leaving loop");
                break;
            }
        }
    });
    auto res = exec();
    O_DEBUG("Stopping thread" << m_waitThread);
    m_waitThread->requestInterruption();
    m_waitThread->quit();
    QDeadlineTimer deadline(3000);
    if(!m_waitThread->wait(deadline)){
        O_WARNING("Terminated thread" << m_waitThread);
        m_waitThread->terminate();
        m_waitThread->wait();
    }
    O_DEBUG("Thread stopped with exit code:" << res);
}

GUIThread::GUIThread() : QThread(){
    m_frameBufferFd = open("/dev/fb0", O_RDWR);
    if(m_frameBufferFd == -1){
        qFatal("Failed to open framebuffer");
    }
    m_waitThread = new WaitThread(m_frameBufferFd);
    setObjectName("gui");
    moveToThread(this);
    Oxide::startThreadWithPriority(m_waitThread, QThread::HighPriority);
}

GUIThread::~GUIThread(){
    auto repaintLocked = m_repaintMutex.tryLock(100);
    m_repaintEvents.clear();
    if(repaintLocked){
        m_repaintMutex.unlock();
    }
}

bool GUIThread::isActive(){ return m_repaintCount.available(); }

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
    m_repaintMutex.lock();
    m_repaintEvents.enqueue(RepaintRequest{
        .surface = surface,
        .region = region,
        .waveform = waveform,
        .marker = marker,
        .global = global
    });
    // Indicate that there is an item in the queue
    m_repaintCount.release();
    m_repaintWait.notify_all();
    m_repaintMutex.unlock();
    if(callback != nullptr){
        m_waitThread->addWait(surface, marker, callback);
    }
}

void GUIThread::addCompleted(
    QString surface,
    unsigned int marker,
    unsigned int internalMarker,
    bool waited
){ m_waitThread->addCompleted(surface, marker, internalMarker, waited); }

WaitThread* GUIThread::waitThread(){ return m_waitThread; }

void GUIThread::shutdown(){
    m_waitThread->shutdown();
    O_DEBUG("Stopping thread" << this);
    requestInterruption();
    m_repaintWait.notify_all();
    quit();
    QDeadlineTimer deadline(6000);
    if(!wait(deadline)){
        O_WARNING("Terminated thread" << this);
        terminate();
        wait();
    }
}

void GUIThread::repaintSurface(QPainter* painter, QRect* rect, std::shared_ptr<Surface> surface){
    const QRect surfaceGeometry = surface->geometry();
    const QRect surfaceRect = surfaceGeometry.translated(-m_screenGeometry.topLeft());
    const QRect surfaceIntersect = rect->translated(-surfaceRect.left(), -surfaceRect.top());
    if(surfaceIntersect.isEmpty()){
        return;
    }
    O_DEBUG("Repaint window" << surface->id().toStdString().c_str() << surfaceIntersect);
    // TODO - See if there is a way to detect if there is just transparency in the region
    //        and don't mark this as repainted.
    painter->drawImage(*rect, *surface->image().get(), surfaceIntersect);
}

void GUIThread::redraw(RepaintRequest& event){
    Q_ASSERT(QThread::currentThread() == (QThread*)this);
    // Get the regions that need to be repainted
    const QPoint screenOffset = m_screenGeometry.topLeft();
    const QRect screenRect = m_screenGeometry.translated(-screenOffset);
    auto region = event.region;
    if(region.isEmpty()){
        O_WARNING("Empty repaint region" << region);
        return;
    }
    if(!event.global && event.surface == nullptr){
        O_WARNING("Window missing");
        return;
    }
    // Get visible region on the screen to repaint
    QRect rect;
    if(region.isEmpty()){
        O_WARNING("Empty repaint region provided");
        return;
    }
    if(event.global){
        rect = region;
    }else{
        auto windowGeometry = event.surface->geometry();
        rect = region
            .translated(windowGeometry.topLeft())
            .intersected(windowGeometry)
            .intersected(screenRect);
    }
    if(rect.isEmpty()){
        O_WARNING("Window region does not intersect with screen:" << region);
        return;
    }
    O_DEBUG("Repainting" << rect);
    // Get windows in order of Z sort order, and filter out invalid windows
    auto visibleSurfaces = dbusInterface->visibleSurfaces();
    visibleSurfaces.erase(
        std::remove_if(
            visibleSurfaces.begin(),
            visibleSurfaces.end(),
            [](std::shared_ptr<Surface> surface){
                auto connection = dynamic_cast<Connection*>(surface->parent());
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
        // Don't repaint portions covered by another window
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
        if(!event.global){
            repaintSurface(&painter, &rect, event.surface);
        }else if(visibleSurfaces.isEmpty()){
            painter.fillRect(rect, colour);
        }else{
            for(auto& surface : visibleSurfaces){
                if(surface != nullptr){
                    repaintSurface(&painter, &rect, surface);
                }
            }
        }
    }
    painter.end();
    auto waveform = event.waveform;
    unsigned int updateMarker = 0;
    // Send updates for all the repainted regions
    for(auto rect : repaintRegion){
        // TODO - detect if there was no change to the repainted region and skip,
        //        Maybe compare against previous window states?
        //        Maybe hash the data before and compare after?
        // https://doc.qt.io/qt-5/qcryptographichash.html
        if(waveform == EPFrameBuffer::Initialize){
            waveform = EPFrameBuffer::Mono;
            // TODO - profile if it makes sense to do this instead of just picking one to always use
            for(int x = rect.left(); x < rect.right(); x++){
                for(int y = rect.top(); y < rect.bottom(); y++){
                    auto pos = QPoint(x, y);
                    // This should not happen, but just in case, ignore if the position is outside the screen
                    if(!frameBuffer->rect().contains(pos)){
                        continue;
                    }
                    auto color = frameBuffer->pixelColor(pos);
                    if(color == Qt::white || color == Qt::black || color == Qt::transparent){
                        continue;
                    }
                    if(color == Qt::gray){
                        waveform = EPFrameBuffer::Grayscale;
                        continue;
                    }
                    waveform = EPFrameBuffer::HighQualityGrayscale;
                    break;
                }
            }
        }
        auto mode =  rect == screenRect ? EPFrameBuffer::FullUpdate : EPFrameBuffer::PartialUpdate;
        O_DEBUG("Sending screen update" << rect << waveform << mode);
        updateMarker = ++m_currentMarker;
        mxcfb_update_data data{
            .update_region = mxcfb_rect{
                .top = (unsigned int)rect.top(),
                .left = (unsigned int)rect.left(),
                .width = (unsigned int)rect.width(),
                .height = (unsigned int)rect.height(),
            },
            .waveform_mode = waveform,
            .update_mode = mode,
            .update_marker = updateMarker,
            .temp = 0x0018,
            .flags = 0,
            .dither_mode = 0,
            .quant_bit = 0,
            .alt_buffer_data = mxcfb_alt_buffer_data{}, // TODO - explore this
        };
        ioctl(m_frameBufferFd, MXCFB_SEND_UPDATE, &data);
    }
    if(event.marker != 0){
        m_waitThread->addCompleted(
            event.global ? "" : event.surface->id(),
            event.marker,
            updateMarker,
            false
        );
    }
    O_DEBUG("Repaint" << rect << "done in" << repaintRegion.rectCount() << "paints");
}

bool GUIThread::inRepaintEvents(std::shared_ptr<Surface> surface){
    Q_ASSERT(surface != nullptr);
    QMutexLocker locker(&m_repaintMutex);
    Q_UNUSED(locker);
    for(auto& event : m_repaintEvents){
        if(event.surface == surface){
            return true;
        }
    }
    return false;
}
#endif
