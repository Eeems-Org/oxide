#include "oxidebackingstore.h"
#include "oxideintegration.h"
#include "oxidescreen.h"

#include <qscreen.h>
#include <QtCore/qdebug.h>
#include <qpa/qplatformscreen.h>
#include <private/qguiapplication_p.h>
#include <liboxide/tarnish.h>
#include <liboxide/meta.h>

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

QPaintDevice* OxideBackingStore::paintDevice(){ return &image; }

void OxideBackingStore::flush(QWindow* window, const QRegion& region, const QPoint& offset){
    Q_UNUSED(offset);
    (static_cast<OxideWindow*>(window->handle()))->repaint(region);
}

void OxideBackingStore::resize(const QSize& size, const QRegion& region){
    Q_UNUSED(region)
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

QImage OxideBackingStore::toImage() const{ return image; }
const QImage& OxideBackingStore::getImageRef() const{ return image; }
QPlatformGraphicsBuffer* OxideBackingStore::graphicsBuffer() const{ return nullptr; }

QT_END_NAMESPACE
