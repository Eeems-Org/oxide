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
    QRect geometry() const override;
    void setGeometry(QRect geometry);
    int depth() const override;
    QImage::Format format() const override;
    QSizeF physicalSize() const override;
    Qt::ScreenOrientation orientation() const override;
    Qt::ScreenOrientation nativeOrientation() const override;
    QString name() const override;
    void addWindow(OxideWindow* window);
    void removeWindow(OxideWindow* window);

private:
    QRect m_geometry;
    QList<OxideWindow*> m_windows;
};
