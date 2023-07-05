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
    if(!mUpdatePending){
        qDebug() << "OxideScreen::scheduleUpdate";
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
        if(Oxide::Tarnish::frameBufferImage() == nullptr){
            Oxide::Tarnish::createFrameBuffer(mGeometry.width(), mGeometry.height());
        }
        redraw();
        mUpdatePending = false;
        return true;
    }
    return QObject::event(event);
}
void OxideScreen::redraw(){
    if(mRepaintRegion.isEmpty()){
        qDebug() << "OxideScreen::redraw No updates";
        return;
    }
    if(Oxide::Tarnish::frameBufferImage() == nullptr){
        qDebug() << "OxideScreen::redraw No framebuffer";
        return;
    }
    const QPoint screenOffset = mGeometry.topLeft();
    const QRect screenRect = mGeometry.translated(-screenOffset);
    qDebug() << "OxideScreen::redraw::QPainter";
    QPainter painter(Oxide::Tarnish::frameBufferImage());
    Qt::GlobalColor colour = Oxide::Tarnish::frameBufferImage()->hasAlphaChannel() ? Qt::transparent : Qt::black;
    qDebug() << "OxideScreen::redraw::mRepaintRegion";
    for(QRect rect : mRepaintRegion){
        rect = rect.intersected(screenRect);
        if(rect.isEmpty()){
            continue;
        }
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, colour);
        // TODO - have some sort of stack to determine which window is on top
        qDebug() << "OxideScreen::redraw::windows" << rect;
        for(auto window : windows()){
            if(!window->isVisible()){
                continue;
            }
            const QRect windowRect = window->geometry().translated(-screenOffset);
            const QRect windowIntersect = rect.translated(-windowRect.left(), -windowRect.top());
            qDebug() << "OxideScreen::redraw::backingStore";
            OxideBackingStore* backingStore = static_cast<OxideWindow*>(window->handle())->backingStore();
            if(backingStore){
                // TODO - lock backing store
                qDebug() << "OxideScreen::redraw::QPainter::drawImage" << windowIntersect;
                painter.drawImage(rect, backingStore->toImage(), windowIntersect);
            }
        }
    }
    qDebug() << "OxideScreen::redraw::QPainter::end";
    painter.end();
    qDebug() << "OxideScreen::redraw::sendUpdate" << mRepaintRegion.boundingRect();
    // TODO - only do full redraw if !frameBuffer->hasAlphaChannel
    //        instead do partial redraws for every image drawn
    //        The performance of this will need to be verified
    Oxide::Tarnish::screenUpdate(mRepaintRegion.boundingRect());
    mRepaintRegion = QRegion();
}
