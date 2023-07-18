#include "oxidescreen.h"

#include <QPainter>
#include <qpa/qwindowsysteminterface.h>
#include <liboxide/tarnish.h>
#include <liboxide.h>

OxideScreen::OxideScreen()
: mGeometry(deviceSettings.screenGeometry()),
  mDepth(32),
  mFormat(DEFAULT_IMAGE_FORMAT),
  mUpdatePending(false),
  m_marker{0}
{}

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

OxideWindow* OxideScreen::topPlatformWindow() const{ return m_windows.isEmpty() ? nullptr : m_windows.first(); }

void OxideScreen::addWindow(OxideWindow* window){
    m_windows.append(window);
    setDirty(window->geometry());
    window->requestActivateWindow();
    if(m_windows.length() == 1){
        raiseTopWindow();
    }
}
void OxideScreen::removeWindow(OxideWindow* window){
    m_windows.removeAll(window);
    setDirty(window->geometry());
    window->requestActivateWindow();
}

void OxideScreen::raiseTopWindow(){
    auto window = Oxide::Tarnish::topWindow();
    if(window != nullptr){
        window->raise(); // TODO - replace with event socket call
    }
}

void OxideScreen::lowerTopWindow(){
    auto window = Oxide::Tarnish::topWindow();
    if(window != nullptr){
        window->lower(); // TODO - replace with event socket call
    }
}

void OxideScreen::closeTopWindow(){
    auto window = Oxide::Tarnish::topWindow();
    if(window != nullptr){
        window->close(); // TODO - replace with event socket call
    }
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
    if(mRepaintRegion.isEmpty()){
        return;
    }
    auto frameBuffer = Oxide::Tarnish::frameBufferImage();
    if(frameBuffer.isNull()){
        O_WARNING("No framebuffer");
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
        // TODO - detect if there was no change to the repainted region and skip, maybe compare against previous window states?
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
        auto waveform = EPFrameBuffer::Mono;
        for(int x = rect.left(); x < rect.right(); x++){
            for(int y = rect.top(); y < rect.bottom(); y++){
                auto color = frameBuffer.pixelColor(x, y);
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
        Oxide::Tarnish::screenUpdate(rect, waveform, ++m_marker);
    }
    mRepaintRegion = QRegion();
}
