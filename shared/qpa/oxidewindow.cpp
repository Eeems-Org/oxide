#include "oxidewindow.h"

#include <libblight.h>
#include <libblight/connection.h>
#include <libblight/types.h>

#include "oxideintegration.h"

OxideWindow::OxideWindow(QWindow* window)
  : QPlatformWindow(window)
  , mBackingStore(nullptr) {}

OxideWindow::~OxideWindow() {}

void
OxideWindow::setBackingStore(OxideBackingStore* store) {
  mBackingStore = store;
}

OxideBackingStore*
OxideWindow::backingStore() const {
  return mBackingStore;
}

OxideScreen*
OxideWindow::platformScreen() const {
  auto window = this->window();
  if (window == nullptr) {
    return nullptr;
  }
  auto screen = window->screen();
  if (screen == nullptr) {
    return nullptr;
  }
  auto handle = screen->handle();
  if (handle == nullptr) {
    return nullptr;
  }
  return static_cast<OxideScreen*>(handle);
}

void
OxideWindow::setGeometry(const QRect& rect) {
  QPlatformWindow::setGeometry(rect);
  if (mBackingStore == nullptr) {
    return;
  }
  auto buffer = mBackingStore->buffer();
  if (buffer == nullptr) {
    return;
  }
  if (
    OxideIntegration::instance()->options().testFlag(OxideIntegration::DebugQPA)
  ) {
    qDebug() << "OxideWindow::setGeometry:" << buffer->surface << geometry()
             << rect;
  }
  Blight::connection()->move(buffer, rect.x(), rect.y());
}

void
OxideWindow::setVisible(bool visible) {
  QPlatformWindow::setVisible(visible);
  auto screen = platformScreen();
  if (screen != nullptr) {
    if (visible) {
      screen->addWindow(this);
      raise();
    } else {
      screen->removeWindow(this);
      lower();
    }
  }
}

void
OxideWindow::raise() {
  if (mBackingStore == nullptr) {
    return;
  }
  auto buffer = mBackingStore->buffer();
  if (buffer == nullptr) {
    return;
  }
  if (
    OxideIntegration::instance()->options().testFlag(OxideIntegration::DebugQPA)
  ) {
    qDebug() << "OxideWindow::raise" << buffer->surface;
  }
  auto maybe = Blight::connection()->raise(buffer);
  if (maybe.has_value()) {
    maybe.value()->wait();
  }
}

void
OxideWindow::lower() {
  if (mBackingStore == nullptr) {
    return;
  }
  auto buffer = mBackingStore->buffer();
  if (buffer == nullptr) {
    return;
  }
  if (
    OxideIntegration::instance()->options().testFlag(OxideIntegration::DebugQPA)
  ) {
    qDebug() << "OxideWindow::lower" << buffer->surface;
  }
  auto maybe = Blight::connection()->lower(buffer);
  if (maybe.has_value()) {
    maybe.value()->wait();
  }
}

bool
OxideWindow::close() {
  if (QPlatformWindow::close()) {
    if (mBackingStore != nullptr) {
      auto buffer = mBackingStore->buffer();
      if (buffer != nullptr) {
        Blight::connection()->remove(buffer);
      }
    }
    return true;
  }
  return false;
}
