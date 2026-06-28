#include "eventlistener.h"

#include <QMutexLocker>
#include <mutex>

EventListener*
EventListener::instance() {
  static EventListener* instance = nullptr;
  static std::once_flag initFlag;
  std::call_once(initFlag, []() {
    instance = new EventListener();
    qApp->installEventFilter(static_cast<QObject*>(instance));
  });
  return instance;
}

EventListener::EventListener()
  : QObject() {}

bool
EventListener::eventFilter(QObject* object, QEvent* event) {
  QMutexLocker locker(&m_mutex);
  Q_UNUSED(locker);
  for (auto hook : m_hooks) {
    if (hook(object, event)) {
      return true;
    }
  }
  return false;
}

void
EventListener::append(std::function<bool(QObject*, QEvent*)> hook) {
  QMutexLocker locker(&m_mutex);
  Q_UNUSED(locker);
  m_hooks.append(hook);
}

void
EventListener::clear() {
  QMutexLocker locker(&m_mutex);
  Q_UNUSED(locker);
  m_hooks.clear();
}
