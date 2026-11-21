#ifndef KEYBOARDHANDLER_H
#define KEYBOARDHANDLER_H

#include <QChar>
#include <QCoreApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QQuickItem>
#include <Qt>

class KeyboardHandler : public QObject {
  Q_OBJECT
public:
  explicit KeyboardHandler(QObject* parent = nullptr);

  QQuickItem* root = nullptr;

  Q_INVOKABLE void
  charPress(const QChar& character, const Qt::KeyboardModifiers& modifier);
  Q_INVOKABLE void keyPress(
    const Qt::Key& key,
    const Qt::KeyboardModifiers& modifier,
    const QString& text
  );
  Q_INVOKABLE void
  stringPress(const QString& text, const Qt::KeyboardModifiers& modifier);

  QList<QQuickItem*> focusObjects();
};

#endif // KEYBOARDHANDLER_H
