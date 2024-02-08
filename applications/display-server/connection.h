#pragma once

#include <QObject>
#include <QFile>
#include <QSocketNotifier>
#include <QLocalSocket>
#include <QTimer>
#include <QMutex>

#include <libblight/connection.h>
#include <linux/input.h>

#include "surface.h"
class Surface;

#include "../../shared/liboxide/meta.h"

class Connection : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)

public:
    Connection(pid_t pid, pid_t pgid);
    ~Connection();

    void processRemovedSurfaces();
    QString id();
    pid_t pid() const;
    pid_t pgid() const;
    int socketDescriptor();
    int inputSocketDescriptor();
    bool isValid();
    bool isRunning();
    bool isStopped();
    bool signal(int signal);
    bool signalGroup(int signal);
    void pause();
    void resume();
    std::shared_ptr<Surface> addSurface(int fd, QRect geometry, int stride, QImage::Format format);
    std::shared_ptr<Surface> getSurface(QString identifier);
    std::shared_ptr<Surface> getSurface(Blight::surface_id_t id);
    QStringList getSurfaceIdentifiers();
    const QList<std::shared_ptr<Surface>> getSurfaces();
    void inputEvents(unsigned int device, const std::vector<input_event>& events);
    bool has(const QString& flag);
    void set(const QString& flag);
    void unset(const QString& flag);

signals:
    void finished();
    void focused();

public slots:
    void close();

private slots:
    void readSocket();
    void notResponding();
    void ping();

private:
    pid_t m_pid;
    pid_t m_pgid;
    int m_pidFd;
    QFile m_process;
    int m_clientFd;
    int m_serverFd;
    int m_clientInputFd;
    int m_serverInputFd;
    QSocketNotifier* m_notifier;
    QLocalSocket m_pidNotifier;
    std::map<Blight::surface_id_t, std::shared_ptr<Surface>> surfaces;
    QMutex removedMutex;
    std::vector<std::shared_ptr<Surface>> removedSurfaces;
    std::atomic_flag m_closed;
    QTimer m_notRespondingTimer;
    QTimer m_pingTimer;
    std::atomic_uint pingId;
    std::atomic_ushort m_surfaceId;
    QStringList flags;

    void ack(Blight::message_ptr_t message, unsigned int size, Blight::data_t data);
};
