#ifndef TASKLIST_H
#define TASKLIST_H

#include <QAbstractListModel>
#include <QDir>

#include "taskitem.h"

class TaskList : public QAbstractListModel
{
    Q_OBJECT
public:
    TaskList() : QAbstractListModel(nullptr) {
        reload();
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        Q_UNUSED(section)
        Q_UNUSED(orientation)
        Q_UNUSED(role)
        return QVariant();
    }
    QString sortBy(){ return _sortBy; }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override{
        if(parent.isValid()){
            return 0;
        }
        return taskItems.length();
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override{
        if(!index.isValid()){
            return QVariant();
        }
        if(index.column() > 0){
            return QVariant();
        }
        if(index.row() >= taskItems.length()){
            return QVariant();
        }
        if(role != Qt::DisplayRole){
            return QVariant();
        }
        return QVariant::fromValue(taskItems[index.row()]);
    }
    void append(TaskItem* taskItem){
        if(get(taskItem->pid()) != nullptr){
            return;
        }
        beginInsertRows(QModelIndex(), taskItems.length(), taskItems.length());
        taskItems.append(taskItem);
        endInsertRows();
        emit updated();
    }
    void append(int pid){
        if(get(pid) != nullptr){
            return;
        }
        beginInsertRows(QModelIndex(), taskItems.length(), taskItems.length());
        taskItems.append(new TaskItem(pid, this));
        endInsertRows();
        emit updated();
    }
    TaskItem* get(int pid){
        for(auto taskItem : taskItems){
            if(pid == taskItem->pid()){
                return taskItem;
            }
        }
        return nullptr;
    }
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override{
        Q_UNUSED(column)
        Q_UNUSED(order)
        emit layoutAboutToBeChanged();
        std::sort(taskItems.begin(), taskItems.end(), [=](TaskItem* a, TaskItem* b) -> bool {
            if(order == Qt::DescendingOrder){
                auto temp = a;
                a = b;
                b = temp;
            }
            auto aval = a->property(_sortBy.toStdString().c_str());
            auto bval = b->property(_sortBy.toStdString().c_str());
            auto sortBy = _sortBy;
            if(aval == bval){
                sortBy = _lastSortBy;
                aval = a->property(_lastSortBy.toStdString().c_str());
                bval = b->property(_lastSortBy.toStdString().c_str());
            }
            if(sortBy == "name"){
                return aval.toString() < bval.toString();
            }
            if(sortBy == "pid" || sortBy == "ppid" || sortBy == "cpu"){
                return aval.toInt() < bval.toInt();
            }
            if(sortBy == "mem"){
                auto aparts = aval.toString().split(" ", Qt::KeepEmptyParts);
                auto bparts = bval.toString().split(" ", Qt::KeepEmptyParts);
                if(aparts[1] == bparts[1]){
                    return aparts[0].toFloat() < bparts[0].toFloat();
                }
                return aparts[1] < bparts[1];
            }
            return false;
        });
        emit layoutChanged();
    }
    Q_INVOKABLE void sort(QString sortBy){
        if(_sortBy != sortBy){
            _sortOrder = Qt::AscendingOrder;
            _lastSortBy = _sortBy;
            _sortBy = sortBy;
            emit sortByChanged(_sortBy);
        }else if(_sortOrder == Qt::AscendingOrder){
            _sortOrder = Qt::DescendingOrder;
        }else{
            _sortOrder = Qt::AscendingOrder;
        }
        sort(0, _sortOrder);
        for(auto taskItem : taskItems){
            emit taskItem->nameChanged(taskItem->name());
        }
    }
    Q_INVOKABLE void clear(){
        beginRemoveRows(QModelIndex(), 0, taskItems.length());
        for(auto taskItem : taskItems){
            if(taskItem != nullptr){
                delete taskItem;
            }
        }
        taskItems.clear();
        endRemoveRows();
        emit updated();
    }
    Q_INVOKABLE void remove(int pid){
        QMutableListIterator<TaskItem*> i(taskItems);
        while(i.hasNext()){
            auto taskItem = i.next();
            if(taskItem->pid() == pid){
                beginRemoveRows(QModelIndex(), taskItems.indexOf(taskItem), taskItems.indexOf(taskItem));
                i.remove();
                delete taskItem;
                endRemoveRows();
            }
        }
        emit updated();
    }
    int removeAll(TaskItem* taskItem) {
        QMutableListIterator<TaskItem*> i(taskItems);
        int count = 0;
        while(i.hasNext()){
            auto item = i.next();
            if(item == taskItem){
                beginRemoveRows(QModelIndex(), taskItems.indexOf(item), taskItems.indexOf(item));
                i.remove();
                delete item;
                endRemoveRows();
                count++;
            }
        }
        emit updated();
        return count;
    }
    int length() { return taskItems.length(); }
    bool empty() { return taskItems.empty(); }
    void reload(){
        QMutableListIterator<TaskItem*> i(taskItems);
        while(i.hasNext()){
            auto taskItem = i.next();
            if(taskItem->exists()){
                taskItem->reload();
                continue;
            }
            beginRemoveRows(QModelIndex(), taskItems.indexOf(taskItem), taskItems.indexOf(taskItem));
            i.remove();
            delete taskItem;
            endRemoveRows();
        }
        QDir directory("/proc");
        if (!directory.exists() || directory.isEmpty()){
            qCritical() << "Unable to access /proc";
            return;
        }
        directory.setFilter( QDir::Dirs | QDir::NoDot | QDir::NoDotDot);
        auto processes = directory.entryInfoList(QDir::NoFilter, QDir::SortFlag::Name);
        // Get all pids we care about
        for(QFileInfo fi : processes){
            std::string pid = fi.baseName().toStdString();
            if(!is_uint(pid)){
                continue;
            }
            if(get(stoi(pid)) != nullptr){
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
            append(stoi(pid));
        }
        sort(0, _sortOrder);
    }
signals:
    void updated();
    void sortByChanged(QString);

private:
    QList<TaskItem*> taskItems;
    int is_uint(std::string input){
        unsigned int i;
        for (i=0; i < input.length(); i++){
            if(!isdigit(input.at(i))){
                return 0;
            }
        }
        return 1;
    }
    QString _sortBy = "name";
    QString _lastSortBy = "pid";
    Qt::SortOrder _sortOrder = Qt::AscendingOrder;
};

#endif // TASKLIST_H
