#pragma once

#include <qpa/qplatformbackingstore.h>
#include <qpa/qplatformwindow.h>
#include <QtGui/QImage>
#include <libblight/types.h>

QT_BEGIN_NAMESPACE

class OxideBackingStore : public QPlatformBackingStore
{
public:
    OxideBackingStore(QWindow* window);
    ~OxideBackingStore();

    QPaintDevice* paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    bool scroll(const QRegion &area, int dx, int dy) override;
    QImage toImage() const override;
    const QImage& getImageRef() const;
    QPlatformGraphicsBuffer* graphicsBuffer() const override;
    Blight::shared_buf_t buffer();

private:
    QImage image;
    Blight::shared_buf_t mBuffer = nullptr;
};

QT_END_NAMESPACE
