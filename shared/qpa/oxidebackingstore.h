#pragma once

#include <qpa/qplatformbackingstore.h>
#include <qpa/qplatformwindow.h>
#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

class OxideBackingStore : public QPlatformBackingStore
{
public:
    OxideBackingStore(QWindow *window);
    ~OxideBackingStore();

    QPaintDevice* paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;

private:
    const bool mDebug;
};

QT_END_NAMESPACE
