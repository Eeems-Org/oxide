#include "oxidescreen.h"

#include <QPainter>
#include <qpa/qwindowsysteminterface.h>
#include <liboxide/tarnish.h>
#include <liboxide.h>

OxideScreen::OxideScreen()
: m_depth(32),
  m_format(DEFAULT_IMAGE_FORMAT),
  mUpdatePending(false),
  m_marker{0}
{}

QRect OxideScreen::tabletGeometry() const{ return m_tabletGeometry; }

QRect OxideScreen::geometry() const { return m_geometry; }

static QTransform m_tabletTransform = QTransform::fromTranslate(0.5, 0.5).rotate(90).translate(-0.5, -0.5);

void OxideScreen::setGeometry(QRect geometry){
    if(m_geometry != geometry){
        m_geometry = geometry;
        auto screenGeometry = deviceSettings.screenGeometry();
        QRectF tabletGeometry = m_tabletTransform.mapRect(Oxide::Math::normalize(geometry, screenGeometry));
        m_tabletGeometry.setLeft(tabletGeometry.left() * screenGeometry.width());
        m_tabletGeometry.setTop(tabletGeometry.top() * screenGeometry.height());
        m_tabletGeometry.setRight(tabletGeometry.right() * screenGeometry.width());
        m_tabletGeometry.setBottom(tabletGeometry.bottom() * screenGeometry.height());
        qDebug() << "Screen geometry set to:" << geometry << screen();
        QWindowSystemInterface::handleScreenGeometryChange(screen(), geometry, geometry);
    }
}

int OxideScreen::depth() const { return m_depth; }

QImage::Format OxideScreen::format() const { return m_format; }

QSizeF OxideScreen::physicalSize() const{
    static const int dpi = 228;
    return QSizeF(geometry().size()) / dpi * qreal(25.4);
}

Qt::ScreenOrientation OxideScreen::orientation() const{ return Qt::PortraitOrientation; }

Qt::ScreenOrientation OxideScreen::nativeOrientation() const{ return Qt::PortraitOrientation; }

QString OxideScreen::name() const{
    auto window = Oxide::Tarnish::topWindow();
    return window != nullptr ? window->identifier() : "";
}

void OxideScreen::scheduleUpdate(){
    if(!mUpdatePending){
        mUpdatePending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

void OxideScreen::setDirty(const QRect& rect){
    const QRect intersection = rect.intersected(m_geometry);
    const QPoint screenOffset = m_geometry.topLeft();
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
        window->raise();
    }
}
void OxideScreen::removeWindow(OxideWindow* window){
    m_windows.removeAll(window);
    setDirty(window->geometry());
    window->requestActivateWindow();
}

void OxideScreen::raiseTopWindow(){
    auto window = topPlatformWindow();
    if(window != nullptr){
        window->raise();
    }
}

void OxideScreen::lowerTopWindow(){
    auto window = topPlatformWindow();
    if(window != nullptr){
        window->lower();
    }
}

void OxideScreen::closeTopWindow(){
    auto window = topPlatformWindow();
    if(window != nullptr){
        window->close();
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
    const QPoint screenOffset = m_geometry.topLeft();
    // TODO - determine if this logic actually works?
    const QRect screenRect = m_geometry.translated(-screenOffset);
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
        //        Maybe hash the data before and compare after? https://doc.qt.io/qt-5/qcryptographichash.html
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        // TODO - don't wipe the area, and instead clear it only if nothing was painted over it
        painter.fillRect(rect, colour);
        // TODO - have some sort of stack to determine which window is on top
        for(auto window : windows()){
            if(!window->isVisible()){
                continue;
            }
            // TODO - determine if this logic actually works?
            const QRect windowRect = window->geometry().translated(-screenOffset);
            const QRect windowIntersect = rect.translated(-windowRect.left(), -windowRect.top());
            OxideBackingStore* backingStore = static_cast<OxideWindow*>(window->handle())->backingStore();
            if(backingStore){
                // TODO - See if there is a way to detect if there is just transparency in the region
                //        and don't mark this as repainted
                painter.drawImage(rect, backingStore->toImage(), windowIntersect);
                repaintedRegion += windowIntersect;
            }
        }
    }
    painter.end();
    Oxide::Tarnish::unlockFrameBuffer();
    for(auto rect : repaintedRegion){
        // TODO - detect if there was no change to the repainted region and skip
        //        Maybe hash the data before and compare after? https://doc.qt.io/qt-5/qcryptographichash.html
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
