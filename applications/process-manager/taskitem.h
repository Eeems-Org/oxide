#ifndef TASKITEM_H
#define TASKITEM_H

#include <QObject>
#include <QFileInfo>
#include <string>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

#include <unistd.h>
#include <signal.h>
#include <math.h>

class TaskItem : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER _name READ name WRITE setName NOTIFY nameChanged);
    Q_PROPERTY(int pid MEMBER _pid READ pid WRITE setPid NOTIFY pidChanged);
    Q_PROPERTY(int ppid MEMBER _ppid READ ppid WRITE setPpid NOTIFY ppidChanged);
    Q_PROPERTY(bool killable MEMBER _killable READ killable WRITE setKillable NOTIFY killableChanged);
    Q_PROPERTY(uint cpu MEMBER _cpu READ cpu WRITE setCpu NOTIFY cpuChanged);
public:
    TaskItem(int pid, QObject* parent)
    : QObject(parent),
      _name(""),
      _pid(pid),
      _ppid(0),
      _killable(true),
      _cpu(0),
      _processors(sysconf(_SC_NPROCESSORS_ONLN)),
      _totalCpuUsage(0),
      _procUsage(0) {
        reload();
    }
    int protectPid;
    Q_INVOKABLE bool signal(int signal) { return kill(_pid, signal); }
    bool exists(){ return _pid > 0 && QFile::exists(folder()); }
    void reload(){
        if(!exists()){
            return;
        }
        setName(readFile(folder() + "/comm"));
        auto procCols = readFile(folder() + "/stat").split(" ", Qt::SkipEmptyParts);
        setPpid(procCols[3].toInt());
        if(_killable){
            setKillable(_pid != getpid() && _pid != getppid());
        }
        QStringList statCols = readFile("/proc/stat").split(" ", Qt::SkipEmptyParts);
        auto total_cpu_usage = statCols[0].toInt() + statCols[1].toUInt() + statCols[2].toUInt() + statCols[3].toUInt();
        auto proc_usage = procCols[13].toUInt() + procCols[14].toUInt();
        if(_totalCpuUsage == 0){
            _totalCpuUsage = total_cpu_usage;
            _procUsage = proc_usage;
            return;
        }
        setCpu(_processors * (proc_usage - _procUsage) * 100 / (total_cpu_usage - _totalCpuUsage));
        _totalCpuUsage = total_cpu_usage;
        _procUsage = proc_usage;
    }

    QString name() { return _name; }
    int pid() { return _pid; }
    int ppid() { return _ppid; }
    bool killable() { return _killable; }
    uint cpu() { return _cpu; }

    void setName(QString name){
        _name = name;
        emit nameChanged(_name);
    }
    void setPid(int pid){
        _pid = pid;
        emit pidChanged(_pid);
    }
    void setPpid(int ppid){
        _ppid = ppid;
        emit ppidChanged(_ppid);
    }
    void setKillable(bool killable){
        _killable = _ppid && killable;
        emit killableChanged(_killable);
    }
    void setCpu(int cpu){
        if(cpu < 0){
            _cpu = 0;
        }else if(cpu > 100){
            _cpu = 100;
        }else{
            _cpu = cpu;
        }
        emit cpuChanged(_cpu);
    }

signals:
    void nameChanged(QString);
    void pidChanged(int);
    void killableChanged(bool);
    void ppidChanged(int);
    void cpuChanged(uint);

private:
    QString _name;
    QString _path;
    int _pid;
    int _ppid;
    bool _killable;
    uint _cpu;
    short _processors;
    uint _totalCpuUsage;
    uint _procUsage;
    QString readFile(const QString &path){
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)){
            qDebug()<<"Error reading file: " + path;
            return "";
        }
        auto data = file.readAll();
        file.close();
        return data;
    }
    QString parseRegex(QString &file_content, const QRegularExpression &reg){
        QRegularExpressionMatchIterator i = reg.globalMatch(file_content);
        if (!i.isValid()){
             return "";
        }
        QString result="";
        while (i.hasNext()){
              QRegularExpressionMatch match = i.next();
              result=match.captured(1);
        }
        return result;
    }
    QString folder() { return QString::fromStdString("/proc/" + std::to_string(_pid)); }
};

#endif // TASKITEM_H
