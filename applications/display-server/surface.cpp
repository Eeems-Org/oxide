#include "surface.h"
#include "connection.h"
#include "dbusinterface.h"
#include "surfacewidget.h"

#include <unistd.h>
#include <liboxide/debug.h>

Surface::Surface(QObject* parent, int fd, QRect geometry, int stride, QImage::Format format)
: QObject(parent),
  m_geometry(geometry),
  m_stride(stride),
  m_format(format)
{
    auto connection = dynamic_cast<Connection*>(parent);
    m_id = QString("%1/surface/%2").arg(connection->id()).arg(fd);
    if(!file.open(fd, QFile::ReadWrite, QFile::AutoCloseHandle)){
        O_WARNING("Failed to open file");
    }
    data = file.map(0, m_stride * m_geometry.height());
    m_image = new QImage(
        data,
        geometry.width(),
        geometry.height(),
        stride,
        format
    );
    auto interface = dynamic_cast<DbusInterface*>(connection->parent());
    component = dynamic_cast<QQuickItem*>(interface->loadComponent(
        "qrc:/Surface.qml",
        id(),
        QVariantMap{
            {"x", geometry.x()},
            {"y", geometry.y()},
            {"width", geometry.width()},
            {"height", geometry.height()},
            {"visible", true},
            {"focus", true}
        }
    ));
    if(component == nullptr){
        O_WARNING("Failed to create surface widget");
        return;
    }
    auto widget = component->findChild<SurfaceWidget*>("Surface");
    if(widget == nullptr){
        O_WARNING("Failed to get surface widget");
        component->deleteLater();
        component = nullptr;
        return;
    }
    widget->forceActiveFocus();
    connect(this, &Surface::update, widget, &SurfaceWidget::update);
    connect(widget, &SurfaceWidget::activeFocusChanged, this, &Surface::activeFocusChanged);
    interface->setFocused(connection);
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

QString Surface::id(){ return m_id; }

bool Surface::isValid(){ return component != nullptr; }

QImage* Surface::image(){ return m_image; }

void Surface::repaint(){ emit update(QRect(QPoint(0, 0), m_geometry.size())); }

int Surface::fd(){ return file.handle(); }

const QRect& Surface::geometry(){ return m_geometry; }

int Surface::stride(){ return m_stride; }

QImage::Format Surface::format(){ return m_format; }

void Surface::move(int x, int y){
    m_geometry.moveTopLeft(QPoint(x, y));
    component->setX(x);
    component->setY(y);
}

void Surface::activeFocusChanged(bool focus){
    if(focus){
        auto connection = dynamic_cast<Connection*>(parent());
        auto interface = dynamic_cast<DbusInterface*>(connection->parent());
        interface->setFocused(connection);
    }
}

#include "moc_surface.cpp"
