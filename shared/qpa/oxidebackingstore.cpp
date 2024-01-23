#include "oxidebackingstore.h"
#include "oxideintegration.h"

#include <qscreen.h>
#include <QtCore/qdebug.h>
#include <qpa/qplatformscreen.h>
#include <private/qguiapplication_p.h>
#include <liboxide/meta.h>
#include <libblight.h>

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

OxideBackingStore::~OxideBackingStore(){
    OxideIntegration::instance()->connection->remove(mBuffer);
}

QPaintDevice* OxideBackingStore::paintDevice(){ return &image; }

void OxideBackingStore::flush(QWindow* window, const QRegion& region, const QPoint& offset){
    Q_UNUSED(offset);
    (static_cast<OxideWindow*>(window->handle()))->repaint(region);
}

void OxideBackingStore::resize(const QSize& size, const QRegion& region){
    Q_UNUSED(region)
    if(image.size() == size){
        return;
    }
    QImage blankImage(size, QImage::Format_RGB16);
    auto maybe = OxideIntegration::instance()->connection->resize(
        mBuffer,
        blankImage.width(),
        blankImage.height(),
        blankImage.bytesPerLine(),
        (Blight::data_t)blankImage.constBits()
    );
    if(!maybe.has_value()){
        qWarning() << "Failed to resize surface:" << mBuffer->surface.c_str();
    }
    mBuffer = maybe.value();
}

bool OxideBackingStore::scroll(const QRegion& area, int dx, int dy){
    Q_UNUSED(area)
    Q_UNUSED(dx)
    Q_UNUSED(dy)
    if(mDebug){
        qDebug() << "OxideBackingStore::scroll";
    }
    return false;
}

QImage OxideBackingStore::toImage() const{ return image; }

const QImage& OxideBackingStore::getImageRef() const{ return image; }

QPlatformGraphicsBuffer* OxideBackingStore::graphicsBuffer() const{ return nullptr; }

Blight::shared_buf_t OxideBackingStore::buffer(){ return mBuffer; }

QT_END_NAMESPACE
