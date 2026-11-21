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

bool KeyboardHandler::keyboardAttached() const {
    return m_keyboardAttached;
}

void KeyboardHandler::charPress(const QChar& character, const Qt::KeyboardModifiers& modifier) {
    if (character != '\x0') {
        int key = character.unicode();
        auto targets = focusObjects();
        if (targets.empty()) {
            qDebug() << "No target";
            return;
        }
        for (auto target : targets) {
            QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, key, modifier, QString(character));
            QCoreApplication::postEvent(target, event);
        }
    } else {
        qDebug() << "Invalid char";
        keyPress(Qt::Key_unknown, modifier, QString());
    }
}

void KeyboardHandler::keyPress(const Qt::Key& key, const Qt::KeyboardModifiers& modifier, const QString& text) {
    auto targets = focusObjects();
    if (targets.empty()) {
        qDebug() << "No target";
        return;
    }
    for (auto target : targets) {
        QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, key, modifier, text);
        QCoreApplication::postEvent(target, event);
    }
}

void KeyboardHandler::stringPress(const QString& text, const Qt::KeyboardModifiers& modifier) {
    auto targets = focusObjects();
    if (targets.empty()) {
        qDebug() << "No target";
        return;
    }
    for (auto target : targets) {
        QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, Qt::Key_unknown, modifier, text);
        QCoreApplication::postEvent(target, event);
    }
}

QList<QQuickItem*> KeyboardHandler::focusObjects() {
    QList<QQuickItem*> focusItems;
    if (!root) {
        return focusItems;
    }
    for (auto widget : root->findChildren<QQuickItem*>()) {
        if (widget->hasActiveFocus()) {
            focusItems.append(widget);
        }
    }
    return focusItems;
}
