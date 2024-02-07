#pragma once

#include <QObject>
#include <QRect>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QImage>

#include "connection.h"
class Connection;

#include "../../shared/liboxide/meta.h"

class Surface : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)

public:
    Surface(
        Connection* connection,
        int fd,
        Blight::surface_id_t identifier,
        QRect geometry,
        int stride,
        QImage::Format format
    );
    ~Surface();
    QString id();
    Blight::surface_id_t identifier(){ return m_identifier; }
    bool isValid();
    std::shared_ptr<QImage> image();
    void repaint(QRect rect = QRect());
    int fd();
    const QRect& geometry();
    const QSize size();
    const QRect rect();
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
    Connection* connection();
    bool isRemoved();
    void removed();

signals:
    void update(const QRect& geometry);

private slots:
#ifndef EPAPER
    void activeFocusChanged(bool focus);
#endif

private:
    Connection* m_connection;
    Blight::surface_id_t m_identifier;
    QRect m_geometry;
    int m_stride;
    QImage::Format m_format;
    QFile file;
    uchar* data;
    std::shared_ptr<QImage> m_image;
#ifndef EPAPER
    QQuickItem* component;
#endif
    QString m_id;
    QStringList flags;
    bool m_removed;
};
