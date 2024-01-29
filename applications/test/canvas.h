#pragma once

#include <QImage>
#include <QObject>
#include <QQmlEngine>
#include <QQuickPaintedItem>

#include <libblight/types.h>

class Canvas : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
public:
    Canvas();
    void paint(QPainter* painter) override;

protected:
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    Blight::shared_buf_t getSurfaceForWindow();

private:
    QPointF m_lastPoint;
    QImage m_drawn;
};
