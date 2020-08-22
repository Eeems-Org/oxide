#include "eventfilter.h"
#include <QTimer>
#include <QDebug>

EventFilter::EventFilter(QObject *parent) : QObject(parent){ }

bool EventFilter::eventFilter(QObject* obj, QEvent* ev){
  if(
      ev->type() == QEvent::KeyPress
      || ev->type() == QEvent::MouseMove
      || ev->type() == QEvent::TabletMove
      || ev->type() == QEvent::TouchBegin
      || ev->type() == QEvent::TouchUpdate
      || ev->type() == QEvent::TouchEnd
      || ev->type() == QEvent::TouchCancel
  ){
      timer->stop();
      timer->start();
  }
  return QObject::eventFilter(obj, ev);
}
