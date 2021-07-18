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

class TaskItem : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER _name READ name WRITE setName NOTIFY nameChanged);
    Q_PROPERTY(int pid MEMBER _pid READ pid WRITE setPid NOTIFY pidChanged);
    Q_PROPERTY(int ppid MEMBER _ppid READ ppid WRITE setPpid NOTIFY ppidChanged);
    Q_PROPERTY(bool killable MEMBER _killable READ killable WRITE setKillable NOTIFY killableChanged);
    Q_PROPERTY(int cpu MEMBER _cpu READ cpu WRITE setCpu NOTIFY cpuChanged);
public:
    TaskItem(int pid, QObject* parent) : QObject(parent), _pid(pid) { reload(); }
    int protectPid;
    Q_INVOKABLE bool signal(int signal) { return kill(_pid, signal); }
    bool exists(){ return _pid > 0 && QFile::exists(folder()); }
    void reload(){
        if(!exists()){
            return;
        }
        QString file_content = readFile(folder() + "/status");
        _name = parseRegex(file_content,QRegularExpression("^Name:\\t+(\\w+)"));
        file_content = readFile(folder() + "/stat");
        auto columns = file_content.split(" ", Qt::KeepEmptyParts);
        _ppid = columns[3].toInt();
        auto hertz = sysconf(_SC_CLK_TCK);
        int seconds = readFile("/proc/uptime").toInt() - (columns[21].toInt() / hertz);
        _cpu = 100 * (((columns[13].toInt() + columns[14].toInt()) / hertz) / seconds);
        _killable = _pid != getpid() && _pid != getppid();
    }

    QString name() { return _name; }
    int pid() { return _pid; }
    int ppid() { return _ppid; }
    bool killable() { return _killable; }
    int cpu() { return _cpu; }

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
        _cpu = cpu;
        emit cpuChanged(_cpu);
    }

signals:
    void nameChanged(QString);
    void pidChanged(int);
    void killableChanged(bool);
    void ppidChanged(int);
    void cpuChanged(int);

private:
    QString _name;
    QString _path;
    int _pid;
    int _ppid;
    bool _killable;
    int _cpu;
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
    QString folder() { return QString::fromStdString("/proc" + std::to_string(_pid)); }
};

#endif // TASKITEM_H
