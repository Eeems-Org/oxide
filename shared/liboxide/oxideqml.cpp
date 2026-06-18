#include "oxideqml.h"

#include <libblight.h>
#include <libblight/connection.h>

#include <QFunctionPointer>
#include <QGuiApplication>
#include <QPainter>
#include <QQuickWindow>
#include <atomic>
#include <mutex>

#include "liboxide.h"

static std::atomic<unsigned int> marker = 0;

namespace Oxide {
  namespace QML {
    OxideQml::OxideQml(QObject* parent)
      : QObject{parent}
      , m_waveform{Blight::WaveformMode::Content}
      , m_contentType{Blight::ContentType::Color}
      , m_updateMode{Blight::UpdateMode::PartialUpdate} {
      deviceSettings.onKeyboardAttachedChanged([this] {
        emit landscapeChanged(deviceSettings.keyboardAttached());
      });
    }

    bool OxideQml::landscape() {
      return deviceSettings.keyboardAttached();
    }

    Blight::WaveformMode OxideQml::waveform() {
      return m_waveform;
    }

    void OxideQml::setWaveform(Blight::WaveformMode waveform) {
      m_waveform = waveform;
      qDebug() << "waveform:" << waveform;
      emit waveformChanged(waveform);
    }

    Blight::ContentType OxideQml::contentType() {
      return m_contentType;
    }

    void OxideQml::setContentType(Blight::ContentType contentType) {
      m_contentType = contentType;
      qDebug() << "contentType:" << contentType;
      emit contentTypeChanged(contentType);
    }

    Blight::UpdateMode OxideQml::updateMode() {
      return m_updateMode;
    }

    void OxideQml::setUpdateMode(Blight::UpdateMode updateMode) {
      m_updateMode = updateMode;
      qDebug() << "updateMode:" << updateMode;
      emit updateModeChanged(updateMode);
    }

    QString OxideQml::deviceName() {
      return deviceSettings.getDeviceName();
    }

    QBrush OxideQml::brushFromColor(const QColor& color) {
      return QBrush(color, Qt::SolidPattern);
    }

    QPen OxideQml::createPen(
      const QBrush& brush,
      qreal width,
      Qt::PenStyle style,
      Qt::PenCapStyle cap,
      Qt::PenJoinStyle join
    ) {
      return QPen(brush, width, style, cap, join);
    }

    OxideQml* getSingleton() {
      static OxideQml* instance = new OxideQml(qApp);
      return instance;
    }

    void registerQML(QQmlApplicationEngine* engine) {
      QQmlContext* context = engine->rootContext();
      context->setContextProperty("Oxide", getSingleton());
      engine->addImportPath("qrc:/codes.eeems.oxide");
      qmlRegisterType<Canvas>("codes.eeems.oxide", 3, 0, "Canvas");
    }

    Canvas::Canvas(QQuickItem* parent)
      : QQuickPaintedItem(parent)
      , m_pen{Qt::black, 6}
      , m_repainted{}
      , m_finalizeTimer(this)
      , m_ghostControlTimer(this)
      , m_drawing{false}
      , m_hovering{false} {
      setAcceptedMouseButtons(Qt::AllButtons);
      m_drawn = QImage(width(), height(), QImage::Format_ARGB32_Premultiplied);
      m_drawn.fill(Qt::transparent);
      m_finalizeTimer.callOnTimeout(this, [this] {
        if (m_drawing || !m_timerMutex.try_lock()) {
          return;
        }
        auto region = m_repainted;
        m_repainted = QRegion();
        repaint(
          window(),
          region.boundingRect(),
          Blight::WaveformMode::Content,
          Blight::ContentType::Color,
          Blight::UpdateMode::PartialUpdate,
          true
        );
        emit drawDone();
        m_ghostControlTimer.start(5000);
        m_timerMutex.unlock();
      });
      m_finalizeTimer.setSingleShot(true);
      m_ghostControlTimer.callOnTimeout(this, [this] {
        if (m_drawing || !m_timerMutex.try_lock()) {
          return;
        }
        repaint(
          window(),
          boundingRect(),
          Blight::WaveformMode::Content,
          Blight::ContentType::Color,
          Blight::UpdateMode::FullUpdate,
          true
        );
        emit drawDone();
        m_timerMutex.unlock();
      });
      m_ghostControlTimer.setSingleShot(true);
    }

    Canvas::~Canvas() {
      m_ghostControlTimer.stop();
    }

    void Canvas::paint(QPainter* painter) {
      painter->drawImage(boundingRect(), m_drawn);
    }

    QPen Canvas::pen() {
      return m_pen;
    }

    void Canvas::setPen(QPen pen) {
      m_pen = pen;
      emit penChanged(pen);
    }

    QImage* Canvas::image() {
      return &m_drawn;
    }

    void Canvas::geometryChange(
      const QRectF& newGeometry,
      const QRectF& oldGeometry
    ) {
      Q_UNUSED(oldGeometry);
      auto size = newGeometry.size().toSize();
      if (size.isEmpty()) {
        return;
      }
      QImage image(size, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter p(&image);
      p.drawImage(image.rect(), m_drawn, m_drawn.rect());
      p.end();
      m_drawn = image;
    }

    void Canvas::mousePressEvent(QMouseEvent* event) {
      if (!isEnabled()) {
        return;
      }
      m_lastPoint = event->position();
      m_drawing = true;
      {
        QPainter painter(&m_drawn);
        painter.setPen(m_pen);
        painter.drawPoint(m_lastPoint);
      }
      const QPoint globalPoint = mapToScene(m_lastPoint).toPoint();
      auto image = getImageForWindow(window());
      {
        QPainter painter(&image);
        painter.setClipRect(mapRectToScene(boundingRect()));
        painter.setPen(m_pen);
        painter.drawPoint(globalPoint);
      }
      m_pending +=
        QRect(globalPoint, globalPoint)
          .marginsAdded(
            QMargins(m_pen.width(), m_pen.width(), m_pen.width(), m_pen.width())
          );
      m_LastPaint.reset();
      emit drawStart();
    }

    void Canvas::mouseMoveEvent(QMouseEvent* event) {
      if (!isEnabled() || !contains(event->position())) {
        return;
      }
      std::lock_guard locker(m_timerMutex);
      Q_UNUSED(locker);
      {
        QPainter painter(&m_drawn);
        painter.setPen(m_pen);
        painter.drawLine(m_lastPoint, event->position());
      }
      const QPoint globalStart = mapToScene(m_lastPoint).toPoint();
      const QPoint globalEnd = event->globalPosition().toPoint();
      int margin = qMax(24, (int)qCeil(m_pen.widthF() / 2.0) + 4);
      auto rect = QRect(globalStart, globalEnd)
                    .normalized()
                    .marginsAdded(QMargins(margin, margin, margin, margin));
      auto image = getImageForWindow(window());
      {
        QPainter painter(&image);
        painter.setClipRect(mapRectToScene(boundingRect()));
        painter.setPen(m_pen);
        painter.drawLine(globalStart, globalEnd);
      }
      m_pending += rect;
      // Only send repaints every 16ms (~60fps)
      if (m_LastPaint.elapsed() > 0.016) {
        applyPending();
      }
      m_lastPoint = event->position();
    }

    void Canvas::mouseReleaseEvent(QMouseEvent* event) {
      Q_UNUSED(event);
      std::lock_guard locker(m_timerMutex);
      Q_UNUSED(locker);
      m_drawing = false;
      applyPending();
      m_finalizeTimer.start(500);
    }

    void Canvas::applyPending() {
      auto rect = m_pending.boundingRect();
      m_repainted += rect;
      repaint(
        window(),
        rect,
        Blight::WaveformMode::UltraFast,
        Blight::ContentType::Monochrome,
        Blight::UpdateMode::PenUpdate
      );
      m_pending = QRegion();
      m_LastPaint.reset();
    }

    Blight::shared_buf_t getSurfaceForWindow(QWindow* window) {
      static auto fn = (Blight::shared_buf_t (*)(QWindow*))
                         qGuiApp->platformFunction("getSurfaceForWindow");
      if (fn == nullptr) {
        qFatal("Could not get getSurfaceForWindow");
      }
      return fn(window);
    }

    QImage getImageForWindow(QWindow* window) {
      return getImageForSurface(getSurfaceForWindow(window));
    }

    void repaint(
      QWindow* window,
      QRectF rect,
      Blight::WaveformMode waveform,
      Blight::ContentType contentType,
      Blight::UpdateMode updateMode,
      bool sync
    ) {
      auto buf = getSurfaceForWindow(window);
      unsigned int _marker = 0;
      if (sync) {
        _marker = ++marker;
      }
      auto maybe = Blight::connection()->repaint(
        buf,
        rect.x(),
        rect.y(),
        rect.width(),
        rect.height(),
        waveform,
        contentType,
        updateMode,
        _marker
      );
      if (sync && maybe.has_value()) {
        Blight::connection()->waitForMarker(_marker);
      }
    }

    QImage getImageForSurface(Blight::shared_buf_t buffer) {
      return QImage(
        buffer->data,
        buffer->width,
        buffer->height,
        buffer->stride,
        (QImage::Format)buffer->format
      );
    }
  } // namespace QML
} // namespace Oxide
