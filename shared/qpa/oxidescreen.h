#pragma once

#include <QMutex>
#include <QEventLoop>
#include <qpa/qplatformscreen.h>

#include "oxidewindow.h"

class OxideWindow;

class Q_DECL_EXPORT OxideScreen : public QObject, public QPlatformScreen{
    Q_OBJECT

public:
    OxideScreen();
    QRect tabletGeometry() const;
    QRect geometry() const override;
    void setGeometry(QRect geometry);
    int depth() const override;
    QImage::Format format() const override;
    QSizeF physicalSize() const override;
    Qt::ScreenOrientation orientation() const override;
    Qt::ScreenOrientation nativeOrientation() const override;
    QString name() const override;
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
    QRect m_geometry;
    int m_depth;
    QImage::Format m_format;
    QSize mPhysicalSize;
    bool mUpdatePending;
    QRegion mRepaintRegion;
    unsigned int m_marker;
    QList<OxideWindow*> m_windows;
    QRect m_tabletGeometry;

    void redraw();
};
