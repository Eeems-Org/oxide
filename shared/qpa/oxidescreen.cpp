#include "oxidescreen.h"

#include <QPainter>
#include <qpa/qwindowsysteminterface.h>
#include <liboxide/tarnish.h>
#include <liboxide.h>

OxideScreen::OxideScreen()
: mGeometry(deviceSettings.screenGeometry()),
  mDepth(32),
  mFormat(DEFAULT_IMAGE_FORMAT),
  mUpdatePending(false){

}

QSizeF OxideScreen::physicalSize() const{
    static const int dpi = 228;
    return QSizeF(geometry().size()) / dpi * qreal(25.4);
}
void OxideScreen::scheduleUpdate(){
    if(!mUpdatePending){
        mUpdatePending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}
void OxideScreen::setDirty(const QRect& rect){
    const QRect intersection = rect.intersected(mGeometry);
    const QPoint screenOffset = mGeometry.topLeft();
    mRepaintRegion += intersection.translated(-screenOffset);
    scheduleUpdate();
}
QWindow* OxideScreen::topWindow() const{
    for(auto window : windows()){
        auto type = window->type();
        if(type == Qt::Window || type == Qt::Dialog){
            return window;
        }
    }
    return nullptr;
}
void OxideScreen::addWindow(OxideWindow* window){
    setDirty(window->geometry());
    QWindow* w = topWindow();
    QWindowSystemInterface::handleWindowActivated(w);
}
void OxideScreen::removeWindow(OxideWindow* window){
    setDirty(window->geometry());
    QWindow* w = topWindow();
    QWindowSystemInterface::handleWindowActivated(w);
}
bool OxideScreen::event(QEvent* event){
    if(event->type() == QEvent::UpdateRequest){
        redraw();
        mUpdatePending = false;
        return true;
    }
    return QObject::event(event);
}
void OxideScreen::redraw(){
    QMutexLocker locker(&mutex);
    Q_UNUSED(locker);
    if(mRepaintRegion.isEmpty()){
        return;
    }
    auto frameBuffer = Oxide::Tarnish::frameBufferImage();
    if(frameBuffer.isNull()){
        O_WARNING(__PRETTY_FUNCTION__ << "No framebuffer");
        return;
    }
    Oxide::Tarnish::lockFrameBuffer();
    const QPoint screenOffset = mGeometry.topLeft();
    const QRect screenRect = mGeometry.translated(-screenOffset);
    QPainter painter(&frameBuffer);
    Qt::GlobalColor colour = frameBuffer.hasAlphaChannel() ? Qt::transparent : Qt::black;
    QRegion repaintedRegion;
    // Paint the regions
    // TODO - explore using QPainter::clipRegion to see if it can speed things up
    for(QRect rect : mRepaintRegion){
        rect = rect.intersected(screenRect);
        if(rect.isEmpty()){
            continue;
        }
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, colour);
        // TODO - have some sort of stack to determine which window is on top
        for(auto window : windows()){
            if(!window->isVisible()){
                continue;
            }
            const QRect windowRect = window->geometry().translated(-screenOffset);
            const QRect windowIntersect = rect.translated(-windowRect.left(), -windowRect.top());
            OxideBackingStore* backingStore = static_cast<OxideWindow*>(window->handle())->backingStore();
            if(backingStore){
                // TODO - See if there is a way to detect if there is just transparency in the region
                //        and don't mark this as repainted.
                painter.drawImage(rect, backingStore->toImage(), windowIntersect);
                repaintedRegion += windowIntersect;
            }
        }
    }
    painter.end();
    Oxide::Tarnish::unlockFrameBuffer();
    for(auto rect : repaintedRegion){
        // TODO - detect if there was no change to the repainted region and skip
        Oxide::Tarnish::screenUpdate(rect);
    }
    mRepaintRegion = QRegion();
}