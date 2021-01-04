#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <QMutex>

class Controller : public QObject
{
    Q_OBJECT

public:
    int protectPid;
    explicit Controller(QQmlApplicationEngine* engine) : QObject(nullptr), mutex(QMutex::NonRecursive), _engine(engine), tasks(){
        _sortBy = "name";
        _lastSortBy = "pid";
    }
    Q_INVOKABLE QList<QObject*> getTasks();
    Q_INVOKABLE void sortBy(QString key);


signals:
    void sortByChanged();
    void reload();

private:
    int is_uint(std::string input);
    QString _sortBy;
    QString _lastSortBy;
    QMutex mutex;
    QQmlApplicationEngine* _engine;
    QList<QObject*> tasks;
    void sort();
};
