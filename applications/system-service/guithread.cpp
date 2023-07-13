#include "guithread.h"

QEvent::Type RepaintEvent::eventType = QEvent::None;

RepaintEvent::RepaintEvent(Window* window, QRect region, EPFrameBuffer::WaveformMode waveform)
: QEvent(registeredType()),
  m_window{window},
  m_region{region},
  m_waveform{waveform}
{

}

QEvent::Type RepaintEvent::registeredType(){
    if(eventType == QEvent::None){
        int generatedType = QEvent::registerEventType();
        eventType = static_cast<QEvent::Type>(generatedType);
    }
    return eventType;
}

bool GUIThread::event(QEvent* event){
    if(event->type() == RepaintEvent::eventType){
        redraw(static_cast<RepaintEvent*>(event));
        return true;
    }
    return QObject::event(event);
}

void GUIThread::run(){
    O_WARNING(__PRETTY_FUNCTION__ << "Thread started");
    int res = exec();
    O_WARNING(__PRETTY_FUNCTION__ << "Thread stopped" << res);
}

void GUIThread::repaintWindow(QPainter* painter, Window* window, QRect* rect){
    const QRect windowRect = window->_geometry().translated(-m_screenGeometry->topLeft());
    const QRect windowIntersect = rect->translated(-windowRect.left(), -windowRect.top());
    if(windowIntersect.isEmpty()){
        return;
    }
    O_DEBUG(__PRETTY_FUNCTION__ << window->identifier() << windowIntersect);
    // TODO - See if there is a way to detect if there is just transparency in the region
    //        and don't mark this as repainted.
    painter->drawImage(*rect, window->toImage(), windowIntersect);
}

void GUIThread::redraw(RepaintEvent* event){
    if(QThread::currentThread() != (QThread*)this){
        qCritical() << __PRETTY_FUNCTION__ << "Not called from GUI thread" << QThread::currentThread() << (QThread*)this;
        qFatal(" Not called from GUI thread");
        return;
    }
    // Get the regions that need to be repainted
    const QPoint screenOffset = m_screenGeometry->topLeft();
    const QRect screenRect = m_screenGeometry->translated(-screenOffset);
    auto region = event->region();
    if(region.isEmpty()){
        O_WARNING(__PRETTY_FUNCTION__ << "Empty window region" << region);
        return;
    }
    QRect rect;
    auto window = event->window();
    if(window == nullptr){
        rect = region;
    }else{
        // Get visible region on the screen to repaint
        auto geometry = window->_geometry();
        rect = region
            .intersected(geometry)
            .translated(-geometry.topLeft())
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
        Q_ASSERT(window != nullptr);
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
    if(window != nullptr){
        // Don't repaint portions covered by another window
        auto i = visibleWindows.constEnd();
        while(i != visibleWindows.constBegin()) {
            --i;
            auto w = *i;
            if(window == w){
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
        if(window != nullptr){
            repaintWindow(&painter, window, &rect);
        }else if(visibleWindows.isEmpty()){
            painter.fillRect(rect, colour);
        }else{
            for(auto window : visibleWindows){
                repaintWindow(&painter, window, &rect);
            }
        }
    }
    painter.end();
    auto waveform = event->waveform();
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
    emit m_repaintNotifier->repainted();
    O_DEBUG(__PRETTY_FUNCTION__ << "Repaint done");
}
