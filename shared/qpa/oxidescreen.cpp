#include "oxidescreen.h"

#include <QPainter>
#include <qpa/qwindowsysteminterface.h>
#include <liboxide/tarnish.h>

OxideScreen::OxideScreen()
: mGeometry(0, 0, 1404, 1872),
  mDepth(32),
  mFormat(QImage::Format_RGB16),
  mUpdatePending(false){
    qDebug() << "OxideScreen::OxideScreen";
}

QSizeF OxideScreen::physicalSize() const{
    static const int dpi = 228;
    return QSizeF(geometry().size()) / dpi * qreal(25.4);
}
void OxideScreen::scheduleUpdate(){
    qDebug() << "OxideScreen::scheduleUpdate";
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
        qDebug() << "OxideScreen::event::update";
        if(frameBuffer == nullptr){
            Oxide::Tarnish::createFrameBuffer(mGeometry.width(), mGeometry.height());
            frameBuffer = Oxide::Tarnish::frameBufferImage();
        }
        if(frameBuffer == nullptr){
            qDebug() << "OxideScreen::event::update No framebuffer";
            return true;
        }
        redraw();
        mUpdatePending = false;
        return true;
    }
    return QObject::event(event);
}
void OxideScreen::redraw(){
    const QPoint screenOffset = mGeometry.topLeft();
    if(mRepaintRegion.isEmpty()){
        return;
    }
    const QRect screenRect = mGeometry.translated(-screenOffset);
    if(!mPainter){
        mPainter = new QPainter(frameBuffer);
    }
    for(QRect rect : mRepaintRegion){
        rect = rect.intersected(screenRect);
        if(rect.isEmpty()){
            continue;
        }
        mPainter->setCompositionMode(QPainter::CompositionMode_Source);
        mPainter->fillRect(rect, frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::black);
        // TODO - have some sort of stack to determine which window is on top
        for(auto window : windows()){
            if(!window->isVisible()){
                continue;
            }
            const QRect windowRect = window->geometry().translated(-screenOffset);
            const QRect windowIntersect = rect.translated(-windowRect.left(), -windowRect.top());
            OxideBackingStore* backingStore = static_cast<OxideWindow*>(window->handle())->backingStore();
            if(backingStore){
                // TODO - lock backing store
                mPainter->drawImage(rect, backingStore->toImage(), windowIntersect);
            }
        }
    }
    // TODO - only do full redraw if !frameBuffer->hasAlphaChannel
    //        instead do partial redraws for every image drawn
    //        The performance of this will need to be verified
    Oxide::Tarnish::screenUpdate(mRepaintRegion.boundingRect());
    mRepaintRegion = QRegion();
}
