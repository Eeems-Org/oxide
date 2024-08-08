#pragma once

#include <QQuickItem>
#include "surface.h"

#ifndef EPAPER
class SurfaceWidget
: public QQuickItem
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
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*);
    std::shared_ptr<Surface> surface();
    std::shared_ptr<QImage> image();

private:
    QString m_identifier;
};
#endif
