#include "guithread.h"
#ifdef EPAPER
#include <epframebuffer.h>
#include <fcntl.h>
#include <libblight/clock.h>
#include <liboxide/debug.h>
#include <liboxide/devicesettings.h>
#include <liboxide/oxideqml.h>
#include <liboxide/threading.h>
#include <mxcfb.h>
#include <sys/mman.h>

#include <QAbstractEventDispatcher>
#include <QPainter>
#include <QTimer>

#include "connection.h"
#include "dbusinterface.h"

void
GUIThread::run() {
  O_DEBUG("Thread started");
  clearFrameBuffer();
  QTimer::singleShot(0, this, [this] {
    Q_ASSERT(QThread::currentThread() == (QThread*)this);
    QMutexLocker locker(&m_repaintMutex);
    Q_UNUSED(locker);
    while (!isInterruptionRequested()) {
      // New repaint request each loop as we have a shared pointer we need
      // to clear
      RepaintRequest event;
      if (!m_repaintEvents.try_dequeue(event)) {
        emit settled();
        // Wait for up to 500ms before trying again
        m_repaintWait.wait(&m_repaintMutex, 500);
        auto found = m_repaintEvents.try_dequeue(event);
        if (!found) {
          // Woken by something needing to cleanup
          // connections/surfaces
          continue;
        }
      }
      do {
        redraw(event);
        if (event.callback != nullptr) {
          event.callback();
        }
      } while (m_repaintEvents.try_dequeue(event));
      eventDispatcher()->processEvents(QEventLoop::AllEvents);
    }
  });
  clearFrameBuffer();
  auto res = exec();
  O_DEBUG("Thread stopped with exit code:" << res);
}

GUIThread*
GUIThread::singleton() {
  static GUIThread* instance = nullptr;
  if (instance == nullptr) {
    instance = new GUIThread(getFrameBuffer()->rect());
    Oxide::startThreadWithPriority(instance, QThread::TimeCriticalPriority);
  }
  return instance;
}

GUIThread::GUIThread(QRect screenGeometry)
  : QThread()
  , m_screenGeometry{screenGeometry}
  , m_screenOffset{screenGeometry.topLeft()}
  , m_screenRect{m_screenGeometry.translated(-m_screenOffset)} {
  auto frameBuffer = getFrameBuffer();
  O_INFO(
    "Framebuffer:" << frameBuffer->width() << "x" << frameBuffer->height()
                   << " " << frameBuffer->bytesPerLine() << "bytes/line"
                   << frameBuffer->format()
  );
  auto buf = new Blight::buf_t{
    .fd = -1,
    .x = 0,
    .y = 0,
    .width = frameBuffer->width(),
    .height = frameBuffer->height(),
    .stride = frameBuffer->bytesPerLine(),
    .format = (Blight::Format)frameBuffer->format(),
    .data = nullptr,
    .uuid = Blight::buf_t::new_uuid(),
    .surface = 0
  };
  buf->fd = memfd_create(buf->uuid.c_str(), MFD_ALLOW_SEALING);
  if (buf->fd == -1) {
    qFatal(std::strerror(errno));
  }
  if (
    deviceSettings.getDeviceType() == Oxide::DeviceSettings::DeviceType::RM2
  ) {
    // rM2 requires a much larger buffer than is actually used
    // Magic larger number that rM2 uses based on virtual sizes
    if (ftruncate(buf->fd, 26359808)) {
      qFatal(std::strerror(errno));
    }
  } else {
    if (ftruncate(buf->fd, buf->size())) {
      qFatal(std::strerror(errno));
    }
  }
  void* data = mmap(
    NULL, buf->size(), PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, buf->fd, 0
  );
  if (data == MAP_FAILED || data == nullptr) {
    qFatal(std::strerror(errno));
  }
  buf->data = reinterpret_cast<Blight::data_t>(data);
  m_frameBuffer = Blight::shared_buf_t(buf);
  if (m_frameBuffer->fd == -1) {
    qFatal("Failed to open framebuffer");
  }
  m_frameBufferImage = Oxide::QML::getImageForSurface(m_frameBuffer);
  moveToThread(this);
}

GUIThread::~GUIThread() {
  RepaintRequest event;
  while (m_repaintEvents.try_dequeue(event))
    ;
  requestInterruption();
  quit();
  wait();
}

void
GUIThread::enqueue(
  std::shared_ptr<Surface> surface,
  QRect region,
  Blight::WaveformMode waveform,
  Blight::ContentType contentType,
  Blight::UpdateMode mode,
  unsigned int marker,
  bool global,
  std::function<void()> callback
) {
  if (isInterruptionRequested() || dbusInterface->inExclusiveMode()) {
    if (callback != nullptr) {
      callback();
    }
    return;
  }
  Q_ASSERT(global || surface != nullptr);
  QRect intersected;
  if (global) {
    intersected = region;
  } else {
    if (!surface->visible()) {
      O_WARNING("Surface is not currently visible" << surface->id());
      if (callback != nullptr) {
        callback();
      }
      return;
    }
    auto surfaceGeometry = surface->geometry();
    double scale = surface->scale();
    if (scale != 1.0) {
      region = QRect(
        region.x() * scale,
        region.y() * scale,
        region.width() * scale,
        region.height() * scale
      );
    }
    intersected = region.translated(surfaceGeometry.topLeft())
                    .intersected(surfaceGeometry)
                    .intersected(m_screenRect);
  }
  if (intersected.isEmpty()) {
    O_WARNING(
      "Region does not intersect with screen"
      << (global ? "(global)" : surface->id()) << region
    );
    if (callback != nullptr) {
      callback();
    }
    return;
  }
  auto visibleSurfaces = this->visibleSurfaces();
  QRegion repaintRegion(intersected);
  if (!global) {
    // Don't repaint portions covered by another surface that doesn't have
    // alpha channel
    auto i = visibleSurfaces.constEnd();
    while (i != visibleSurfaces.constBegin()) {
      --i;
      auto _surface = *i;
      if (surface == _surface) {
        break;
      }
      auto image = _surface->image();
      if (image != nullptr && !image->isNull() && image->hasAlphaChannel()) {
        continue;
      }
      auto geometry = _surface->geometry();
      repaintRegion -= region.intersected(geometry)
                         .translated(-geometry.topLeft())
                         .intersected(m_screenRect);
    }
  }
  if (repaintRegion.isEmpty()) {
    O_WARNING("Region is currently covered" << surface->id() << region);
    if (callback != nullptr) {
      callback();
    }
    return;
  }
  m_repaintEvents.enqueue(
    RepaintRequest{
      .surface = surface,
      .region = repaintRegion,
      .waveform = waveform,
      .contentType = contentType,
      .mode = mode,
      .marker = marker,
      .global = global,
      .callback = callback
    }
  );
  notify();
}

void
GUIThread::notify() {
  if (!dbusInterface->inExclusiveMode()) {
    m_repaintWait.notify_one();
  }
}

void
GUIThread::clearFrameBuffer() {
  getFrameBuffer()->fill(Qt::white);
  EPFramebuffer::instance()->swapBuffers(
    m_screenGeometry,
    EPContentType::Color,
    EPScreenMode::Content,
    EPFramebuffer::UpdateFlag::FullUpdate
  );
}

Blight::shared_buf_t
GUIThread::framebuffer() {
  return m_frameBuffer;
}

void
GUIThread::repaintSurface(
  QPainter* painter,
  const QRect* rect,
  std::shared_ptr<Surface> surface
) {
  // This should already be handled, but just in case it leaks
  if (surface->isRemoved()) {
    return;
  }
  const QRect surfaceGeometry = surface->geometry();
  const QRect surfaceGlobalRect =
    surfaceGeometry.translated(-m_screenGeometry.topLeft());
  if (!rect->intersects(surfaceGlobalRect)) {
    return;
  }
  auto image = surface->image();
  if (image == nullptr || image->isNull()) {
    return;
  }
  QRect imageRect =
    rect->translated(-surfaceGlobalRect.left(), -surfaceGlobalRect.top())
      .intersected(surface->rect());
  const QRect sourceRect = rect->intersected(surfaceGlobalRect);
  if (
    imageRect.isEmpty() || !imageRect.isValid() || sourceRect.isEmpty() ||
    !sourceRect.isValid()
  ) {
    return;
  }
  O_DEBUG("Repaint surface" << surface->id() << sourceRect << imageRect);
  // TODO - See if there is a way to detect if there is just transparency in
  // the region and don't mark this as repainted.
  double scale = surface->scale();
  if (scale != 1.0) {
    imageRect = QRect(
      imageRect.x() / scale,
      imageRect.y() / scale,
      imageRect.width() / scale,
      imageRect.height() / scale
    );
  }
  painter->drawImage(sourceRect, *image.get(), imageRect);
}

void
GUIThread::redraw(RepaintRequest& event) {
  Q_ASSERT(QThread::currentThread() == (QThread*)this);
  if (dbusInterface->inExclusiveMode()) {
    O_DEBUG("In exclusive mode, skipping redraw");
    return;
  }
  if (!event.global) {
    if (event.surface == nullptr) {
      O_WARNING("surface missing");
      return;
    }
    if (event.surface->isRemoved()) {
      return;
    }
  }
  auto& region = event.region;
  if (region.isEmpty()) {
    O_WARNING("Empty repaint region" << region);
    return;
  }
  Blight::ClockWatch cw;
  // Get visible region on the screen to repaint
  O_DEBUG("Repainting" << region.boundingRect());
  QImage* frameBuffer = getFrameBuffer();
  // "QPainter::begin: A paint device can only be painted by one painter at a
  // time." Ignore this warning, Nothing is displayed if the painter is not
  // active when calling sendUpdate currently
  for (QRect rect : event.region) {
    bool hasAlpha = event.global;
    if (!hasAlpha) {
      auto image = event.surface->image();
      hasAlpha =
        image != nullptr && !image->isNull() && image->hasAlphaChannel();
    }
    QPainter painter(frameBuffer);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (hasAlpha) {
      painter.fillRect(rect, Qt::white);
      for (auto& surface : visibleSurfaces()) {
        if (surface != nullptr) {
          repaintSurface(&painter, &rect, surface);
        }
      }
    } else {
      repaintSurface(&painter, &rect, event.surface);
    }
    painter.end();
    sendUpdate(
      rect, event.waveform, event.contentType, event.mode, event.marker
    );
  }
  O_DEBUG(
    "Repaint" << region.boundingRect() << "done in" << region.rectCount()
              << "paints, and" << cw.elapsed() << "seconds"
  );
}

void
GUIThread::sendUpdate(
  const QRect& rect,
  Blight::WaveformMode waveform,
  Blight::ContentType contentType,
  Blight::UpdateMode mode,
  unsigned int marker
) {
  Q_UNUSED(marker);
  auto instance = EPFramebuffer::instance();
  O_DEBUG("Sending screen update" << rect << waveform << contentType << mode);
  instance->swapBuffers(
    rect,
    (EPContentType)contentType,
    (EPScreenMode)waveform,
    (EPFramebuffer::UpdateFlag)mode
  );
  QPainter(&instance->previousBuffer)
    .drawImage(rect, instance->frameBuffer, rect);
  QPainter(&m_frameBufferImage).drawImage(rect, instance->frameBuffer, rect);
}

void
GUIThread::swap(
  const QRect& rect,
  Blight::WaveformMode waveform,
  Blight::ContentType contentType,
  Blight::UpdateMode mode
) {
  if (m_frameBuffer == nullptr || m_frameBuffer->fd < 0) {
    O_WARNING("swap called when m_frameBuffer not initialized");
    return;
  }
  QPainter painter(getFrameBuffer());
  painter.drawImage(rect, m_frameBufferImage, rect);
  painter.end();
  guiThread->sendUpdate(rect, waveform, contentType, mode, 0);
}

QList<std::shared_ptr<Surface>>
GUIThread::visibleSurfaces() {
  auto visibleSurfaces = dbusInterface->visibleSurfaces();
  visibleSurfaces.erase(
    std::remove_if(
      visibleSurfaces.begin(),
      visibleSurfaces.end(),
      [](std::shared_ptr<Surface> surface) {
        auto connection = surface->connection();
        if (connection == nullptr || !connection->isRunning()) {
          return true;
        }
        if (!surface->has("system") && getpgid(connection->pgid()) < 0) {
          O_WARNING(surface->id() << "With no running process");
          return true;
        }
        auto image = surface->image();
        if (image == nullptr || image->isNull()) {
          O_WARNING(surface->id() << "Null framebuffer");
          return true;
        }
        return false;
      }
    ),
    visibleSurfaces.end()
  );
  return visibleSurfaces;
}
QImage*
GUIThread::getFrameBuffer() {
  auto* instance = EPFramebuffer::instance();
#ifdef __arm__
  return instance->renderTarget;
#else
  return &instance->frameBuffer;
#endif
}
#endif
