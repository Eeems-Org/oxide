#pragma once

#include <QObject>
#include <QRect>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QImage>

#include "../../shared/liboxide/meta.h"
#include "../../shared/libblight/meta.h"

class Surface : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_SURFACE_INTERFACE)

public:
    Surface(QObject* parent, int fd, QRect geometry, int stride, QImage::Format format);
    ~Surface();
    QString id();
    bool isValid();
    std::shared_ptr<QImage> image();
    void repaint();
    int fd();
    const QRect& geometry();
    const QSize size();
    int stride();
    QImage::Format format();
    void move(int x, int y);
    bool visible();
    void setVisible(bool visible);
    int z();
    void setZ(int z);
    bool has(const QString& flag);
    void set(const QString& flag);
    void unset(const QString& flag);

signals:
    void update(const QRect& geometry);

private slots:
    void activeFocusChanged(bool focus);

private:
    QRect m_geometry;
    int m_stride;
    QImage::Format m_format;
    QFile file;
    uchar* data;
    std::shared_ptr<QImage> m_image;
    QQuickItem* component;
    QString m_id;
    QStringList flags;
};
