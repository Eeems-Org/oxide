#ifndef TASKITEM_H
#define TASKITEM_H

#include <QObject>
#include <QFileInfo>
#include <string>

class TaskItem : public QObject {
    Q_OBJECT
public:
    explicit TaskItem(std::string pid);
    Q_PROPERTY(QString name MEMBER _name NOTIFY nameChanged)
    Q_PROPERTY(QString pid MEMBER _pid NOTIFY pidChanged)
    bool ok();
    std::string getprop(std::string name);

signals:
    void nameChanged();
    void pidChanged();

private:
    QString _name;
    QString _path;
    QString _pid;
};

#endif // TASKITEM_H
