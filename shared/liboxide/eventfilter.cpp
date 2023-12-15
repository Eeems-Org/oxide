#include "eventfilter.h"
#include "debug.h"

#include <QTimer>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QScreen>
#include <QGuiApplication>

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
    EventFilter::EventFilter(QObject *parent) : QObject(parent), root(nullptr){}

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
    QPointF globalPos(QQuickItem* obj){
        qreal x = obj->x();
        qreal y = obj->y();
        while(obj->parentItem() != nullptr){
            obj = obj->parentItem();
            x += obj->x();
            y += obj->y();
        }
        return QPointF(x, y);
    }
    QMouseEvent* toMouseEvent(QEvent::Type type, QEvent* ev){
        auto tabletEvent = static_cast<QTabletEvent*>(ev);
        return new QMouseEvent(
            type,
            transpose(tabletEvent->posF()),
            transpose(tabletEvent->globalPosF()),
            transpose(tabletEvent->globalPosF()),
            tabletEvent->button(),
            tabletEvent->buttons(),
            tabletEvent->modifiers()
        );
    }
    bool isAt(QQuickItem* item, QPointF pos){
        auto itemPos = globalPos(item);
        auto otherItemPos = QPointF(itemPos.x() + item->width(), itemPos.y() + item->height());
        return pos.x() >= itemPos.x() && pos.x() <= otherItemPos.x() && pos.y() >= itemPos.y() && pos.y() <= otherItemPos.y();
    }
    QList<QObject*> widgetsAt(QQuickItem* root, QPointF pos){
        QList<QObject*> result;
        auto children = root->findChildren<QQuickItem*>();
        for(auto child : children){
            if(result.contains(child)){
                continue;
            }
            if(!child->isVisible() || !child->isEnabled()){
                continue;
            }
            if(child->acceptedMouseButtons() & Qt::LeftButton && isAt(child, pos)){
                result.append((QObject*)child);
                for(auto item : widgetsAt(child, pos)){
                    if(!result.contains(item)){
                        result.append(item);
                    }
                }
                continue;
            }
            if(!child->clip()){
                for(auto item : widgetsAt(child, pos)){
                    if(!result.contains(item)){
                        result.append(item);
                    }
                }
            }
        }
        return result;
    }
    int parentCount(QQuickItem* obj){
        int count = 0;
        while(obj->parentItem()){
            count++;
            obj = obj->parentItem();
        }
        return count;
    }
    void postEvent(QEvent::Type type, QEvent* ev, QQuickItem* root){
        auto mouseEvent = toMouseEvent(type, ev);
        auto pos = mouseEvent->globalPos();
        for(auto postWidget : widgetsAt(root, pos)){
            if(parentCount((QQuickItem*)postWidget)){
                O_DEBUG_EVENT("postWidget: " << postWidget);
                auto event = new QMouseEvent(
                    mouseEvent->type(), mouseEvent->localPos(), mouseEvent->windowPos(),
                    mouseEvent->screenPos(), mouseEvent->button(), mouseEvent->buttons(),
                    mouseEvent->modifiers()
                );
                auto widgetPos = globalPos((QQuickItem*)postWidget);
                auto localPos = event->localPos();
                localPos.setX(pos.x() - widgetPos.x());
                localPos.setY((pos.y()) - widgetPos.y());
                event->setLocalPos(localPos);
                QGuiApplication::postEvent(postWidget, event);
            }
        }
        delete mouseEvent;
    }

    bool EventFilter::eventFilter(QObject* obj, QEvent* ev){
        auto type = ev->type();
        bool filtered = QObject::eventFilter(obj, ev);
        if(!filtered){
            if(type == QEvent::TabletPress){
                O_DEBUG_EVENT(ev << ev->isAccepted());
                postEvent(QMouseEvent::MouseButtonPress, ev, root);
            }else if(type == QEvent::TabletRelease){
                O_DEBUG_EVENT(ev << ev->isAccepted());
                postEvent(QMouseEvent::MouseButtonRelease, ev, root);
            }else if(type == QEvent::TabletMove){
                O_DEBUG_EVENT(ev << ev->isAccepted());
                postEvent(QMouseEvent::MouseMove, ev, root);
            }
#ifdef DEBUG_EVENTS
            /*else */if(
                type != QEvent::MetaCall
                && type != QEvent::SockAct
                && type != QEvent::SockClose
                && type != QEvent::ChildAdded
                && type != QEvent::ChildPolished
                && type != QEvent::ChildRemoved
                && type != QEvent::Timer
                && type != QEvent::UpdateRequest
            ){
                O_DEBUG(ev << ev->isAccepted() << obj);
            }
#endif
        }
        return filtered;
    }
}

#include "moc_eventfilter.cpp"
