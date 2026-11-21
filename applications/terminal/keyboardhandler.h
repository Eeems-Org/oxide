#ifndef KEYBOARDHANDLER_H
#define KEYBOARDHANDLER_H

#include <Qt>
#include <QChar>
#include <QKeyEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QQuickItem>

#include <liboxide/devicesettings.h>

class KeyboardHandler : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool keyboardAttached READ keyboardAttached NOTIFY keyboardAttachedChanged)
public:
    explicit KeyboardHandler(QObject* parent = nullptr);

    QQuickItem* root = nullptr;

    bool keyboardAttached() const;

    Q_INVOKABLE void charPress(const QChar& character, const Qt::KeyboardModifiers& modifier);
    Q_INVOKABLE void keyPress(const Qt::Key& key, const Qt::KeyboardModifiers& modifier, const QString& text);
    Q_INVOKABLE void stringPress(const QString& text, const Qt::KeyboardModifiers& modifier);

    QList<QQuickItem*> focusObjects();

signals:
    void keyboardAttachedChanged(bool attached);

private:
    bool m_keyboardAttached = false;
};

#endif // KEYBOARDHANDLER_H
