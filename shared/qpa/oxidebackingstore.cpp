#include "oxidebackingstore.h"
#include "oxideintegration.h"

#include <qscreen.h>
#include <QtCore/qdebug.h>
#include <qpa/qplatformscreen.h>
#include <private/qguiapplication_p.h>
#include <liboxide/tarnish.h>

QT_BEGIN_NAMESPACE

OxideBackingStore::OxideBackingStore(QWindow *window)
: QPlatformBackingStore(window),
  mDebug(OxideIntegration::instance()->options() & OxideIntegration::DebugBackingStore)
{
    if(mDebug){
        qDebug() << "OxideBackingStore::OxideBackingStore:" << (quintptr)this;
    }
}

OxideBackingStore::~OxideBackingStore(){}

QPaintDevice* OxideBackingStore::paintDevice(){
    if(mDebug){
        qDebug("OxideBackingStore::paintDevice");
    }
    return getFrameBuffer();
}

void OxideBackingStore::flush(QWindow* window, const QRegion& region, const QPoint& offset){
    Q_UNUSED(window);
    Q_UNUSED(region);
    Q_UNUSED(offset);
    if(mDebug){
        static int c = 0;
        QString filename = QString("output%1.png").arg(c++, 4, 10, QChar(u'0'));
        qDebug() << "OxideBackingStore::flush() saving contents to" << filename.toLocal8Bit().constData();
        getFrameBuffer()->save(filename);
    }
    Oxide::Tarnish::screenUpdate();
}

void OxideBackingStore::resize(const QSize& size, const QRegion& region){
    Q_UNUSED(size)
    Q_UNUSED(region)
    if(mDebug){
        qDebug("OxideBackingStore::resize");
    }
    // TODO - handle resize
}
void OxideBackingStore::beginPaint(const QRegion& region){
    if(mDebug){
        qDebug("OxideBackingStore::beginPaint");
    }
    getFrameBuffer();
    Oxide::Tarnish::lockFrameBuffer();
    QPlatformBackingStore::beginPaint(region);
}

void OxideBackingStore::endPaint(){
    if(mDebug){
        qDebug("OxideBackingStore::endPaint");
    }
    getFrameBuffer();
    Oxide::Tarnish::unlockFrameBuffer();
    QPlatformBackingStore::endPaint();
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
QImage* OxideBackingStore::getFrameBuffer(){
    if(frameBuffer != nullptr){
        return frameBuffer;
    }
    auto image = Oxide::Tarnish::frameBufferImage();
    if(image != nullptr){
        return image;
    }
    auto size = QGuiApplication::primaryScreen()->size();
    Oxide::Tarnish::createFrameBuffer(size.width(), size.height());
    frameBuffer = Oxide::Tarnish::frameBufferImage();
    return frameBuffer;
}

QT_END_NAMESPACE
