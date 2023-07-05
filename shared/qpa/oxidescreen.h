#pragma once

#include <qpa/qplatformscreen.h>
#include "oxidebackingstore.h"
#include "oxidewindow.h"

class OxideWindow;

class OxideScreen : public QObject, public QPlatformScreen{
public:
    OxideScreen();
    QRect geometry() const override { return mGeometry; }
    int depth() const override { return mDepth; }
    QImage::Format format() const override { return mFormat; }
    QSizeF physicalSize() const override;
    void scheduleUpdate();
    void setDirty(const QRect& rect);
    QWindow* topWindow() const;
    void addWindow(OxideWindow* window);
    void removeWindow(OxideWindow* window);

protected:
    bool event(QEvent *event) override;

private:
    QRect mGeometry;
    int mDepth;
    QImage::Format mFormat;
    QSize mPhysicalSize;
    bool mUpdatePending;
    QRegion mRepaintRegion;
    void redraw();
};
