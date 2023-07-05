#include "oxidebackingstore.h"
#include "oxideintegration.h"
#include "oxidescreen.h"

#include <qscreen.h>
#include <QtCore/qdebug.h>
#include <qpa/qplatformscreen.h>
#include <private/qguiapplication_p.h>
#include <liboxide/tarnish.h>

QT_BEGIN_NAMESPACE

OxideBackingStore::OxideBackingStore(QWindow* window)
: QPlatformBackingStore(window),
  mDebug(OxideIntegration::instance()->options() & OxideIntegration::DebugQPA)
{
    if(mDebug){
        qDebug() << "OxideBackingStore::OxideBackingStore:" << (quintptr)this;
    }
    if(window != nullptr && window->handle()){
        (static_cast<OxideWindow*>(window->handle()))->setBackingStore(this);
    }
}

OxideBackingStore::~OxideBackingStore(){}

QPaintDevice* OxideBackingStore::paintDevice(){
    if(mDebug){
        qDebug("OxideBackingStore::paintDevice");
    }
    return &image;
}

void OxideBackingStore::flush(QWindow* window, const QRegion& region, const QPoint& offset){
    Q_UNUSED(region);
    Q_UNUSED(offset);
    if(mDebug){
        static int c = 0;
        QString filename = QString("/tmp/output%1.png").arg(c++, 4, 10, QChar(u'0'));
        qDebug() << "OxideBackingStore::flush() saving contents to" << filename.toLocal8Bit().constData();
        image.save(filename);
    }
    ((OxideScreen*)window->screen()->handle())->scheduleUpdate();
}

void OxideBackingStore::resize(const QSize& size, const QRegion& region){
    Q_UNUSED(size)
    Q_UNUSED(region)
    if(mDebug){
        qDebug("OxideBackingStore::resize");
    }
    if(image.size() != size){
        image = QImage(size, QImage::Format_RGB16);
    }
}

bool OxideBackingStore::scroll(const QRegion& area, int dx, int dy){
    Q_UNUSED(area)
    Q_UNUSED(dx)
    Q_UNUSED(dy)
    if(mDebug){
        qDebug("OxideBackingStore::scroll");
    }
    return false;
}
QImage OxideBackingStore::toImage() const{
    if(mDebug){
        qDebug("OxideBackingStore::toImage");
    }
    return image;
}
const QImage& OxideBackingStore::getImageRef() const{
    if(mDebug){
        qDebug("OxideBackingStore::getImageRef");
    }
    return image;
}

QPlatformGraphicsBuffer* OxideBackingStore::graphicsBuffer() const{
    if(mDebug){
        qDebug("OxideBackingStore::graphicsBuffer");
    }
    return nullptr;
}

QT_END_NAMESPACE
