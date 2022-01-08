#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <QMutex>

#include "tasklist.h"
#include "sentry_settings.h"

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TaskList* tasks MEMBER m_tasks READ tasks NOTIFY tasksChanged)
    Q_PROPERTY(QString sortBy READ sortBy WRITE sortBy NOTIFY sortByChanged)
public:
    int protectPid;
    explicit Controller(QQmlApplicationEngine* engine) : QObject(nullptr), mutex(QMutex::NonRecursive), _engine(engine){
        m_tasks = new TaskList();
        connect(m_tasks, &TaskList::sortByChanged, this, &Controller::sortByChanged);
        emit tasksChanged(m_tasks);
    }
    QString sortBy(){ return m_tasks->sortBy(); }
    void sortBy(QString key){ m_tasks->sort(key); }
    Q_INVOKABLE void reload(){ m_tasks->reload(); }
    Q_INVOKABLE void breadcrumb(QString category, QString message, QString type = "default"){
#ifdef SENTRY
        sentry_breadcrumb(category.toStdString().c_str(), message.toStdString().c_str(), type.toStdString().c_str());
#else
        Q_UNUSED(category);
        Q_UNUSED(message);
        Q_UNUSED(type);
#endif
    }
    TaskList* tasks(){ return m_tasks; }


signals:
    void sortByChanged(QString);
    void tasksChanged(TaskList*);

private:
    QMutex mutex;
    QQmlApplicationEngine* _engine;
    TaskList* m_tasks;
};
