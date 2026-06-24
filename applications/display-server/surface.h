#pragma once

#include <QImage>
#include <QObject>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QRect>

#include <memory>
#include <sys/mman.h>

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
    QImage::Format format,
    double scale
  );
  ~Surface();
  QString id();
  Blight::surface_id_t identifier() { return m_identifier; }
  bool isValid();
  std::shared_ptr<QImage> image();
  void repaint(QRect rect = QRect(), bool flash = false);
  int fd();
  const QRect& geometry();
  const QRect& rawGeometry();
  const QSize size();
  const QSize rawSize();
  const QRect rect();
  const QRect rawRect();
  double scale();
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
  std::shared_ptr<Connection> connection();
  bool isRemoved();
  void removed();

signals:
  void update(const QRect& geometry);

private slots:
#ifndef EPAPER
  void activeFocusChanged(bool focus);
#endif

private:
  std::shared_ptr<Connection> m_connection;
  Blight::surface_id_t m_identifier;
  QRect m_geometry;
  QRect m_geometryScaled;
  int m_stride;
  QImage::Format m_format;
  int m_fd;
  uchar* data = reinterpret_cast<uchar*>(MAP_FAILED);
  std::shared_ptr<QImage> m_image;
#ifndef EPAPER
  QQuickItem* component = nullptr;
#endif
  QString m_id;
  QStringList flags;
  bool m_removed;
  double m_scale;
};
