#pragma once

#include <QObject>
#include <QRect>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QImage>

#include "../../shared/liboxide/meta.h"

class Surface : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_SURFACE_INTERFACE)

public:
    Surface(QObject* parent, int fd, QRect geometry, int stride, QImage::Format format);
    ~Surface();
    QString id();
    bool isValid();
    QImage* image();
    void repaint();

signals:
    void update(const QRect& geometry);

private:
    QRect geometry;
    int stride;
    QImage::Format format;
    QFile file;
    uchar* data;
    QImage* m_image;
    QQuickItem* component;
};
