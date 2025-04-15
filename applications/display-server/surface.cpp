#include "surface.h"
#include "connection.h"
#include "dbusinterface.h"
#ifdef EPAPER
#include "guithread.h"
#else
#include "surfacewidget.h"
#endif

#include <unistd.h>
#include <sys/mman.h>
#include <liboxide/debug.h>

#define S_DEBUG(msg) O_DEBUG("[" << id() << "]" << msg)
#define S_WARNING(msg) O_WARNING("[" << id() << "]" << msg)
#define S_INFO(msg) O_INFO("[" << id() << "]" << msg)

Surface::Surface(
    Connection* connection,
    int fd,
    Blight::surface_id_t identifier,
    QRect geometry,
    int stride,
    QImage::Format format
)
: QObject(),
  m_connection(connection),
  m_identifier{identifier},
  m_geometry(geometry),
  m_stride(stride),
  m_format(format),
  m_fd{fd},
  m_removed{false}
{
    m_id = QString("%1/surface/%2").arg(connection->id()).arg(identifier);
    data = reinterpret_cast<uchar*>(mmap(
        NULL,
        m_stride * m_geometry.height(),
        PROT_READ | PROT_WRITE,
        MAP_SHARED_VALIDATE,
        fd,
        0
    ));
    if(data == MAP_FAILED){
        S_WARNING("Failed to map buffer");
        return;
    }
    m_image = std::shared_ptr<QImage>(new QImage(
        data,
        geometry.width(),
        geometry.height(),
        stride,
        format
    ));
#ifndef EPAPER
    component = dynamic_cast<QQuickItem*>(dbusInterface->loadComponent(
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
    if(component == nullptr){
        S_WARNING("Failed to create surface widget");
        return;
    }
    auto widget = component->findChild<SurfaceWidget*>("Surface");
    if(widget == nullptr){
        S_WARNING("Failed to get surface widget");
        component->deleteLater();
        component = nullptr;
        return;
    }
    widget->forceActiveFocus();
    connect(this, &Surface::update, widget, &SurfaceWidget::update);
    connect(widget, &SurfaceWidget::activeFocusChanged, this, &Surface::activeFocusChanged);
#endif
    emit connection->focused();
    setVisible(true);
    setZ(std::numeric_limits<int>::max());
    S_INFO("Surface created");
}

Surface::~Surface(){
    S_INFO("Surface destroyed");
    if(data != MAP_FAILED){
        ::munmap(data, m_stride * m_geometry.height());
    }
    ::close(m_fd);
    setVisible(false);
#ifdef EPAPER
    repaint();
#else
    if(component != nullptr){
        component->deleteLater();
        component = nullptr;
    }
#endif
}

QString Surface::id(){ return m_id; }

bool Surface::isValid(){
#ifdef EPAPER
    return data != MAP_FAILED;
#else
    return component != nullptr;
#endif
}

std::shared_ptr<QImage> Surface::image(){
    if(!m_connection->isRunning()){
        return std::shared_ptr<QImage>();
    }
    return m_image;
}

void Surface::repaint(QRect rect){
    if(rect.isEmpty()){
        rect = this->rect();
    }
#ifdef EPAPER
    guiThread->enqueue(
        nullptr,
        geometry(),
        Blight::WaveformMode::HighQualityGrayscale,
        Blight::UpdateMode::PartialUpdate,
        0,
        true
    );
#else
    emit update(rect);
#endif
}

int Surface::fd(){ return m_fd; }

const QRect& Surface::geometry(){ return m_geometry; }

const QSize Surface::size(){ return m_geometry.size(); }

const QRect Surface::rect(){ return QRect(QPoint(0, 0), size()); }

int Surface::stride(){ return m_stride; }

QImage::Format Surface::format(){ return m_format; }

void Surface::move(int x, int y){
    m_geometry.moveTopLeft(QPoint(x, y));
#ifndef EPAPER
    component->setX(x);
    component->setY(y);
#endif
}

void Surface::setVisible(bool visible){
#ifdef EPAPER
    setProperty("visible", visible);
#else
    if(component != nullptr){
        component->setVisible(visible);
    }
#endif
}

bool Surface::visible(){
#ifdef EPAPER
    return property("visible").toBool();
#else
    if(component == nullptr){
        return false;
    }
    return component->isVisible();
#endif
}

int Surface::z(){
#ifdef EPAPER
    return property("z").toInt();
#else
    if(component == nullptr){
        return -1;
    }
    return component->z();
#endif
}

void Surface::setZ(int z){
#ifdef EPAPER
    setProperty("z", z);
#else
    if(component != nullptr){
        component->setZ(z);
    }
#endif
}

bool Surface::has(const QString& flag){ return flags.contains(flag); }

void Surface::set(const QString& flag){
    if(!has(flag)){
        flags.append(flag);
    }
}

void Surface::unset(const QString& flag){ flags.removeAll(flag); }

Connection* Surface::connection(){ return m_connection; }

bool Surface::isRemoved(){ return m_removed; }

void Surface::removed(){ m_removed = true; }

#ifndef EPAPER
void Surface::activeFocusChanged(bool focus){
    if(focus){
        emit m_connection->focused();
    }
}
#endif

#include "moc_surface.cpp"
