#pragma once

#include <QQuickPaintedItem>
#include "surface.h"

class SurfaceWidget : public QQuickPaintedItem{
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
    void paint(QPainter* painter);
    Surface* surface();
    QImage* image();

private:
    QString m_identifier;
};
