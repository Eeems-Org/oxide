#pragma once

#include <QMutex>
#include <QEventLoop>
#include <qpa/qplatformscreen.h>

#include "oxidebackingstore.h"
#include "oxidewindow.h"

class OxideWindow;

class Q_DECL_EXPORT OxideScreen : public QObject, public QPlatformScreen{
    Q_OBJECT

public:
    OxideScreen();
    QRect geometry() const override { return mGeometry; }
    void setGeometry(QRect geometry) { mGeometry = geometry; }
    int depth() const override { return mDepth; }
    QImage::Format format() const override { return mFormat; }
    QSizeF physicalSize() const override;
    void scheduleUpdate();
    void setDirty(const QRect& rect);
    QWindow* topWindow() const;
    OxideWindow* topPlatformWindow() const;
    void addWindow(OxideWindow* window);
    void removeWindow(OxideWindow* window);

signals:
    void waitForPaint(unsigned int marker);

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
    unsigned int m_marker;
    QList<OxideWindow*> m_windows;
};
