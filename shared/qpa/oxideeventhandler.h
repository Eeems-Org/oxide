#pragma once
#include "oxidescreen.h"

#include <QLocalSocket>
#include <QTouchDevice>
#include <qpa/qwindowsysteminterface.h>
#include <QtCore/private/qthread_p.h>
#include <liboxide/tarnish.h>

class OxideEventHandler : public QObject{
public:
    explicit OxideEventHandler(QLocalSocket* socket);
    ~OxideEventHandler();

private:
    QLocalSocket* m_socket = nullptr;
    bool m_tabletPenDown;
    QTouchDevice* m_touchscreen;
    QMutex m_eventMutex;
    QList<QWindowSystemInterface::TouchPoint> m_touchPoints;
    QList<QWindowSystemInterface::TouchPoint> m_lastTouchPoints;

    void readEvents();
    QWindowSystemInterface::TouchPoint getTouchPoint(const Oxide::Tarnish::TouchEventPoint& data);
    void handleTouch(Oxide::Tarnish::TouchEventArgs* data);
    void handleTablet(Oxide::Tarnish::TabletEventArgs* data);
    void handleKey(Oxide::Tarnish::KeyEventArgs* data);
};
