#include "surface.h"
#include "connection.h"

Surface::Surface(QObject* parent, int fd, QRect geometry)
: QObject(parent),
  fd(fd),
  geometry(geometry)
{
}

Surface::~Surface(){
    ::close(fd);
}

QString Surface::id(){
    auto connection = dynamic_cast<Connection*>(parent());
    return QString("%1/surface/%2").arg(connection->id()).arg(fd);
}

#include "moc_surface.cpp"
