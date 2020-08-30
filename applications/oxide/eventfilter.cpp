#include "eventfilter.h"
#include <QTimer>
#include <QDebug>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QScreen>

#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872.0
#define WACOM_X_SCALAR (float(DISPLAYWIDTH) / float(DISPLAYHEIGHT))
#define WACOM_Y_SCALAR (float(DISPLAYHEIGHT) / float(DISPLAYWIDTH))

EventFilter::EventFilter(QObject *parent) : QObject(parent){ }

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
QList<QObject*> widgetsAt(QQuickItem* root, QPointF pos){
    QList<QObject*> result;
    auto children = root->findChildren<QQuickItem*>();
    for(auto child : children){
        auto childPos = globalPos(child);
        auto otherChildPos = QPointF(childPos.x() + child->width(), childPos.y() + child->height());
        if(pos.x() >= childPos.x() && pos.x() <= otherChildPos.x() && pos.y() >= childPos.y() && pos.y() <= otherChildPos.y()){
            if(child->isVisible() && child->isEnabled() && child->acceptedMouseButtons() & Qt::LeftButton){
                result.append((QObject*)child);
                for(auto item : widgetsAt(child, pos)){
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
            qDebug() << "postWidget: " << postWidget;
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
}

bool EventFilter::eventFilter(QObject* obj, QEvent* ev){
    auto type = ev->type();
    bool filtered = QObject::eventFilter(obj, ev);
    if(
      type == QEvent::KeyPress
      || type == QEvent::MouseMove
      || type == QEvent::TabletMove
      || type == QEvent::TouchBegin
      || type == QEvent::TouchUpdate
      || type == QEvent::TouchEnd
      || type == QEvent::TouchCancel
    ){
        timer->stop();
        timer->start();
    }else if(!filtered){
        if(type == QEvent::TabletPress){
            qDebug() << ev;
            postEvent(QMouseEvent::MouseButtonPress, ev, root);
        }else if(type == QEvent::TabletRelease){
            qDebug() << ev;
            postEvent(QMouseEvent::MouseButtonRelease, ev, root);
        }else if(type == QEvent::TabletMove){
            qDebug() << ev;
            postEvent(QMouseEvent::MouseMove, ev, root);
        }else if(
            type == QEvent::MouseMove
            || type == QEvent::MouseButtonPress
            || type == QEvent::MouseButtonRelease
        ){
            for(auto widget : widgetsAt(root, ((QMouseEvent*)ev)->globalPos())){
                if(parentCount((QQuickItem*)widget)){
                    qDebug() << "postWidget: " << widget;
                }
            }
            qDebug() << obj;
            qDebug() << ev;
        }
    }
    return filtered;
}
