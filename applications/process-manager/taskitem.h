#ifndef TASKITEM_H
#define TASKITEM_H

#include <QObject>
#include <QFileInfo>
#include <string>

class TaskItem : public QObject {
    Q_OBJECT
public:
    explicit TaskItem(int pid);
    Q_PROPERTY(QString name MEMBER _name NOTIFY nameChanged);
    Q_PROPERTY(int pid MEMBER _pid READ pid NOTIFY pidChanged);
    Q_PROPERTY(int ppid MEMBER _ppid NOTIFY ppidChanged);
    Q_PROPERTY(bool killable MEMBER _killable WRITE setKillable NOTIFY killableChanged);
    void setKillable(bool killable){
        _killable = _ppid && killable;
    }
    int pid(){
        return _pid;
    }
    Q_INVOKABLE bool signal(int signal);

signals:
    void nameChanged();
    void pidChanged();
    void killableChanged();
    void ppidChanged();

private:
    QString _name;
    QString _path;
    int _pid;
    int _ppid;
    bool _killable;
};

#endif // TASKITEM_H
