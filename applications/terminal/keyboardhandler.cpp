#include "keyboardhandler.h"

#include <liboxide/devicesettings.h>

#include <QQmlApplicationEngine>

KeyboardHandler::KeyboardHandler(QObject* parent)
  : QObject(parent)
  , m_keyboardAttached(deviceSettings.keyboardAttached()) {
  deviceSettings.onKeyboardAttachedChanged([this]() {
    bool now = deviceSettings.keyboardAttached();
    if (m_keyboardAttached != now) {
      m_keyboardAttached = now;
      emit keyboardAttachedChanged(m_keyboardAttached);
    }
  });
}

QQuickItem*
KeyboardHandler::root() const {
  return m_root;
}

void
KeyboardHandler::setRoot(QQuickItem* root) {
  if (m_root != root) {
    m_root = root;
    emit rootChanged();
  }
}

bool
KeyboardHandler::keyboardAttached() const {
  return m_keyboardAttached;
}

void
KeyboardHandler::charPress(
  const QChar& character,
  const Qt::KeyboardModifiers& modifier
) {
  qDebug() << "charPress" << character;
  if (character != '\x0') {
    int key = character.unicode();
    auto targets = focusObjects();
    if (targets.empty()) {
      qDebug() << "No target";
      return;
    }
    for (auto target : targets) {
      QKeyEvent* event =
        new QKeyEvent(QEvent::KeyPress, key, modifier, QString(character));
      QCoreApplication::postEvent(target, event);
      target->forceActiveFocus();
    }
  } else {
    qDebug() << "Invalid char";
    keyPress(Qt::Key_unknown, modifier, QString());
  }
}

void
KeyboardHandler::keyPress(
  const Qt::Key& key,
  const Qt::KeyboardModifiers& modifier,
  const QString& text
) {
  auto targets = focusObjects();
  if (targets.empty()) {
    qDebug() << "No target";
    return;
  }
  for (auto target : targets) {
    QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, key, modifier, text);
    QCoreApplication::postEvent(target, event);
    target->forceActiveFocus();
  }
}

void
KeyboardHandler::stringPress(
  const QString& text,
  const Qt::KeyboardModifiers& modifier
) {
  auto targets = focusObjects();
  if (targets.empty()) {
    qDebug() << "No target";
    return;
  }
  for (auto target : targets) {
    QKeyEvent* event =
      new QKeyEvent(QEvent::KeyPress, Qt::Key_unknown, modifier, text);
    QCoreApplication::postEvent(target, event);
    target->forceActiveFocus();
  }
}

QList<QQuickItem*>
KeyboardHandler::focusObjects() {
  QList<QQuickItem*> focusItems;
  if (!m_root) {
    qDebug() << "focusObjects: m_root is null";
    return focusItems;
  }
  for (auto widget : m_root->findChildren<QQuickItem*>()) {
    if (widget->hasActiveFocus()) {
      focusItems.append(widget);
    }
  }
  return focusItems;
}
