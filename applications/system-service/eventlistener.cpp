#include "eventlistener.h"

#include <QMutexLocker>

EventListener* EventListener::instance(){
    static EventListener* instance = nullptr;
    if(instance == nullptr){
        instance = new EventListener();
        qApp->installEventFilter((QObject*)instance);
    }
    return instance;
}

EventListener::EventListener() : QObject(){}

bool EventListener::eventFilter(QObject* object, QEvent* event){
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    for(auto hook : m_hooks){
        if(hook(object, event)){
            return true;
        }
    }
    return false;
}

void EventListener::append(std::function<bool (QObject*, QEvent*)> hook){
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    m_hooks.append(hook);
}

void EventListener::clear(){
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(locker);
    m_hooks.clear();
}
