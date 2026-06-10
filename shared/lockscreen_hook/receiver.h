#pragma once
#include <atomic>

#include <QLoggingCategory>
#include <QMutex>
#include <QObject>
#include <QWaitCondition>

Q_DECLARE_LOGGING_CATEGORY(loggingCategory);

static std::atomic<bool> passcodeHandlerHooked = false;

class LockscreenHookReceiver : public QObject {
  Q_OBJECT
public:
  static LockscreenHookReceiver* singleton();
  explicit LockscreenHookReceiver(QObject* parent = nullptr);
  ~LockscreenHookReceiver() override = default;

  void waitForUnlock();
  bool setPasscodeHandler(QObject* passcodeHandler);
  inline QObject* passcodeHandler() { return m_passcodeHandler; }

public slots:
  void onUnlocked();

private:
  QMutex m_mutex;
  QWaitCondition m_cond;
  bool m_unlockNotified = false;
  QObject* m_passcodeHandler = nullptr;
};
