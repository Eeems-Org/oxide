#pragma once

#include <QObject>
#include <QFile>
#include <liboxide/socketpair.h>
#include <libblight/connection.h>

#include "surface.h"

#include "../../shared/liboxide/meta.h"

using namespace Oxide;

class Connection : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_SURFACE_INTERFACE)

public:
    Connection(QObject* parent, pid_t pid, pid_t pgid);
    ~Connection();

    QString id();

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
    Surface* addSurface(int fd, QRect geometry, int stride, QImage::Format format);
    Surface* getSurface(QString identifier);

signals:
    void finished();

private slots:
    void readSocket();

private:
    pid_t m_pid;
    pid_t m_pgid;
    QFile m_process;
    SocketPair m_socketPair;
    QList<Surface*> surfaces;
};
