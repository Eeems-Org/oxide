#pragma once

#include <QObject>
#include <QFile>
#include <QSocketNotifier>
#include <libblight/connection.h>
#include <linux/input.h>

#include "surface.h"

#include "../../shared/liboxide/meta.h"

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
    int socketDescriptor();
    int inputReadSocketDescriptor();
    int inputWriteSocketDescriptor();
    bool isValid();
    bool isRunning();
    bool signal(int signal);
    bool signalGroup(int signal);
    void pause();
    void resume();
    void close();
    std::shared_ptr<Surface> addSurface(int fd, QRect geometry, int stride, QImage::Format format);
    std::shared_ptr<Surface> getSurface(QString identifier);
    QStringList getSurfaces();
    void inputEvent(const input_event& event);

signals:
    void finished();
    void focused();

private slots:
    void readSocket();

private:
    pid_t m_pid;
    pid_t m_pgid;
    QFile m_process;
    int m_clientFd;
    int m_serverFd;
    int m_clientInputFd;
    int m_serverInputFd;
    QSocketNotifier* m_notifier;
    QList<std::shared_ptr<Surface>> surfaces;
};
