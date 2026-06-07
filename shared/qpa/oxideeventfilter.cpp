#include "oxideeventfilter.h"

#include <liboxide/devicesettings.h>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface_p.h>

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QTabletEvent>

OxideEventFilter::OxideEventFilter(QObject* parent)
  : QObject(parent)
  , debugEvents(qEnvironmentVariableIsSet("OXIDE_QPA_DEBUG_EVENTS"))
  , debugAllEvents(qEnvironmentVariableIsSet("OXIDE_QPA_DEBUG_ALL_EVENTS"))
  , disableTabletSynth(
      qEnvironmentVariableIsSet("OXIDE_QPA_DISBLE_TABLET_SYNTHESIZE")
    ) {}

QVector<QEvent::Type> ignoreTypes{
  QEvent::MetaCall,
  QEvent::ThreadChange,
  QEvent::SockAct,
  QEvent::Timer,
  QEvent::ChildAdded,
  QEvent::ChildRemoved,
  QEvent::DeferredDelete
};

bool
OxideEventFilter::eventFilter(QObject* obj, QEvent* ev) {
  if (debugAllEvents || (debugEvents && !ignoreTypes.contains(ev->type()))) {
    qDebug() << obj << ev;
  }
  bool filtered = QObject::eventFilter(obj, ev);
  if (isEnabled() && !filtered && !ev->isAccepted()) {
    auto type = ev->type();
    switch (type) {
      case QEvent::TabletPress: {
        ev->accept();
        auto tabletEvent = static_cast<QTabletEvent*>(ev);
        QWindowSystemInterface::handleMouseEvent(
          nullptr,
          transpose(tabletEvent->position()),
          transpose(tabletEvent->globalPosition()),
          tabletEvent->buttons(),
          tabletEvent->button(),
          QEvent::MouseButtonPress,
          Qt::NoModifier,
          Qt::MouseEventSynthesizedByQt
        );
        return true;
      }
      case QEvent::TabletRelease: {
        ev->accept();
        auto tabletEvent = static_cast<QTabletEvent*>(ev);
        QWindowSystemInterface::handleMouseEvent(
          nullptr,
          transpose(tabletEvent->position()),
          transpose(tabletEvent->globalPosition()),
          tabletEvent->buttons(),
          tabletEvent->button(),
          QEvent::MouseButtonRelease,
          Qt::NoModifier,
          Qt::MouseEventSynthesizedByQt
        );
        return true;
      }
      case QEvent::TabletMove: {
        ev->accept();
        auto tabletEvent = static_cast<QTabletEvent*>(ev);
        QWindowSystemInterface::handleMouseEvent(
          nullptr,
          transpose(tabletEvent->position()),
          transpose(tabletEvent->globalPosition()),
          tabletEvent->buttons(),
          tabletEvent->button(),
          QEvent::MouseMove,
          Qt::NoModifier,
          Qt::MouseEventSynthesizedByQt
        );
        return true;
      }
      default:
        break;
    }
  }
  return filtered;
}

#define DISPLAYWIDTH (deviceSettings.getScreenWidth())
#define DISPLAYHEIGHT (deviceSettings.getScreenHeight())
#define WACOM_X_SCALAR (float(DISPLAYWIDTH) / float(DISPLAYHEIGHT))
#define WACOM_Y_SCALAR (float(DISPLAYHEIGHT) / float(DISPLAYWIDTH))

QPointF
OxideEventFilter::transpose(QPointF pointF) {
  switch (deviceSettings.getDeviceType()) {
    case Oxide::DeviceSettings::DeviceType::RM1:
    case Oxide::DeviceSettings::DeviceType::RM2:
      pointF = swap(pointF);
      // Handle scaling from wacom to screensize
      pointF.setX(pointF.x() * WACOM_X_SCALAR);
      pointF.setY((DISPLAYWIDTH - pointF.y()) * WACOM_Y_SCALAR);
      break;
    default:
      break;
  }
  return pointF;
}

QPointF
OxideEventFilter::swap(QPointF pointF) {
  return QPointF(pointF.y(), pointF.x());
}

bool
OxideEventFilter::isEnabled() {
  return !disableTabletSynth &&
         qApp->testAttribute(Qt::AA_SynthesizeMouseForUnhandledTabletEvents);
}
