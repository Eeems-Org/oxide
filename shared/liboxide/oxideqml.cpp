#include "oxideqml.h"
#include "liboxide.h"

#include <QPainter>

#ifdef EPAPER
#include <QQuickWindow>
#include <QGuiApplication>
#include <QFunctionPointer>

#include <libblight.h>
#include <libblight/connection.h>
#endif

namespace Oxide {
    namespace QML{
        OxideQml::OxideQml(QObject *parent) : QObject{parent}{
            deviceSettings.onKeyboardAttachedChanged([this]{
                emit landscapeChanged(deviceSettings.keyboardAttached());
            });
        }

        bool OxideQml::landscape(){ return deviceSettings.keyboardAttached(); }

        OxideQml* getSingleton(){
            static OxideQml* instance = new OxideQml(qApp);
            return instance;
        }

        void registerQML(QQmlApplicationEngine* engine){
            QQmlContext* context = engine->rootContext();
            context->setContextProperty("Oxide", getSingleton());
            engine->addImportPath( "qrc:/codes.eeems.oxide");
            qmlRegisterType<Canvas>("codes.eeems.oxide", 2, 8, "Canvas");
        }

        Canvas::Canvas(QQuickItem* parent)
        : QQuickPaintedItem(parent),
          m_brush{Qt::black},
          m_penWidth{6}
        {
            setAcceptedMouseButtons(Qt::AllButtons);
            m_drawn = QImage(width(), height(), QImage::Format_Grayscale8);
            m_drawn.fill(Qt::white);
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
            m_drawn = QImage(newGeometry.size().toSize(), QImage::Format_Grayscale8);
            m_drawn.fill(Qt::white);
        }

        void Canvas::mousePressEvent(QMouseEvent* event){
            if(!isEnabled()){
                return;
            }
            m_lastPoint = event->localPos();
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
#ifdef EPAPER
            {
                auto buf = getSurfaceForWindow(window());
                QImage image(buf->data, buf->width, buf->height, buf->stride, (QImage::Format)buf->format);
                QPainter p(&image);
                p.setClipRect(mapRectToScene(boundingRect()));
                p.setPen(pen);
                p.drawLine(globalStart, globalEnd);
                qDebug() << rect << QRect(buf->x, buf->y, buf->width, buf->height);
                Blight::connection()->repaint(
                    buf,
                    rect.x(),
                    rect.y(),
                    rect.width(),
                    rect.height(),
                    Blight::WaveformMode::Mono
                );
            }
#else
            update(rect);
#endif
            m_lastPoint = event->localPos();
        }

#ifdef EPAPER
        Blight::shared_buf_t getSurfaceForWindow(QWindow* window){
            static auto fn = (Blight::shared_buf_t(*)(QWindow*))qGuiApp->platformFunction("getSurfaceForWindow");
            if(fn == nullptr){
                qFatal("Could not get getSurfaceForWindow");
            }
            return fn(window);
        }
#endif
    }
}
