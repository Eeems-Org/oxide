#include "eventfilter.h"
#include "debug.h"

#include <QTimer>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QScreen>
#include <QGuiApplication>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface_p.h>

#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872.0
#define WACOM_X_SCALAR (float(DISPLAYWIDTH) / float(DISPLAYHEIGHT))
#define WACOM_Y_SCALAR (float(DISPLAYHEIGHT) / float(DISPLAYWIDTH))
#ifdef DEBUG_EVENTS
#define O_DEBUG_EVENT(msg) O_DEBUG(msg)
#else
#define O_DEBUG_EVENT(msg)
#endif

namespace Oxide{
    EventFilter::EventFilter(QObject *parent) : QObject(parent) {}

    QPointF swap(QPointF pointF){
        return QPointF(pointF.y(), pointF.x());
    }

    QPointF transpose(QPointF pointF){
        pointF = swap(pointF);
        // Handle scaling from wacom to screensize
        pointF.setX(pointF.x() * WACOM_X_SCALAR);
        pointF.setY((DISPLAYWIDTH - pointF.y()) * WACOM_Y_SCALAR);
        return pointF;
    }

    bool EventFilter::eventFilter(QObject* obj, QEvent* ev){
        auto type = ev->type();
        if(QObject::eventFilter(obj, ev)){
            return true;
        }
        if(type == QEvent::TabletPress){
            O_DEBUG_EVENT(ev);
            auto tabletEvent = (QTabletEvent*)ev;
            QWindowSystemInterface::handleMouseEvent(
                nullptr,
                transpose(tabletEvent->position()),
                transpose(tabletEvent->globalPosition()),
                tabletEvent->buttons(),
                tabletEvent->button(),
                QEvent::MouseButtonPress
            );
            tabletEvent->accept();
            return true;
        }
        if(type == QEvent::TabletRelease){
            O_DEBUG_EVENT(ev);
            auto tabletEvent = (QTabletEvent*)ev;
            QWindowSystemInterface::handleMouseEvent(
                nullptr,
                transpose(tabletEvent->position()),
                transpose(tabletEvent->globalPosition()),
                tabletEvent->buttons(),
                tabletEvent->button(),
                QEvent::MouseButtonRelease
            );
            tabletEvent->accept();
            return true;
        }
        if(type == QEvent::TabletMove){
            O_DEBUG_EVENT(ev);
            auto tabletEvent = (QTabletEvent*)ev;
            QWindowSystemInterface::handleMouseEvent(
                nullptr,
                transpose(tabletEvent->position()),
                transpose(tabletEvent->globalPosition()),
                tabletEvent->buttons(),
                tabletEvent->button(),
                QEvent::MouseMove
            );
            tabletEvent->accept();
            return true;
        }
#ifdef DEBUG_EVENTS
        if(
            type == QEvent::MouseMove
            || type == QEvent::MouseButtonPress
            || type == QEvent::MouseButtonRelease
        ){
            O_DEBUG(obj);
            O_DEBUG(ev);
        }
#endif
        return false;
    }
}

#include "moc_eventfilter.cpp"
