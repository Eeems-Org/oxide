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
    QRect geometry() const override;
    void setGeometry(QRect geometry);
    int depth() const override;
    QImage::Format format() const override;
    QSizeF physicalSize() const override;
    void scheduleUpdate();
    void setDirty(const QRect& rect);
    QWindow* topWindow() const;
    OxideWindow* topPlatformWindow() const;
    void addWindow(OxideWindow* window);
    void removeWindow(OxideWindow* window);

public slots:
    void raiseTopWindow();
    void lowerTopWindow();
    void closeTopWindow();

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
