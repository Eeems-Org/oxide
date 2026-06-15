#pragma once

#include <libblight/concurrentqueue.h>
#include <libblight/connection.h>
#include <linux/input.h>

#include <QFile>
#include <QImage>
#include <QList>
#include <QLocalSocket>
#include <QObject>
#include <QReadWriteLock>
#include <QRect>
#include <QSocketNotifier>
#include <QString>
#include <QTimer>

#include <memory>
#include <mutex>
#include <queue>

#include "surface.h"
class Surface;

#include "../../shared/liboxide/meta.h"

class Connection : public QObject {
  Q_OBJECT
  Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)

public:
  Connection(pid_t pid, pid_t pgid);
  ~Connection();

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
  std::shared_ptr<Surface> addSurface(
    int fd,
    QRect geometry,
    int stride,
    QImage::Format format,
    double scale
  );
  std::shared_ptr<Surface> getSurface(QString identifier);
  std::shared_ptr<Surface> getSurface(Blight::surface_id_t id);
  QStringList getSurfaceIdentifiers();
  const QList<std::shared_ptr<Surface>> getSurfaces();
  void inputEvents(unsigned int device, const std::vector<input_event>& events);
  void processInputEvents();
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
  QSocketNotifier* m_notifier = nullptr;
  QLocalSocket m_pidNotifier;
  QReadWriteLock surfacesLock;
  std::map<Blight::surface_id_t, std::shared_ptr<Surface>> surfaces;
  std::atomic_flag m_closed;
  QTimer m_notRespondingTimer;
  QTimer m_pingTimer;
  std::atomic_uint pingId;
  std::atomic_ushort m_surfaceId;
  QStringList flags;
  moodycamel::ConcurrentQueue<Blight::event_packet_t> m_inputQueue;
  Blight::event_packet_t m_lastEvent;
  unsigned int m_lastEventOffset;
  std::mutex processQueueMutex;

  void
  ack(Blight::message_ptr_t message, unsigned int size, Blight::data_t data);
};
