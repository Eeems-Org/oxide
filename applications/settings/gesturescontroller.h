#pragma once

#include <QObject>

#include <liboxide.h>
#include <liboxide/dbus.h>

using namespace codes::eeems::oxide1;

enum SwipeDirection { None, Right, Left, Up, Down };

class GesturesController : public QObject {
  Q_OBJECT
  Q_PROPERTY(
    int swipeLengthRight READ swipeLengthRight WRITE setSwipeLengthRight NOTIFY
      swipeLengthRightChanged
  )
  Q_PROPERTY(
    int swipeLengthLeft READ swipeLengthLeft WRITE setSwipeLengthLeft NOTIFY
      swipeLengthLeftChanged
  )
  Q_PROPERTY(
    int swipeLengthUp READ swipeLengthUp WRITE setSwipeLengthUp NOTIFY
      swipeLengthUpChanged
  )
  Q_PROPERTY(
    int swipeLengthDown READ swipeLengthDown WRITE setSwipeLengthDown NOTIFY
      swipeLengthDownChanged
  )

public:
  explicit GesturesController(QObject* parent, System* systemApi)
    : QObject(parent)
    , systemApi(systemApi) {
    connect(
      systemApi,
      &System::swipeLengthChanged,
      this,
      [this](int direction, int length) {
        switch (direction) {
          case Right:
            emit swipeLengthRightChanged(length);
            break;
          case Left:
            emit swipeLengthLeftChanged(length);
            break;
          case Up:
            emit swipeLengthUpChanged(length);
            break;
          case Down:
            emit swipeLengthDownChanged(length);
            break;
        }
      }
    );
    deactivate();
  }

  Q_INVOKABLE void activate() {
    systemApi->blockSignals(false);
    emit swipeLengthRightChanged(swipeLengthRight());
    emit swipeLengthLeftChanged(swipeLengthLeft());
    emit swipeLengthUpChanged(swipeLengthUp());
    emit swipeLengthDownChanged(swipeLengthDown());
  }

  Q_INVOKABLE void deactivate() { systemApi->blockSignals(true); }

  int swipeLengthRight() { return systemApi->getSwipeLength(Right); }
  void setSwipeLengthRight(int length) {
    systemApi->setSwipeLength(Right, length);
  }
  int swipeLengthLeft() { return systemApi->getSwipeLength(Left); }
  void setSwipeLengthLeft(int length) {
    systemApi->setSwipeLength(Left, length);
  }
  int swipeLengthUp() { return systemApi->getSwipeLength(Up); }
  void setSwipeLengthUp(int length) { systemApi->setSwipeLength(Up, length); }
  int swipeLengthDown() { return systemApi->getSwipeLength(Down); }
  void setSwipeLengthDown(int length) {
    systemApi->setSwipeLength(Down, length);
  }

signals:
  void swipeLengthRightChanged(int);
  void swipeLengthLeftChanged(int);
  void swipeLengthUpChanged(int);
  void swipeLengthDownChanged(int);

private:
  System* systemApi = nullptr;
};
