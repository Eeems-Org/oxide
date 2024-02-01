#include "oxideqml.h"
#include "liboxide.h"

#include <QPainter>
#include <QQuickWindow>
#include <QGuiApplication>
#include <QFunctionPointer>

#include <libblight.h>
#include <libblight/connection.h>
#include <atomic>


static std::atomic<unsigned int> marker = 0;

namespace Oxide {
    namespace QML{
        OxideQml::OxideQml(QObject *parent) : QObject{parent}{
            deviceSettings.onKeyboardAttachedChanged([this]{
                emit landscapeChanged(deviceSettings.keyboardAttached());
            });
        }

        bool OxideQml::landscape(){ return deviceSettings.keyboardAttached(); }

        QBrush OxideQml::brushFromColor(const QColor& color){
            return QBrush(color, Qt::SolidPattern);
        }

        OxideQml* getSingleton(){
            static OxideQml* instance = new OxideQml(qApp);
            return instance;
        }

        void registerQML(QQmlApplicationEngine* engine){
            QQmlContext* context = engine->rootContext();
            context->setContextProperty("Oxide", getSingleton());
            engine->addImportPath( "qrc:/codes.eeems.oxide");
            qmlRegisterType<Canvas>("codes.eeems.oxide", 3, 0, "Canvas");
        }

        Canvas::Canvas(QQuickItem* parent)
        : QQuickPaintedItem(parent),
          m_brush{Qt::black},
          m_penWidth{6}
        {
            setAcceptedMouseButtons(Qt::AllButtons);
            m_drawn = QImage(width(), height(), QImage::Format_ARGB32_Premultiplied);
            m_drawn.fill(Qt::transparent);
        }

        void Canvas::paint(QPainter* painter){
            painter->drawImage(boundingRect(), m_drawn);
        }

        QBrush Canvas::brush(){ return m_brush; }

        void Canvas::setBrush(QBrush brush){
            m_brush = brush;
            emit brushChanged(brush);
        }

        qreal Canvas::penWidth(){ return m_penWidth; }

        void Canvas::setPenWidth(qreal penWidth){
            m_penWidth = penWidth;
            emit penWidthChanged(penWidth);
        }

        QImage* Canvas::image(){ return &m_drawn; }

        void Canvas::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry){
            Q_UNUSED(oldGeometry);
            auto size = newGeometry.size().toSize();
            if(size.isEmpty()){
                return;
            }
            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter p(&image);
            p.drawImage(image.rect(), m_drawn, m_drawn.rect());
            p.end();
            m_drawn = image;
        }

        void Canvas::mousePressEvent(QMouseEvent* event){
            if(!isEnabled()){
                return;
            }
            m_lastPoint = event->localPos();
            emit drawStart();
        }

        void Canvas::mouseMoveEvent(QMouseEvent* event){
            if(
                !isEnabled()
                || !contains(event->localPos())
            ){
                return;
            }
            QPainter p(&m_drawn);
            QPen pen(m_brush, m_penWidth);
            p.setPen(pen);
            p.drawLine(m_lastPoint, event->localPos());
            p.end();
            const QPoint globalStart = mapToScene(m_lastPoint).toPoint();
            const QPoint globalEnd = event->globalPos();
            auto rect = QRect(globalStart, globalEnd)
                .normalized()
                .marginsAdded(QMargins(24, 24, 24, 24));
            {
                auto image = getImageForWindow(window());
                QPainter p(&image);
                p.setClipRect(mapRectToScene(boundingRect()));
                p.setPen(pen);
                p.drawLine(globalStart, globalEnd);
                repaint(window(), rect, Blight::WaveformMode::Mono);
            }
            m_lastPoint = event->localPos();
        }

        void Canvas::mouseReleaseEvent(QMouseEvent* event){
            Q_UNUSED(event);
            auto color = m_brush.color();
            if(
                !m_brush.isOpaque()
                && (
                    color != Qt::black || color != Qt::white
                )
            ){
                repaint(window(), mapRectToScene(boundingRect()));
            }
            emit drawDone();
        }

        Blight::shared_buf_t getSurfaceForWindow(QWindow* window){
            static auto fn = (Blight::shared_buf_t(*)(QWindow*))qGuiApp->platformFunction("getSurfaceForWindow");
            if(fn == nullptr){
                qFatal("Could not get getSurfaceForWindow");
            }
            return fn(window);
        }

        QImage getImageForWindow(QWindow* window){
            return getImageForSurface(getSurfaceForWindow(window));
        }

        void repaint(QWindow* window, QRectF rect, Blight::WaveformMode waveform, bool sync){
            auto buf = getSurfaceForWindow(window);
            unsigned int _marker = 0;
            if(sync){
                _marker = ++marker;
            }
            auto maybe = Blight::connection()->repaint(
                buf,
                rect.x(),
                rect.y(),
                rect.width(),
                rect.height(),
                waveform,
                _marker
            );
            if(sync && maybe.has_value()){
                Blight::connection()->waitForMarker(_marker);
            }
        }

        QImage getImageForSurface(Blight::shared_buf_t buffer){
            return QImage(
                buffer->data,
                buffer->width,
                buffer->height,
                buffer->stride,
                (QImage::Format)buffer->format
            );
        }
    }
}
