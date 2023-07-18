#include "guithread.h"
#include "dbusservice.h"

bool WaitThread::event(QEvent* event){
    if(event->type() != QEvent::UpdateRequest){
        return QObject::event(event);
    }
    if(!m_processing){
        m_processing = true;
        QQueue<std::function<void()>> callbacks;
        forever{
            auto pendingMarkerWaits = getPendingMarkers();
            auto completedMarkers = getCompletedMarkers();
            if(pendingMarkerWaits.isEmpty() && completedMarkers.isEmpty()){
                break;
            }
            QMutableListIterator i(pendingMarkerWaits);
            while(i.hasNext()){
                PendingMarkerWait pendingMarkerWait = i.next();
                if(isPendingMarkerWaitDone(pendingMarkerWait)){
                    if(pendingMarkerWait.window.isEmpty()){
                        O_DEBUG("Global marker wait completed" << pendingMarkerWait.marker);
                    }else{
                        O_DEBUG("Window marker wait completed" << pendingMarkerWait.window << pendingMarkerWait.marker);
                    }
                    i.remove();
                    callbacks.append(pendingMarkerWait.callback);
                }
            }
            while(!callbacks.isEmpty()){
                auto callback = callbacks.dequeue();
                callback();
            }
            purgeOldCompletedItems();
            m_completedMutex.lock();
            m_pendingMarkerWaits.append(pendingMarkerWaits);
            m_completedMutex.unlock();
        }
        m_processing = false;
    }
    return true;
}

WaitThread::WaitThread(int frameBufferFd) : QThread(), m_frameBufferFd{frameBufferFd}, m_processing{false}{
    setObjectName("ioctl");
    moveToThread(this);
}

WaitThread::~WaitThread(){
    m_completedMutex.lock();
    for(auto pendingWaitMarker : m_pendingMarkerWaits){
        pendingWaitMarker.callback();
    }
    m_pendingMarkerWaits.clear();
    m_completedMarkers.clear();
    m_completedMutex.unlock();
}

bool WaitThread::isPendingMarkerWaitDone(PendingMarkerWait pendingMarkerWait){
    // Marker should never be 0, but just in case
    if(pendingMarkerWait.marker == 0){
        return true;
    }
    // Somehow this marker slipped through the cracks, lets assume it's done after 7 seconds
    if(pendingMarkerWait.age.hasExpired(1000 * 7)){
        O_WARNING("Stale MXCFB_WAIT_FOR_UPDATE_COMPLETE marker" << pendingMarkerWait.marker << "for window" << pendingMarkerWait.window);
        return true;
    }
    auto completedMarkers = getCompletedMarkers();
    for(auto completedItem : completedMarkers){
        if(pendingMarkerWait.window != completedItem.window || pendingMarkerWait.marker != completedItem.marker){
            continue;
        }
        if(completedItem.age.hasExpired(1000*7)){
            completedItem.waited = true;
            return true;
        }
        if(!completedItem.waited && completedItem.internalMarker != 0){
            O_DEBUG("Waiting for" << pendingMarkerWait.marker << "marker");
            mxcfb_update_marker_data data{
                .update_marker = completedItem.internalMarker,
                        .collision_test = 0,
            };
            ioctl(m_frameBufferFd, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &data);
        }
        return true;
    }
    return false;
}

void WaitThread::purgeOldCompletedItems(){
    QMutexLocker locker(&m_completedMutex);
    Q_UNUSED(locker);
    QMutableListIterator i(m_completedMarkers);
    // Remove completed items that are older than 7 seconds
    while(i.hasNext()){
        CompletedMarker completedItem = i.next();
        // Remove if older than 14 seconds
        if(completedItem.age.hasExpired(1000*14)){
            i.remove();
        }
    }
}

QList<PendingMarkerWait> WaitThread::getPendingMarkers(){
    QMutexLocker locker(&m_completedMutex);
    Q_UNUSED(locker);
    QQueue<PendingMarkerWait> pendingMarkerWaits;
    while(!m_pendingMarkerWaits.isEmpty()){
        pendingMarkerWaits.enqueue(m_pendingMarkerWaits.dequeue());
    }
    return pendingMarkerWaits;
}

QQueue<CompletedMarker> WaitThread::getCompletedMarkers(){
    QMutexLocker locker(&m_completedMutex);
    Q_UNUSED(locker);
    QQueue<CompletedMarker> completedMarkers;
    for(auto marker : m_completedMarkers){
        completedMarkers.enqueue(marker);
    }
    return completedMarkers;
}

bool GUIThread::event(QEvent* event){
    if(event->type() != QEvent::UpdateRequest){
        return QObject::event(event);
    }
    if(!m_processing){
        m_processing = true;
        forever{
            auto events = getRepaintEvents();
            if(events.isEmpty()){
                break;
            }
            while(!events.isEmpty()){
                auto event = events.dequeue();
                redraw(event);
            }
        }
        deletePendingWindows();
        m_processing = false;
    }
    QCoreApplication::postEvent(m_waitThread, new QEvent(QEvent::UpdateRequest));
    return true;
}

void GUIThread::run(){
    O_INFO("Thread started");
    auto res = exec();
    O_INFO("Stopping thread" << m_waitThread);
    m_waitThread->quit();
    QDeadlineTimer deadline(1000);
    if(!m_waitThread->wait(deadline)){
        O_WARNING("Terminated thread" << m_waitThread);
        m_waitThread->terminate();
        m_waitThread->wait();
    }
    O_INFO("Thread stopped with exit code:" << res);
}

GUIThread::GUIThread() : QThread(), m_processing{false}{
    m_frameBufferFd = open("/dev/fb0", O_RDWR);
    if(m_frameBufferFd == -1){
        qFatal("Failed to open framebuffer");
    }
    m_waitThread = new WaitThread(m_frameBufferFd);
    setObjectName("gui");
    moveToThread(this);
    Oxide::startThreadWithPriority(m_waitThread, priority());
}

GUIThread::~GUIThread(){
    m_mutex.lock();
    m_processing = true;
    m_repaintEvents.clear();
    for(auto window : m_deleteQueue){
        if(window != nullptr){
            window->deleteLater();
        }
    }
    m_mutex.unlock();
}

bool GUIThread::isActive(){ return !m_repaintEvents.isEmpty() || m_processing; }

void GUIThread::enqueue(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool global){
    Q_ASSERT(global || window != nullptr);
    m_mutex.lock();
    m_repaintEvents.enqueue(RepaintRequest{
        .window = window,
        .region = region,
        .waveform = waveform,
        .marker = marker,
        .global = global
    });
    m_mutex.unlock();
    scheduleUpdate();
}

void GUIThread::addWait(Window* window, unsigned int marker, std::function<void ()> callback){
    if(window != nullptr && marker != 0 && isComplete(window, marker)){
        callback();
        return;
    }
    m_waitThread->m_completedMutex.lock();
    PendingMarkerWait pendingMarkerWait{
        .marker = marker,
        .window = window == nullptr ? "" : window->identifier(),
        .callback = callback,
        .age = QElapsedTimer(),
    };
    m_waitThread->m_pendingMarkerWaits.append(pendingMarkerWait);
    pendingMarkerWait.age.start();
    m_waitThread->m_completedMutex.unlock();
    scheduleUpdate();
}

void GUIThread::addWait(unsigned int marker, std::function<void()> callback){ addWait(nullptr, marker, callback); }

bool GUIThread::isComplete(Window* window, unsigned int marker){
    QMutexLocker locker(&m_waitThread->m_completedMutex);
    Q_UNUSED(locker);
    auto identifier = window->identifier();
    for(auto item : m_waitThread->m_completedMarkers){
        if(item.window == identifier && item.marker == marker){
            return item.waited;
        }
    }
    return false;
}

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
    if(event.global && m_deleteQueue.contains(event.window)){
        O_WARNING("Window has been closed");
        return;
    }
    auto windowGeometry = event.window->_geometry();
    // Get visible region on the screen to repaint
    QRect rect;
    if(event.global){
        rect = region;
    }else{
        rect = region
            .intersected(windowGeometry)
            .translated(-windowGeometry.topLeft())
            .intersected(screenRect);
    }
    if(rect.isEmpty()){
        O_WARNING("Window region does not intersect with screen");
        return;
    }
    O_WARNING("Repainting" << rect);
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
            O_WARNING(window->identifier() << "With not running process");
            QMetaObject::invokeMethod(window, &Window::close, Qt::QueuedConnection);
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
    for(auto rect : repaintRegion){
        painter.setCompositionMode(QPainter::CompositionMode_Source);
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
        m_waitThread->m_completedMutex.lock();
        CompletedMarker completedMarker{
            .internalMarker = updateMarker,
            .marker = event.marker,
            .window = event.global ? "" : event.window->identifier(),
            .age = QElapsedTimer(),
        };
        m_waitThread->m_completedMarkers.append(completedMarker);
        completedMarker.age.start();
        m_waitThread->m_completedMutex.unlock();
        QCoreApplication::postEvent(m_waitThread, new QEvent(QEvent::UpdateRequest));
    }
    O_DEBUG("Repaint" << rect << "done");
}

QQueue<RepaintRequest> GUIThread::getRepaintEvents(){
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    QQueue<RepaintRequest> events;
    while(!m_repaintEvents.isEmpty()){
        events.enqueue(m_repaintEvents.dequeue());
    }
    return events;
}

void GUIThread::deletePendingWindows(){
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    if(!m_repaintEvents.isEmpty()){
        return;
    }
    O_DEBUG("Deleting pending windows");
    while(!m_deleteQueue.isEmpty()){
        auto window = m_deleteQueue.first();
        Q_ASSERT(window != nullptr);
        bool skip = false;
        for(auto event : m_repaintEvents){
            if(event.window == window){
                skip = true;
                break;
            }
        }
        if(!skip){
            m_deleteQueue.dequeue();
            window->deleteLater();
        }
    }
}

void GUIThread::scheduleUpdate(){
    QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    QCoreApplication::postEvent(m_waitThread, new QEvent(QEvent::UpdateRequest));
}
