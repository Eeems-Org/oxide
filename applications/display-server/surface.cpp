#include "surface.h"
#include "connection.h"
#include "dbusinterface.h"
#include "surfacewidget.h"

#include <unistd.h>
#include <liboxide/debug.h>

Surface::Surface(QObject* parent, int fd, QRect geometry, int stride, QImage::Format format)
: QObject(parent),
  geometry(geometry),
  stride(stride),
  format(format)
{
    if(!file.open(fd, QFile::ReadWrite, QFile::AutoCloseHandle)){
        O_WARNING("Failed to open file");
    }
    data = file.map(0, file.size());
    m_image = new QImage(data, geometry.width(), geometry.height(), stride, format);
    auto interface = dynamic_cast<DbusInterface*>(parent->parent());
    component = dynamic_cast<QQuickItem*>(interface->loadComponent(
        "qrc:/Surface.qml",
        id(),
        QVariantMap{
            {"x", geometry.x()},
            {"y", geometry.y()},
            {"width", geometry.width()},
            {"height", geometry.height()},
            {"visible", true},
        }
    ));
    auto widget = component->findChild<SurfaceWidget*>("Surface");
    if(widget == nullptr){
        O_WARNING("Failed to get surface widget");
        return;
    }
    connect(this, &Surface::update, widget, &SurfaceWidget::update);
}

Surface::~Surface(){
    O_DEBUG("Surface" << id() << "destroyed");
    file.close();
    if(m_image != nullptr){
        delete m_image;
        m_image = nullptr;
    }
    if(component != nullptr){
        component->setVisible(false);
        component->deleteLater();
        component = nullptr;
    }
}

QString Surface::id(){
    auto connection = dynamic_cast<Connection*>(parent());
    return QString("%1/surface/%2").arg(connection->id()).arg(file.handle());
}

bool Surface::isValid(){ return component != nullptr; }

QImage* Surface::image(){ return m_image; }

void Surface::repaint(){ emit update(QRect(QPoint(0, 0), geometry.size())); }

#include "moc_surface.cpp"
