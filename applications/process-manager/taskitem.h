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
    Q_PROPERTY(QString mem MEMBER _mem READ mem WRITE setMem NOTIFY memChanged);

public:
    TaskItem(int pid, QObject* parent)
    : QObject(parent),
      _name(""),
      _pid(pid),
      _ppid(0),
      _killable(true),
      _cpu(0),
      _mem("0b"),
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
        auto file_contents = readFile("/proc/stat");
        if(file_contents == ""){
            return;
        }
        QStringList statCols = file_contents.split(" ", Qt::SkipEmptyParts);
        file_contents = readFile(folder() + "/cmdline");
        if(file_contents == ""){
            return;
        }
        auto name = file_contents
            .split("\n", Qt::SkipEmptyParts)
            .first()
            .split("/", Qt::SkipEmptyParts)
            .last();
        setName(name);
        file_contents = readFile(folder() + "/stat");
        if(file_contents == ""){
            return;
        }
        auto procCols = file_contents
            .split("\n", Qt::SkipEmptyParts)
            .first()
            .split(" ", Qt::SkipEmptyParts);
        file_contents = readFile(folder() + "/smaps");
        if(file_contents == ""){
            return;
        }
        setPpid(procCols[3].toInt());
        if(_killable){
            setKillable(_pid != getpid() && _pid != getppid());
        }
        double mem = 0;
        for(auto line : file_contents.split("\n", Qt::SkipEmptyParts).filter(QRegularExpression("^Pss:"))){
            auto cols = line.split(" ", Qt::SkipEmptyParts);
            mem += cols[1].toUInt();
        }
        QStringListIterator i(QStringList() << "MiB" << "GiB" << "TiB");
        QString unit("KiB");
        while(mem >= 1024 && i.hasNext()){
            unit = i.next();
            mem /= 1024;
        }
        setMem(QString().setNum(mem, 'f', 1) + " " + unit);
        // user + nice + system + idle + iowait + irq + softirq
        ulong total_cpu_usage = statCols[0].toUInt() + statCols[1].toUInt() + statCols[2].toUInt()
            + statCols[3].toUInt() + statCols[4].toUInt() + statCols[5].toUInt() + statCols[6].toUInt()
            + statCols[7].toUInt();
        // utime + stime + cutime + cstime + delayacct_blkio_ticks
        ulong proc_usage = procCols[13].toUInt() + procCols[14].toUInt() + procCols[15].toUInt()
            + procCols[16].toUInt() + procCols[41].toUInt();
        if(_totalCpuUsage == 0){
            _totalCpuUsage = total_cpu_usage;
            _procUsage = proc_usage;
            return;
        }
        auto actual_proc_usage = proc_usage - _procUsage;
        auto actual_cpu_usage = total_cpu_usage - _totalCpuUsage;
        setCpu(100 * actual_proc_usage / actual_cpu_usage);
        _totalCpuUsage = total_cpu_usage;
        _procUsage = proc_usage;
    }

    QString name() { return _name; }
    int pid() { return _pid; }
    int ppid() { return _ppid; }
    bool killable() { return _killable; }
    uint cpu() { return _cpu; }
    QString mem() { return _mem; }

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
    void setMem(QString mem){
        _mem = mem;
        emit memChanged(_mem);
    }

signals:
    void nameChanged(QString);
    void pidChanged(int);
    void killableChanged(bool);
    void ppidChanged(int);
    void cpuChanged(uint);
    void memChanged(QString);

private:
    QString _name;
    QString _path;
    int _pid;
    int _ppid;
    bool _killable;
    uint _cpu;
    QString _mem;
    ulong _totalCpuUsage;
    ulong _procUsage;
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
