#include "oxidewindow.h"
#include "oxideintegration.h"
#include <libblight.h>

OxideWindow::OxideWindow(QWindow* window) : QPlatformWindow(window), mBackingStore(nullptr){ }

OxideWindow::~OxideWindow(){ }

void OxideWindow::setBackingStore(OxideBackingStore* store){
    mBackingStore = store;
}

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
    }
    setGeometry(screen->geometry());
}

void OxideWindow::repaint(const QRegion& region){
    if(mBackingStore == nullptr){
        return;
    }
    auto buffer = mBackingStore->buffer();
    for(auto rect : region){
        OxideIntegration::instance()->connection->repaint(
            buffer,
            rect.x(),
            rect.y(),
            rect.width(),
            rect.height(),
            Blight::WaveformMode::Mono
        );
    }
}

void OxideWindow::raise(){
    if(mBackingStore == nullptr){
        return;
    }
    auto maybe = OxideIntegration::instance()
        ->connection
        ->raise(mBackingStore->buffer());
    if(maybe.has_value()){
        maybe.value()->wait();
    }
}

void OxideWindow::lower(){
    if(mBackingStore == nullptr){
        return;
    }
    auto maybe = OxideIntegration::instance()
         ->connection
         ->lower(mBackingStore->buffer());
    if(maybe.has_value()){
        maybe.value()->wait();
    }
}
