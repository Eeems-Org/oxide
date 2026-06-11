#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(loggingCategory);

class LockscreenHookReceiver : public QObject {
  Q_OBJECT
public:
  static LockscreenHookReceiver* singleton();
  explicit LockscreenHookReceiver(QObject* parent = nullptr);
  ~LockscreenHookReceiver() override = default;

  bool waitForHome();
  bool waitForHook();
  bool setPasscodeHandler(QObject* passcodeHandler);
  inline QObject* passcodeHandler() { return m_passcodeHandler; }
  bool hasPasscode();

public slots:
  void onUnlocked();

signals:
  void unlocked();

private:
  QObject* m_passcodeHandler = nullptr;

  int execute(const QString& executable);
};
