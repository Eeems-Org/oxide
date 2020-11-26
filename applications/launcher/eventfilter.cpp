#include "eventfilter.h"
#include "settings.h"
#include <QTimer>
#include <QDebug>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QTabletEvent>
#include <QScreen>

#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872.0
#define WACOM_X_SCALAR (float(DISPLAYWIDTH) / float(DISPLAYHEIGHT))
#define WACOM_Y_SCALAR (float(DISPLAYHEIGHT) / float(DISPLAYWIDTH))
//#define DEBUG_EVENTS

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
QPointF fixTouch(QPointF pt) {
    return QPointF(DISPLAYWIDTH - pt.x(), pt.y());
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

QMouseEvent* tabletToMouseEvent(QEvent::Type type, QEvent* ev){
    auto tabletEvent = (QTabletEvent*)ev;
    auto button = tabletEvent->pressure() > 0 || type == QMouseEvent::MouseButtonRelease ? Qt::LeftButton : Qt::NoButton;
    return new QMouseEvent(
                type,
                transpose(tabletEvent->posF()),
                transpose(tabletEvent->globalPosF()),
                transpose(tabletEvent->globalPosF()),
                button,
                button,
                tabletEvent->modifiers()
                );
}

QMouseEvent* touchToMouseEvent(QEvent::Type type, QEvent* ev){
    auto touchEvent = (QTouchEvent*)ev;
    auto button = Qt::LeftButton;
    return new QMouseEvent(
                type,
                fixTouch(touchEvent->touchPoints()[0].pos()),
            fixTouch(touchEvent->touchPoints()[0].screenPos()),
            fixTouch(touchEvent->touchPoints()[0].screenPos()),
            button,
            button,
            touchEvent->modifiers()
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
        if(isAt(child, pos)){
            if(child->isVisible() && child->isEnabled() && child->acceptedMouseButtons() & Qt::LeftButton){
                if(!result.contains(child)){
                    result.append((QObject*)child);
                    for(auto item : widgetsAt(child, pos)){
                        if(!result.contains(item)){
                            result.append(item);
                        }
                    }
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
void postEvent(QMouseEvent* mouseEvent, QQuickItem* root){
    auto pos = mouseEvent->globalPos();
    for(auto postWidget : widgetsAt(root, pos)){
        if(parentCount((QQuickItem*)postWidget)){
#ifdef DEBUG_EVENTS
            qDebug() << "postWidget: " << postWidget;
#endif
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
            qDebug() << "postEvent: " << event;
            QGuiApplication::postEvent(postWidget, event);
        }
    }
}

bool isTabletEvent(QEvent::Type type) {
    return type == QEvent::TabletPress ||
            type == QEvent::TabletRelease ||
            type == QEvent::TabletMove;
}

bool isTouchEvent(QEvent::Type type) {
    return type == QEvent::TouchBegin ||
            type == QEvent::TouchUpdate ||
            type == QEvent::TouchEnd;
}

QEvent::Type tabletEventTypeToMouseEventType(QEvent::Type type) {
    switch(type) {
    case QEvent::TabletPress:
        return QEvent::MouseButtonPress;
    case QEvent::TabletRelease:
        return QEvent::MouseButtonRelease;
    case QEvent::TabletMove:
        return QEvent::MouseMove;
    default:
        return type;
    }
    return type;
}

QEvent::Type touchEventTypeToMouseEventType(QEvent::Type type) {
    switch(type) {
    case QEvent::TouchBegin:
        return QEvent::MouseButtonPress;
    case QEvent::TouchEnd:
        return QEvent::MouseButtonRelease;
    case QEvent::TouchUpdate:
        return QEvent::MouseMove;
    default:
        return type;
    }
    return type;
}

bool EventFilter::eventFilter(QObject* obj, QEvent* ev){
    auto type = ev->type();
    bool filtered = QObject::eventFilter(obj, ev);
    if(!filtered){
        if (isTabletEvent(type) || isTouchEvent(type)) {
            QMouseEvent* mouseEvent = nullptr;
            if (isTabletEvent(type)) {
                mouseEvent = tabletToMouseEvent(tabletEventTypeToMouseEventType(type), ev);
            }
            else if (Settings::instance().getDeviceType() == DeviceType::RM2 && isTouchEvent(type)) {
                mouseEvent = touchToMouseEvent(touchEventTypeToMouseEventType(type), ev);
            }
            postEvent(mouseEvent, root);
            delete mouseEvent;
        }
#ifdef DEBUG_EVENTS
        else if (type == QEvent::MouseMove
                 || type == QEvent::MouseButtonPress
                 || type == QEvent::MouseButtonRelease) {
            for(auto widget : widgetsAt(root, ((QMouseEvent*)ev)->globalPos())){
                if(parentCount((QQuickItem*)widget)){
                    qDebug() << "postMouseWidget: " << widget;
                }
            }
            qDebug() << obj;
            qDebug() << ev;
        }
#endif
    }
    return filtered;
}
