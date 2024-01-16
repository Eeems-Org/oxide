#pragma once

#include <QObject>
#include <QFile>

#include <liboxide/socketpair.h>

using namespace Oxide;

class Connection : public QObject {
    Q_OBJECT

public:
    Connection(QObject* parent, pid_t pid, pid_t pgid);
    ~Connection();

    pid_t pid() const;
    pid_t pgid() const;
    QLocalSocket* socket();
    bool isValid();
    bool isRunning();
    bool signal(int signal);
    bool signalGroup(int signal);
    void pause();
    void resume();
    qint64 write(QByteArray data);
    QString errorString();
    void close();

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
