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
    return Oxide::Tarnish::frameBufferImage();
}

void OxideBackingStore::flush(QWindow* window, const QRegion& region, const QPoint& offset){
    Q_UNUSED(window);
    Q_UNUSED(region);
    Q_UNUSED(offset);

    if(mDebug){
        static int c = 0;
        QString filename = QString("output%1.png").arg(c++, 4, 10, QChar(u'0'));
        qDebug() << "OxideBackingStore::flush() saving contents to" << filename.toLocal8Bit().constData();
        Oxide::Tarnish::frameBufferImage()->save(filename);
    }
    Oxide::Tarnish::screenUpdate();
}

void OxideBackingStore::resize(const QSize &size, const QRegion &){
    auto image = Oxide::Tarnish::frameBufferImage();
    if(image->size() != size){
        qDebug("OxideBackingStore::resize");
        // TODO handle resize
        //QImage::Format format = QGuiApplication::primaryScreen()->handle()->format();
    }
}

QT_END_NAMESPACE
