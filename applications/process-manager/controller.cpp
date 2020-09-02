#include "controller.h"
#include "taskitem.h"
#include <QIODevice>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QQmlContext>
#include <QDir>
#include <QSet>
#include <QDebug>
#include <stdio.h>
#include <stdlib.h>

int Controller::is_uint(std::string input){
    unsigned int i;
    for (i=0; i < input.length(); i++){
        if(!isdigit(input.at(i))){
            return 0;
        }
    }
    return 1;
}

void Controller::sortBy(QString key){
    _sortBy = key;
    QQmlContext* context = _engine->rootContext();
    context->setContextProperty("tasks", QVariant::fromValue(getTasks()));
}

QList<QObject*> Controller::getTasks(){
    QList<QObject*> result;
    QDir directory("/proc");
    if (!directory.exists() || directory.isEmpty()){
        qCritical() << "Unable to access /proc";
        return result;
    }
    directory.setFilter( QDir::Dirs | QDir::NoDot | QDir::NoDotDot);
    auto processes = directory.entryInfoList(QDir::NoFilter, QDir::SortFlag::Name);
    for(QFileInfo fi : processes){
        std::string pid = fi.baseName().toStdString();
        if(is_uint(pid)){
            TaskItem* task = new TaskItem(pid);
            if(task->ok()){
                result.append(task);
            }
        }
    }
    std::string sortBy = _sortBy.toStdString();
    std::sort(result.begin(), result.end(), [sortBy](const QObject* a, const QObject* b) -> bool {
        if(sortBy == "name"){
            return a->property("name") < b->property("name");
        }
        return a->property("pid") < b->property("pid");
    });
    return result;
}
