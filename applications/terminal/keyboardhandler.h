#ifndef KEYBOARDHANDLER_H
#define KEYBOARDHANDLER_H

#include <QChar>
#include <QCoreApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QQuickItem>
#include <Qt>

#include <liboxide/devicesettings.h>

class KeyboardHandler : public QObject {
  Q_OBJECT
  Q_PROPERTY(
    bool keyboardAttached READ keyboardAttached NOTIFY keyboardAttachedChanged
  )
  Q_PROPERTY(QQuickItem* root READ root WRITE setRoot NOTIFY rootChanged)
public:
  explicit KeyboardHandler(QObject* parent = nullptr);

  QQuickItem* root() const;
  void setRoot(QQuickItem* root);

  bool keyboardAttached() const;

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

signals:
  void keyboardAttachedChanged(bool attached);
  void rootChanged();

private:
  QQuickItem* m_root = nullptr;
  bool m_keyboardAttached = false;
};

#endif // KEYBOARDHANDLER_H
