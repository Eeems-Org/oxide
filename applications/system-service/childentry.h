#pragma once

#include <QObject>

#include "socketpair.h"

class ChildEntry : public QObject {
    Q_OBJECT

public:
    ChildEntry(QObject* parent, pid_t pid, pid_t pgid);
    ~ChildEntry();

    pid_t pid() const;
    pid_t pgid() const;
    QLocalSocket* socket();
    bool isValid();
    bool signal(int signal);
    bool signalGroup(int signal);
    void pause();
    void resume();
    qint64 write(QByteArray data);
    QString errorString();

signals:
    void finished();

private slots:
    void readSocket();

private:
    pid_t m_pid;
    pid_t m_pgid;
    QFile m_process;
    SocketPair m_socketPair;
};
