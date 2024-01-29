#include "oxidescreen.h"

#include <QPainter>
#include <qpa/qwindowsysteminterface.h>
#include <liboxide.h>

OxideScreen::OxideScreen() {}

QRect OxideScreen::geometry() const { return m_geometry; }

static QTransform m_tabletTransform = QTransform::fromTranslate(0.5, 0.5).rotate(90).translate(-0.5, -0.5);

void OxideScreen::setGeometry(QRect geometry){
    if(m_geometry != geometry){
        m_geometry = geometry;
        qDebug() << "Screen geometry set to:" << geometry << screen();
        QWindowSystemInterface::handleScreenGeometryChange(screen(), geometry, geometry);
    }
}

int OxideScreen::depth() const { return 16; }

QImage::Format OxideScreen::format() const { return QImage::Format_RGB16; }

QSizeF OxideScreen::physicalSize() const{
    static const int dpi = 228;
    return QSizeF(geometry().size()) / dpi * qreal(25.4);
}

Qt::ScreenOrientation OxideScreen::orientation() const{
    // TODO - allow rotation
    return Qt::PortraitOrientation;
}

Qt::ScreenOrientation OxideScreen::nativeOrientation() const{ return Qt::PortraitOrientation; }

QString OxideScreen::name() const{ return "Oxide"; }

void OxideScreen::addWindow(OxideWindow* window){
    m_windows.append(window);
    window->requestActivateWindow();
    if(m_windows.length() == 1){
        window->raise();
    }
}
void OxideScreen::removeWindow(OxideWindow* window){
    m_windows.removeAll(window);
    window->requestActivateWindow();
}

OxideWindow* OxideScreen::getWindow(const WId& winId){
    for(auto& window : m_windows){
        if(window->winId() == winId){
            return window;
        }
    }
    return nullptr;
}
