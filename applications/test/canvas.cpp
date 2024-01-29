#include "canvas.h"

#include <QPainter>
#include <QQuickWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QFunctionPointer>

#include <QWindow>
#include <QBackingStore>

#include <libblight.h>
#include <libblight/connection.h>

Canvas::Canvas() {
    setAcceptedMouseButtons(Qt::AllButtons);
    m_drawn = QImage(width(), height(), QImage::Format_Grayscale8);
    m_drawn.fill(Qt::white);
}

void Canvas::paint(QPainter* painter){
    painter->drawImage(boundingRect(), m_drawn);
}

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
    p.setPen(QPen(Qt::black, 6));
    p.drawLine(m_lastPoint, event->localPos());
    p.end();
#ifdef EPAPER
    {
        auto buf = getSurfaceForWindow();
        QImage image(buf->data, buf->width, buf->height, buf->stride, (QImage::Format)buf->format);
        QPainter p(&image);
        p.setClipRect(mapRectToScene(boundingRect()));
        p.setPen(QPen(Qt::black, 6));
        const QPoint globalStart = mapToScene(m_lastPoint).toPoint();
        const QPoint globalEnd = event->globalPos();
        p.drawLine(globalStart, globalEnd);
        auto rect = QRect(globalStart, globalEnd)
            .normalized()
            .marginsAdded(QMargins(24, 24, 24, 24));
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
    const QPoint globalStart = mapToScene(m_lastPoint).toPoint();
    const QPoint globalEnd = event->globalPos();
    auto rect = QRect(globalStart, globalEnd)
        .normalized()
        .marginsAdded(QMargins(24, 24, 24, 24));
    update(rect);
#endif
    m_lastPoint = event->localPos();
}

Blight::shared_buf_t Canvas::getSurfaceForWindow(){
    static auto fn = (Blight::shared_buf_t(*)(QWindow*))qGuiApp->platformFunction("getSurfaceForWindow");
    if(fn == nullptr){
        qFatal("Could not get getSurfaceForWindow");
    }
    return fn(window());
}
