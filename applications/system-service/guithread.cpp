#include "guithread.h"
#include "dbusservice.h"

void WaitThread::run(){
    O_INFO("Thread started");
    QTimer::singleShot(0, [this]{
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
                if(pendingMarkerWait.window.isEmpty()){
                    O_DEBUG("Global marker wait completed" << pendingMarkerWait.marker);
                }else{
                    O_DEBUG("Window marker wait completed" << pendingMarkerWait.window << pendingMarkerWait.marker);
                }
                callbacks.append(pendingMarkerWait.callback);
                m_pendingMutex.lock();
            }
            m_pendingMarkerWaits.append(requeue);
            m_pendingMutex.unlock();
            purgeOldCompletedItems();
            for(auto callback : callbacks){
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
    O_INFO("Thread stopped with exit code:" << res);
}

WaitThread::WaitThread(int frameBufferFd) : QThread(), m_frameBufferFd{frameBufferFd}{
    setObjectName("ioctl");
    moveToThread(this);
}

WaitThread::~WaitThread(){
    m_pendingMutex.lock();
    for(auto pendingWaitMarker : m_pendingMarkerWaits){
        pendingWaitMarker.callback();
    }
    m_pendingMarkerWaits.clear();
    m_pendingMutex.unlock();
    m_completedMutex.lock();
    m_completedMarkers.clear();
    m_completedMutex.unlock();
}

bool WaitThread::isEmpty(){ return m_completedMarkers.isEmpty() && m_completedMarkers.isEmpty(); }

void WaitThread::addWait(Window* window, unsigned int marker, std::function<void ()> callback){
    if((window != nullptr && marker != 0 && isComplete(window, marker)) || isInterruptionRequested()){
        callback();
        return;
    }
    m_completedMutex.lock();
    PendingMarkerWait pendingMarkerWait{
        .marker = marker,
        .window = window == nullptr ? "" : window->identifier(),
        .callback = callback,
        .deadline = QDeadlineTimer(1000 * 7, Qt::PreciseTimer),
    };
    m_pendingMarkerWaits.append(pendingMarkerWait);
    m_pendingtWait.notify_all();
    m_completedMutex.unlock();
}

void WaitThread::addWait(unsigned int marker, std::function<void()> callback){ addWait(nullptr, marker, callback); }

bool WaitThread::isComplete(Window* window, unsigned int marker){
    if(isInterruptionRequested()){
        return true;
    }
    QMutexLocker locker(&m_completedMutex);
    Q_UNUSED(locker);
    auto identifier = window->identifier();
    for(auto item : m_completedMarkers){
        if(item.window == identifier && item.marker == marker){
            return item.waited;
        }
    }
    return false;
}

void WaitThread::addCompleted(QString window, unsigned int marker, unsigned int internalMarker, bool waited){
    m_completedMutex.lock();
    CompletedMarker completedMarker{
        .internalMarker = internalMarker,
                .marker = marker,
                .window = window,
                .waited = waited,
                .deadline = QDeadlineTimer(1000 * 7, Qt::PreciseTimer),
                .cleanupDeadline = QDeadlineTimer(1000 * 14, Qt::PreciseTimer),
    };
    m_completedMarkers.append(completedMarker);
    m_completedMutex.unlock();
    m_pendingtWait.notify_all();
}

bool WaitThread::isPendingMarkerWaitDone(PendingMarkerWait pendingMarkerWait){
    // Marker should never be 0, but just in case
    if(pendingMarkerWait.marker == 0){
        return true;
    }
    if(pendingMarkerWait.deadline.isForever()){
        O_WARNING("Invalid MXCFB_WAIT_FOR_UPDATE_COMPLETE marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.window);
        return true;
    }
    // Somehow this marker slipped through the cracks, lets assume it's done after 7 seconds
    if(pendingMarkerWait.deadline.hasExpired()){
        O_WARNING("Stale MXCFB_WAIT_FOR_UPDATE_COMPLETE marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.window);
        return true;
    }
    QMutexLocker locker(&m_completedMutex);
    Q_UNUSED(locker);
    for(auto completedItem : m_completedMarkers){
        if(pendingMarkerWait.window != completedItem.window || pendingMarkerWait.marker != completedItem.marker){
            continue;
        }
        if(completedItem.deadline.hasExpired()){
            O_WARNING("Stale completed item for marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.window);
            completedItem.waited = true;
            return true;
        }
        if(completedItem.internalMarker == 0){
            O_WARNING("Unused MXCFB_WAIT_FOR_UPDATE_COMPLETE marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.window);
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
    O_INFO("Thread started");
    QTimer::singleShot(0, [this]{
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
            deletePendingWindows();
            eventDispatcher()->processEvents(QEventLoop::AllEvents);
            if(isInterruptionRequested()){
                O_DEBUG("Interruption requested, leaving loop");
                break;
            }
        }
    });
    auto res = exec();
    O_INFO("Stopping thread" << m_waitThread);
    m_waitThread->requestInterruption();
    m_waitThread->quit();
    QDeadlineTimer deadline(3000);
    if(!m_waitThread->wait(deadline)){
        O_WARNING("Terminated thread" << m_waitThread);
        m_waitThread->terminate();
        m_waitThread->wait();
    }
    O_INFO("Thread stopped with exit code:" << res);
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
    auto deleteLocked = m_deleteQueueMutex.tryLock(100);
    m_repaintEvents.clear();
    for(auto window : m_deleteQueue){
        if(window != nullptr){
            window->deleteLater();
        }
    }
    if(deleteLocked){
        m_deleteQueueMutex.unlock();
    }
    if(repaintLocked){
        m_repaintMutex.unlock();
    }
}

bool GUIThread::isActive(){ return m_repaintCount.available(); }

void GUIThread::enqueue(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool global, std::function<void()> callback){
    if(isInterruptionRequested()){
        callback();
        return;
    }
    Q_ASSERT(global || window != nullptr);
    m_repaintMutex.lock();
    m_repaintEvents.enqueue(RepaintRequest{
        .window = window,
        .region = region,
        .waveform = waveform,
        .marker = marker,
        .global = global,
        .callback = callback
    });
    m_repaintCount.release(); // Indicate that there is an item in the queue
    m_repaintWait.notify_all();
    m_repaintMutex.unlock();
    if(callback != nullptr){
        m_waitThread->addWait(window, marker, callback);
    }
}

void GUIThread::addCompleted(QString window, unsigned int marker, unsigned int internalMarker, bool waited){ m_waitThread->addCompleted(window, marker, internalMarker, waited); }

void GUIThread::deleteWindowLater(Window* window){
    m_deleteQueueMutex.lock();
    m_deleteQueue.append(window);
    m_deleteQueueMutex.unlock();
}

WaitThread* GUIThread::waitThread(){ return m_waitThread; }

void GUIThread::repaintWindow(QPainter* painter, QRect* rect, Window* window){
    const QRect windowGeometry = window->_geometry();
    const QRect windowRect = windowGeometry.translated(-m_screenGeometry->topLeft());
    const QRect windowIntersect = rect->translated(-windowRect.left(), -windowRect.top());
    if(windowIntersect.isEmpty()){
        return;
    }
    O_DEBUG(window->identifier() << windowIntersect << window->z());
    // TODO - See if there is a way to detect if there is just transparency in the region
    //        and don't mark this as repainted.
    painter->drawImage(*rect, window->toImage(), windowIntersect);
}

void GUIThread::redraw(RepaintRequest& event){
    Q_ASSERT(QThread::currentThread() == (QThread*)this);
    // Get the regions that need to be repainted
    const QPoint screenOffset = m_screenGeometry->topLeft();
    const QRect screenRect = m_screenGeometry->translated(-screenOffset);
    auto region = event.region;
    if(region.isEmpty()){
        O_WARNING("Empty repaint region" << region);
        return;
    }
    if(!event.global && event.window == nullptr){
        O_WARNING("Window missing");
        return;
    }
    if(event.global && inDeleteQueue(event.window)){
        O_WARNING("Window has been closed");
        return;
    }
    m_deleteQueueMutex.unlock();
    // Get visible region on the screen to repaint
    QRect rect;
    if(region.isEmpty()){
        O_WARNING("Empty repaint region provided");
        return;
    }
    if(event.global){
        rect = region;
    }else{
        auto windowGeometry = event.window->_geometry();
        rect = region.translated(windowGeometry.topLeft())
            .intersected(windowGeometry)
            .intersected(screenRect);
    }
    if(rect.isEmpty()){
        O_WARNING("Window region does not intersect with screen:" << region);
        return;
    }
    O_DEBUG("Repainting" << rect);
    // Get windows in order of Z sort order, and filter out invalid windows
    auto visibleWindows = guiAPI->sortedWindows();
    // TODO - Use STL style iterators https://doc.qt.io/qt-5/containers.html#stl-style-iterators
    QMutableListIterator i(visibleWindows);
    while(i.hasNext()){
        auto window = i.next();
        if(!window->_isVisible()){
            i.remove();
            continue;
        }
        if(getpgid(window->pgid()) < 0){
            O_WARNING(window->identifier() << "With no running process");
            // TODO - figure out how to safely trigger a close after repainting is done
            i.remove();
            continue;
        }
        auto image = window->toImage();
        if(image.isNull()){
            O_WARNING(window->identifier() << "Null framebuffer");
            i.remove();
            continue;
        }
    }
    // TODO - explore using QPainter::clipRegion to see if it can speed things up
    QRegion repaintRegion(rect);
    if(!event.global){
        // Don't repaint portions covered by another window
        auto i = visibleWindows.constEnd();
        while(i != visibleWindows.constBegin()) {
            --i;
            auto w = *i;
            if(event.window == w){
                break;
            }
            auto geometry = w->_geometry();
            repaintRegion -= region
                .intersected(geometry)
                .translated(-geometry.topLeft())
                .intersected(screenRect);
        }
    }
    auto frameBuffer = EPFrameBuffer::instance()->framebuffer();
    Qt::GlobalColor colour = frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::white;
    QPainter painter(frameBuffer);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    for(auto rect : repaintRegion){
        if(!event.global){
            repaintWindow(&painter, &rect, event.window);
        }else if(visibleWindows.isEmpty()){
            painter.fillRect(rect, colour);
        }else{
            for(auto window : visibleWindows){
                if(window != nullptr){
                    repaintWindow(&painter, &rect, window);
                }
            }
        }
    }
    painter.end();
    auto waveform = event.waveform;
    unsigned int updateMarker = 0;
    // Send updates for all the repainted regions
    for(auto rect : repaintRegion){
        // TODO - detect if there was no change to the repainted region and skip, maybe compare against previous window states?
        //        Maybe hash the data before and compare after? https://doc.qt.io/qt-5/qcryptographichash.html
        if(waveform == EPFrameBuffer::Initialize){
            waveform = EPFrameBuffer::Mono;
            // TODO - profile if it makes sense to do this instead of just picking one to always use
            for(int x = rect.left(); x < rect.right(); x++){
                for(int y = rect.top(); y < rect.bottom(); y++){
                    auto color = frameBuffer->pixelColor(x, y);
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
        m_waitThread->addCompleted(event.global ? "" : event.window->identifier(), event.marker, updateMarker, false);
    }
    O_DEBUG("Repaint" << rect << "done in" << repaintRegion.rectCount() << "paints");
}

void GUIThread::deletePendingWindows(){
    if(m_repaintCount.available() || !m_waitThread->isEmpty()){
        return;
    }
    m_deleteQueueMutex.lock();
    unsigned int count = 0;
    QMutableListIterator i(m_deleteQueue);
    while(i.hasNext()){
        auto window = i.next();
        if(!inRepaintEvents(window)){
            count++;
            i.remove();
            window->deleteLater();
        }
    }
    O_DEBUG("deleted" << count << "pending windows");
    m_deleteQueueMutex.unlock();
}

bool GUIThread::inDeleteQueue(Window* window){
    QMutexLocker locker(&m_deleteQueueMutex);
    Q_UNUSED(locker);
    return m_deleteQueue.contains(window);
}

bool GUIThread::inRepaintEvents(Window* window){
    Q_ASSERT(window != nullptr);
    QMutexLocker locker(&m_repaintMutex);
    Q_UNUSED(locker);
    for(auto event : m_repaintEvents){
        if(event.window == window){
            return true;
        }
    }
    return false;
}
