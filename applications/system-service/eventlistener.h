#pragma once

#include <QObject>

#define eventListener EventListener::instance()

class EventListener : QObject
{
    Q_OBJECT
public:
    static EventListener* instance();
    EventListener();

    bool eventFilter(QObject* object, QEvent* event);
    void append(std::function<bool(QObject*,QEvent*)> hook);
    void clear();

private:
    QMutex m_mutex;
    QList<std::function<bool(QObject*,QEvent*)>> m_hooks;
};
