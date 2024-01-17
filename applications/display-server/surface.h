#pragma once

#include <QObject>
#include <QRect>

#include "../../shared/liboxide/meta.h"

class Surface : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_SURFACE_INTERFACE)

public:
    Surface(QObject* parent, int fd, QRect geometry);
    ~Surface();
    QString id();

private:
    int fd;
    QRect geometry;
};
