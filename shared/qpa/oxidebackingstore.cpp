#include "oxidebackingstore.h"
#include "oxideintegration.h"

#include <QPainter>
#include <QScreen>
#include <QtCore/qdebug.h>
#include <qpa/qplatformscreen.h>
#include <private/qguiapplication_p.h>
#include <liboxide/debug.h>
#include <liboxide/meta.h>
#include <libblight.h>

QT_BEGIN_NAMESPACE

OxideBackingStore::OxideBackingStore(QWindow* window)
: QPlatformBackingStore(window)
{
    if(OxideIntegration::instance()->options().testFlag(OxideIntegration::DebugQPA)){
        qDebug() << "OxideBackingStore::OxideBackingStore:" << (quintptr)this;
    }
    if(window != nullptr && window->handle()){
        (static_cast<OxideWindow*>(window->handle()))->setBackingStore(this);
    }
}

OxideBackingStore::~OxideBackingStore(){
    Blight::connection()->remove(mBuffer);
}

QPaintDevice* OxideBackingStore::paintDevice(){ return &image; }

void OxideBackingStore::flush(QWindow* window, const QRegion& region, const QPoint& offset){
    Q_UNUSED(offset);
    Q_UNUSED(window);
    if(mBuffer == nullptr || region.isEmpty()){
        return;
    }
    if(OxideIntegration::instance()->options().testFlag(OxideIntegration::DebugQPA)){
        qDebug() << "OxideBackingStore::repaint:" << mBuffer->surface << offset << region;
    }
    bool ok;
    auto waveform = (Blight::WaveformMode)window->property("WA_WAVEFORM").toInt(&ok);
    if(!ok || !waveform){
        waveform = Blight::WaveformMode::HighQualityGrayscale;
    }
    for(auto rect : region){
        Blight::connection()->repaint(
            mBuffer,
            rect.x(),
            rect.y(),
            rect.width(),
            rect.height(),
            waveform
        );
    }
}

void OxideBackingStore::resize(const QSize& size, const QRegion& region){
    Q_UNUSED(region)
    if(image.size() == size){
        return;
    }
    if(OxideIntegration::instance()->options().testFlag(OxideIntegration::DebugQPA)){
        qDebug() << "OxideBackingStore::resize:" << (mBuffer == nullptr ? 0 : mBuffer->surface) << size << region;
    }
    bool ok;
    auto format = (QImage::Format)window()->property("WA_FORMAT").toInt(&ok);
    if(!ok || !format){
        format = QImage::Format_ARGB32_Premultiplied;
    }
    QImage blankImage(size, format);
    if(mBuffer == nullptr){
        auto maybe = Blight::createBuffer(
            window()->x(),
            window()->y(),
            blankImage.width(),
            blankImage.height(),
            blankImage.bytesPerLine(),
            (Blight::Format)blankImage.format()
        );
        if(!maybe.has_value()){
            qWarning() << "Failed to create buffer:" << strerror(errno);
            return;
        }
        auto buffer = maybe.value();
        QImage(
            buffer->data,
            buffer->width,
            buffer->height,
            (QImage::Format)buffer->format
        ).fill(Qt::transparent);
        Blight::addSurface(buffer);
        if(!buffer->surface){
            qWarning() << "Failed to create surface:" << strerror(errno);
            return;
        }
        mBuffer = buffer;
    }else{
        blankImage.fill(Qt::transparent);
        QPainter p(&blankImage);
        p.drawImage(blankImage.rect(), image, image.rect());
        p.end();
        auto maybe = Blight::connection()->resize(
            mBuffer,
            blankImage.width(),
            blankImage.height(),
            blankImage.bytesPerLine(),
            (Blight::data_t)blankImage.constBits()
        );
        if(!maybe.has_value()){
            qWarning() << "Failed to resize surface:" << strerror(errno);
            return;
        }
        mBuffer = maybe.value();
    }
    image = QImage(
        mBuffer->data,
        mBuffer->width,
        mBuffer->height,
        (QImage::Format)mBuffer->format
    );
    if(OxideIntegration::instance()->options().testFlag(OxideIntegration::DebugQPA)){
        qDebug() << "OxideBackingStore::resized" << mBuffer->surface;
    }
}

bool OxideBackingStore::scroll(const QRegion& area, int dx, int dy){
    Q_UNUSED(area)
    Q_UNUSED(dx)
    Q_UNUSED(dy)
    if(OxideIntegration::instance()->options().testFlag(OxideIntegration::DebugQPA)){
        qDebug() << "OxideBackingStore::scroll";
    }
    return false;
}

QImage OxideBackingStore::toImage() const{ return image; }

const QImage& OxideBackingStore::getImageRef() const{ return image; }

QPlatformGraphicsBuffer* OxideBackingStore::graphicsBuffer() const{ return nullptr; }

Blight::shared_buf_t OxideBackingStore::buffer(){ return mBuffer; }

QT_END_NAMESPACE
