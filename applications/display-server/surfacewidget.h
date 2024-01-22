#pragma once

#ifdef EPAPER
#include <QQuickPaintedItem>
#else
#include <QQuickItem>
#endif
#include "surface.h"

class SurfaceWidget
#ifdef EPAPER
: public QQuickPaintedItem
#else
: public QQuickItem
#endif
{
    Q_OBJECT
    Q_PROPERTY(QString identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
    QML_NAMED_ELEMENT(Surface)

public:
    SurfaceWidget(QQuickItem* parent = nullptr);
    QString identifier();
    void setIdentifier(QString identifier);

signals:
    void identifierChanged(QString);

protected slots:
    void updated();

protected:
#ifdef EPAPER
    void paint(QPainter* painter);
#else
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*);
#endif
    std::shared_ptr<Surface> surface();
    std::shared_ptr<QImage> image();

private:
    QString m_identifier;
};
