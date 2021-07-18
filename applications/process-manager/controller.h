#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <QMutex>

#include "tasklist.h"

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TaskList* tasks MEMBER m_tasks READ tasks NOTIFY tasksChanged)
public:
    int protectPid;
    explicit Controller(QQmlApplicationEngine* engine) : QObject(nullptr), mutex(QMutex::NonRecursive), _engine(engine){
        _sortBy = "name";
        _lastSortBy = "pid";
        m_tasks = new TaskList();
        emit tasksChanged(m_tasks);
    }
    Q_INVOKABLE void sortBy(QString key){ m_tasks->sort(key); }
    Q_INVOKABLE void reload(){ m_tasks->reload(); }
    TaskList* tasks(){ return m_tasks; }


signals:
    void sortByChanged();
    void tasksChanged(TaskList*);

private:
    QString _sortBy;
    QString _lastSortBy;
    QMutex mutex;
    QQmlApplicationEngine* _engine;
    TaskList* m_tasks;
};
