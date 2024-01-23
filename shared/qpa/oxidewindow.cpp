#include "oxidewindow.h"

OxideWindow::OxideWindow(QWindow* window) : QPlatformWindow(window), mBackingStore(nullptr){ }

OxideWindow::~OxideWindow(){ }

void OxideWindow::setBackingStore(OxideBackingStore* store) { mBackingStore = store; }

OxideBackingStore* OxideWindow::backingStore() const { return mBackingStore; }

OxideScreen* OxideWindow::platformScreen() const{
    auto window = this->window();
    if(window == nullptr){
        return nullptr;
    }
    auto screen = window->screen();
    if(screen == nullptr){
        return nullptr;
    }
    auto handle = screen->handle();
    if(handle == nullptr){
        return nullptr;
    }
    return static_cast<OxideScreen*>(handle);
}

void OxideWindow::setVisible(bool visible){
    QPlatformWindow::setVisible(visible);
    auto screen = platformScreen();
    if(screen != nullptr){
        if(visible){
            screen->addWindow(this);
        }else{
            screen->removeWindow(this);
        }
        if(screen->topPlatformWindow() == this){
            auto window = Oxide::Tarnish::topWindow();
            if(window != nullptr){
                window->setVisible(visible); // TODO - replace with event socket call
            }
        }
    }
    setGeometry(screen->geometry());
}

void OxideWindow::repaint(const QRegion& region){
    auto screen = platformScreen();
    if(screen == nullptr){
        return;
    }
    const QRect currentGeometry = geometry();
    const QRect oldGeometryLocal = mOldGeometry;
    mOldGeometry = currentGeometry;
    // If this is a move, redraw the previous location
    if(oldGeometryLocal != currentGeometry){
        screen->setDirty(oldGeometryLocal);
    }
    auto topLeft = currentGeometry.topLeft();
    for(auto rect : region){
        screen->setDirty(rect.translated(topLeft));
    }
}

bool OxideWindow::close(){
    if(isTopWindow()){
        auto window = Oxide::Tarnish::topWindow();
        if(window != nullptr){
            // TODO - replace with event socket call
            window->close();
        }
    }
    return QPlatformWindow::close();
}

void OxideWindow::raise(){
    if(isTopWindow()){
        auto window = Oxide::Tarnish::topWindow();
        if(window != nullptr){
            // TODO - replace with event socket call
            window->raise();
        }
    }
}

void OxideWindow::lower(){
    if(isTopWindow()){
        auto window = Oxide::Tarnish::topWindow();
        if(window != nullptr){
            // TODO - replace with event socket call
            window->lower();
        }
    }
}

bool OxideWindow::isTopWindow(){
    auto screen = platformScreen();
    return screen != nullptr && screen->topPlatformWindow() == this;
}
