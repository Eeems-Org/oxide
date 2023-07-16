#include "oxidewindow.h"
#include <liboxide/tarnish.h>

OxideScreen* OxideWindow::platformScreen() const{ return static_cast<OxideScreen*>(window()->screen()->handle()); }

void OxideWindow::setVisible(bool visible){
    QPlatformWindow::setVisible(visible);
    auto screen = platformScreen();
    if(visible){
        screen->addWindow(this);
    }else{
        screen->removeWindow(this);
    }
    setGeometry(screen->geometry());
}

void OxideWindow::repaint(const QRegion& region){
    const QRect currentGeometry = geometry();
    const QRect oldGeometryLocal = mOldGeometry;
    mOldGeometry = currentGeometry;
    // If this is a move, redraw the previous location
    if(oldGeometryLocal != currentGeometry){
        platformScreen()->setDirty(oldGeometryLocal);
    }
    auto topLeft = currentGeometry.topLeft();
    for(auto rect : region){
        platformScreen()->setDirty(rect.translated(topLeft));
    }
}

bool OxideWindow::close(){
    if(platformScreen()->topPlatformWindow() == this){
        Oxide::Tarnish::topWindow()->close(); // TODO - replace with event socket call
    }
    return true;
}

void OxideWindow::raise(){
    if(platformScreen()->topPlatformWindow() == this){
        Oxide::Tarnish::topWindow()->raise(); // TODO - replace with event socket call
    }
}

void OxideWindow::lower(){
    if(platformScreen()->topPlatformWindow() == this){
        Oxide::Tarnish::topWindow()->lower(); // TODO - replace with event socket call
    }
}

void OxideWindow::setGeometry(const QRect& rect){
    if(platformScreen()->topPlatformWindow() == this){
        Oxide::Tarnish::topWindow()->setGeometry(rect); // TODO - replace with event socket call
    }
}
