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
#include <QMutableListIterator>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
    sort();
    context->setContextProperty("tasks", QVariant::fromValue(tasks));
}

QList<QObject*> Controller::getTasks(){
    QDir directory("/proc");
    if (!directory.exists() || directory.isEmpty()){
        tasks.clear();
        qCritical() << "Unable to access /proc";
        return tasks;
    }
    directory.setFilter( QDir::Dirs | QDir::NoDot | QDir::NoDotDot);
    auto processes = directory.entryInfoList(QDir::NoFilter, QDir::SortFlag::Name);
    // Get all pids we care about
    QList<int> pids;
    for(QFileInfo fi : processes){
        std::string pid = fi.baseName().toStdString();
        if(!is_uint(pid)){
            continue;
        }
        QFile statm(("/proc/" + pid + "/statm").c_str());
        QTextStream stream(&statm);
        if(!statm.open(QIODevice::ReadOnly | QIODevice::Text)){
            qDebug() << "Unable to open statm for pid " << pid.c_str();
            continue;
        }
        QString content = stream.readAll().trimmed();
        statm.close();
        // Ignore kernel processes
        if(content == "0 0 0 0 0 0 0"){
            continue;
        }
        pids.append(stoi(pid));
    }
    // Remove any pids that we've already got cached,
    // and remove any cached items that no longer exist
    QMutableListIterator<QObject*> i(tasks);
    while(i.hasNext()){
        auto taskItem = reinterpret_cast<TaskItem*>(i.next());
        const auto pid = taskItem->pid();
        if(!pids.contains(pid)){
            i.remove();
        }else{
            pids.removeAll(pid);
        }
    }
    // Create TaskItem instances for all new tasks
    for(auto pid : pids){
        auto taskItem = new TaskItem(pid);
        auto taskPid = taskItem->pid();
        taskItem->setKillable(taskPid != getpid() && taskPid != getppid() && taskPid != protectPid);
        tasks.append(taskItem);
    }
    sort();
    return tasks;
}
void Controller::sort(){
    std::string sortBy = _sortBy.toStdString();
    std::sort(tasks.begin(), tasks.end(), [sortBy](const QObject* a, const QObject* b) -> bool {
        return a->property(sortBy.c_str()) < b->property(sortBy.c_str());
    });
}
