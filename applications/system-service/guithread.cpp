#include "guithread.h"

bool GUIThread::event(QEvent* event){
    if(event->type() != QEvent::UpdateRequest){
        return QObject::event(event);
    }
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    if(!m_processing){
        m_processing = true;
        startTimer(0, Qt::PreciseTimer);
    }
    return true;
}

void GUIThread::timerEvent(QTimerEvent* event){
    killTimer(event->timerId());
    forever{
        m_mutex.lock();
        if(m_repaintEvents.isEmpty()){
            m_mutex.unlock();
            break;
        }
        QQueue<RepaintRequest> events;
        while(!m_repaintEvents.isEmpty()){
            events.enqueue(m_repaintEvents.dequeue());
        }
        m_mutex.unlock();
        while(!events.isEmpty()){
            auto event = events.dequeue();
            redraw(event);
        }
        m_mutex.lock();
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
        m_mutex.unlock();
    }
    m_processing = false;
}

GUIThread::GUIThread() : QThread(){
    connect(this, &QThread::started, []{ O_WARNING(__PRETTY_FUNCTION__ << "Thread started"); });
    connect(this, &QThread::finished, []{ O_WARNING(__PRETTY_FUNCTION__ << "Thread stopped"); });
    setObjectName("gui");
    moveToThread(this);
}

void GUIThread::flush(){
    QMutexLocker locker(&m_mutex);
    if(!active()){
        return;
    }
    QEventLoop loop;
    QMetaObject::Connection conn;
    conn = QObject::connect(m_repaintNotifier, &RepaintNotifier::windowRepainted, [&loop, &conn, this](Window* window, unsigned int marker){
        Q_UNUSED(window)
        Q_UNUSED(marker);
        QMutexLocker locker(&m_mutex);
        if(!active()){
            QObject::disconnect(conn);
            loop.exit();
        }
    });
    locker.unlock();
    loop.exec();
}

bool GUIThread::active(){ return !m_repaintEvents.isEmpty() || m_processing; }

void GUIThread::enqueue(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool global){
    Q_ASSERT(global || window != nullptr);
    if(QThread::currentThread() != (QThread*)this){
        QMetaObject::invokeMethod(
            this,
            "enqueue",
            Qt::QueuedConnection,
            Q_ARG(Window*, window),
            Q_ARG(QRect, region),
            Q_ARG(EPFrameBuffer::WaveformMode, waveform),
            Q_ARG(unsigned int, marker),
            Q_ARG(bool, global)
        );
        return;
    }
    m_mutex.lock();
    m_repaintEvents.enqueue(RepaintRequest{
        .window = window,
        .region = region,
        .waveform = waveform,
        .marker = marker,
        .global = global
    });
    m_mutex.unlock();
    QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest), Qt::HighEventPriority);
}

void GUIThread::repaintWindow(QPainter* painter, QRect* rect, Window* window){
    const QRect windowGeometry = window->_geometry();
    const QRect windowRect = windowGeometry.translated(-m_screenGeometry->topLeft());
    const QRect windowIntersect = rect->translated(-windowRect.left(), -windowRect.top());
    if(windowIntersect.isEmpty()){
        return;
    }
    O_DEBUG(__PRETTY_FUNCTION__ << window->identifier() << windowIntersect << window->z());
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
        O_WARNING(__PRETTY_FUNCTION__ << "Empty repaint region" << region);
        return;
    }
    if(!event.global && event.window == nullptr){
        O_WARNING(__PRETTY_FUNCTION__ << "Window missing");
        return;
    }
    if(event.global && m_deleteQueue.contains(event.window)){
        O_WARNING(__PRETTY_FUNCTION__ << "Window has been closed");
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
        O_WARNING(__PRETTY_FUNCTION__ << "Window region does not intersect with screen");
        return;
    }
    O_WARNING(__PRETTY_FUNCTION__ << "Repainting" << rect);
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
            O_WARNING(__PRETTY_FUNCTION__ << window->identifier() << "With not running process");
            QMetaObject::invokeMethod(window, &Window::close, Qt::QueuedConnection);
            i.remove();
            continue;
        }
        auto image = window->toImage();
        if(image.isNull()){
            O_WARNING(__PRETTY_FUNCTION__ << window->identifier() << "Null framebuffer");
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
        O_DEBUG(__PRETTY_FUNCTION__ << "Sending screen update" << rect << waveform << mode);
        EPFrameBuffer::sendUpdate(rect, waveform, mode);
    }
    EPFrameBuffer::waitForLastUpdate();
    emit m_repaintNotifier->windowRepainted(event.window, event.marker);
    O_DEBUG(__PRETTY_FUNCTION__ << "Repaint" << rect << "done");
}
