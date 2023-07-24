#pragma once
#include "oxidescreen.h"

#include <QLocalSocket>
#include <QTouchDevice>
#include <qpa/qwindowsysteminterface.h>
#include <QtCore/private/qthread_p.h>
#include <liboxide/tarnish.h>

class OxideEventHandler : public QDaemonThread{
public:
    explicit OxideEventHandler(QLocalSocket* socket, OxideScreen* primaryScreen);
    ~OxideEventHandler();

private:
    QLocalSocket* m_socket;
    OxideScreen* m_primaryScreen;
    bool m_tabletPenDown;
    QTouchDevice m_touchscreen;
    QMutex m_eventMutex;
    QList<QWindowSystemInterface::TouchPoint> m_touchPoints;
    QList<QWindowSystemInterface::TouchPoint> m_lastTouchPoints;

    void readEvents();
    QWindowSystemInterface::TouchPoint getTouchPoint(const Oxide::Tarnish::TouchEventPoint& data);
    void handleTouch(Oxide::Tarnish::TouchEventArgs* data);
};
