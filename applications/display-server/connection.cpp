#include "connection.h"

#include <assert.h>
#include <libblight/socket.h>
#include <liboxide/debug.h>
#include <liboxide/signalhandler.h>
#include <memory>
#include <mutex>
#include <signal.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <cstring>

#include "dbusinterface.h"

#ifdef EPAPER
#include <mxcfb.h>

#include "guithread.h"
#endif
#include "surface.h"

#define C_DEBUG(msg) O_DEBUG("[" << id() << "]" << msg)
#define C_WARNING(msg) O_WARNING("[" << id() << "]" << msg)
#define C_INFO(msg) O_INFO("[" << id() << "]" << msg)

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

static int
pidfd_open(pid_t pid, unsigned int flags) {
  return syscall(SYS_pidfd_open, pid, flags);
}

Connection::Connection(pid_t pid, pid_t pgid)
  : QObject()
  , m_pid{pid}
  , m_pgid{pgid}
  , m_closed{false}
  , pingId{0}
  , m_surfaceId{0} {
  m_pidFd = pidfd_open(m_pid, 0);
  if (m_pidFd < 0) {
    O_WARNING(std::strerror(errno));
  } else {
    connect(
      &m_pidNotifier, &QLocalSocket::disconnected, this, &Connection::close
    );
    m_pidNotifier.setSocketDescriptor(
      m_pidFd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly
    );
  }
  int fds[2];
  if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) == -1) {
    C_WARNING("Unable to open socket pair:" << strerror(errno));
    fds[0] = fds[1] = -1;
  }
  m_clientFd = fds[0];
  m_serverFd = fds[1];
  if (m_serverFd > 0) {
    m_notifier = new QSocketNotifier(m_serverFd, QSocketNotifier::Read, this);
    connect(
      m_notifier, &QSocketNotifier::activated, this, &Connection::readSocket
    );
  }

  m_pingTimer.setParent(this);
  m_pingTimer.setTimerType(Qt::PreciseTimer);
  m_pingTimer.setInterval(1000);
  m_pingTimer.setSingleShot(true);
  connect(&m_pingTimer, &QTimer::timeout, this, &Connection::ping);
  m_pingTimer.start();

  m_notRespondingTimer.setParent(this);
  m_notRespondingTimer.setTimerType(Qt::PreciseTimer);
  m_notRespondingTimer.setInterval(m_pingTimer.interval() * 2);
  m_notRespondingTimer.setSingleShot(true);
  connect(
    &m_notRespondingTimer, &QTimer::timeout, this, &Connection::notResponding
  );
  m_notRespondingTimer.start();
  C_INFO("Connection created");
}

Connection::~Connection() {
  close();
  {
    QWriteLocker _locker(&surfacesLock);
    surfaces.clear();
  }
  if (m_notifier != nullptr) {
    m_notifier->deleteLater();
    m_notifier = nullptr;
  }
  for (auto& [device, buf] : m_inputBuffers) {
    if (buf.buffer != nullptr) {
      buf.buffer->interrupt();
      buf.buffer->interrupt();
      munmap(buf.buffer, sizeof(Blight::EvdevRingBuffer));
      buf.buffer = nullptr;
    }
    if (buf.fd >= 0) {
      ::close(buf.fd);
      buf.fd = -1;
    }
  }
  m_inputBuffers.clear();
  ::close(m_clientFd);
  ::close(m_serverFd);
  ::close(m_pidFd);
  C_INFO("Connection destroyed");
}

QString
Connection::id() {
  return QString("connection/%1").arg(m_pid);
}

pid_t
Connection::pid() const {
  return m_pid;
}

pid_t
Connection::pgid() const {
  return m_pgid;
}

int
Connection::socketDescriptor() {
  return m_clientFd;
}

int
Connection::inputFd(unsigned short device) {
  if (m_inputBuffers.contains(device)) {
    return m_inputBuffers[device].fd;
  }
  if (!QFile::exists(QStringLiteral("/dev/input/event%1").arg(device))) {
    return -1;
  }
  auto [fd, buffer] = Blight::EvdevRingBuffer::createSharedMemory(true);
  if (fd < 0 || buffer == nullptr) {
    C_WARNING("Failed to create input buffer for event" << device)
    return -1;
  }
  m_inputBuffers[device] = {fd, buffer};
  buffer->insert({.type = EV_SYN, .code = SYN_DROPPED});
  C_DEBUG("Created input buffer for event" << device << " (fd=" << fd << ")");
  return fd;
}

bool
Connection::isValid() {
  return m_clientFd > 0 && m_serverFd > 0 && isRunning();
}

bool
Connection::isRunning() {
  return getpgid(m_pid) != -1;
}

bool
Connection::isStopped() {
  QFile file(QString("/proc/%1/stat").arg(m_pid));
  if (!file.open(QFile::ReadOnly)) {
    O_WARNING("Failed checking process status" << file.errorString());
    return false;
  }
  auto data = file.readAll();
  int idx = data.lastIndexOf(')');
  if (idx < 0 || idx + 2 >= data.size()) {
    return false;
  }
  return data.at(idx + 2) == 'T';
}

bool
Connection::signal(int signal) {
  if (!isRunning()) {
    errno = ESRCH;
    return false;
  }
  return ::kill(m_pid, signal) != -1;
}

bool
Connection::signalGroup(int signal) {
  if (!isRunning()) {
    errno = ESRCH;
    return false;
  }
  return ::kill(-m_pgid, signal) != -1;
}

void
Connection::pause() {
  // TODO - Send pause request over socket instead
  QElapsedTimer timer;
  bool replied = false;
  auto conn =
    connect(signalHandler, &Oxide::SignalHandler::sigUsr2, [&timer, &replied] {
      replied = true;
      timer.invalidate();
    });
  signalGroup(SIGUSR2);
  timer.start();
  while (timer.isValid() && !timer.hasExpired(1000)) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
  }
  signalHandler->disconnect(conn);
  if (replied) {
    return;
  }
  signalGroup(SIGSTOP);
  siginfo_t info;
  waitid(P_PID, m_pid, &info, WSTOPPED);
}

void
Connection::resume() {
  // TODO - Send resume request over socket instead
  QElapsedTimer timer;
  bool replied = false;
  auto conn =
    connect(signalHandler, &Oxide::SignalHandler::sigUsr1, [&timer, &replied] {
      replied = true;
      timer.invalidate();
    });
  signalGroup(SIGUSR1);
  timer.start();
  while (timer.isValid() && !timer.hasExpired(1000)) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
  }
  signalHandler->disconnect(conn);
  if (replied) {
    return;
  }
  signalGroup(SIGCONT);
  siginfo_t info;
  waitid(P_PID, m_pid, &info, WCONTINUED);
}

void
Connection::close() {
  if (!m_closed.test_and_set()) {
    m_pingTimer.stop();
    m_notRespondingTimer.stop();
    {
      QReadLocker _locker(&surfacesLock);
      for (auto& item : surfaces) {
        item.second->removed();
      }
    }
    {
      QWriteLocker _locker(&surfacesLock);
      surfaces.clear();
    }
    emit finished();
  }
}

std::shared_ptr<Surface>
Connection::addSurface(
  int fd,
  QRect geometry,
  int stride,
  QImage::Format format,
  double scale
) {
  // TODO - add validation that id is never 0, and that it doesn't point to an
  // existsing surface
  auto surfaceId = ++m_surfaceId;
  std::shared_ptr<Surface> surface;
  try {
    surface = std::shared_ptr<Surface>(
      new Surface(this, fd, surfaceId, geometry, stride, format, scale),
      [](Surface* s) { s->deleteLater(); }
    );
  } catch (const std::bad_alloc&) {
    C_WARNING("Unable to add surface, out of memory");
    return surface;
  }
  if (!surface->isValid()) {
    return surface;
  }
  {
    QWriteLocker _locker(&surfacesLock);
    surfaces.insert_or_assign(surfaceId, surface);
  }
  dbusInterface->sortZ();
  surface->repaint();
  return surface;
}

std::shared_ptr<Surface>
Connection::getSurface(QString identifier) {
  QReadLocker _locker(&surfacesLock);
  for (auto& item : surfaces) {
    auto& surface = item.second;
    if (surface == nullptr) {
      continue;
    }
    if (surface->id() == identifier) {
      return surface;
    }
  }
  return nullptr;
}

std::shared_ptr<Surface>
Connection::getSurface(Blight::surface_id_t id) {
  QReadLocker _locker(&surfacesLock);
  if (!surfaces.contains(id)) {
    return nullptr;
  }
  return surfaces[id];
}

QStringList
Connection::getSurfaceIdentifiers() {
  QStringList identifiers;
  QReadLocker _locker(&surfacesLock);
  for (auto& item : surfaces) {
    auto& surface = item.second;
    if (surface == nullptr) {
      continue;
    }
    identifiers.append(surface->id());
  }
  return identifiers;
}

const QList<std::shared_ptr<Surface>>
Connection::getSurfaces() {
  QList<std::shared_ptr<Surface>> values;
  QReadLocker _locker(&surfacesLock);
  for (auto& item : surfaces) {
    auto& surface = item.second;
    if (surface != nullptr) {
      values.append(surface);
    }
  }
  return values;
}

void
Connection::inputEvents(
  unsigned int device,
  const std::vector<input_event>& events
) {
  if (!isRunning() || isStopped()) {
    dbusInterface->sortZ();
    return;
  }
  if (!events.size()) {
    return;
  }
  if (!m_inputBuffers.contains(device)) {
    return;
  }
  auto* buffer = m_inputBuffers[device].buffer;
  if (buffer == nullptr) {
    C_WARNING("event" << device << " buffer is nullptr");
    return;
  }
  C_DEBUG("Writing" << events.size() << "input events to device" << device);
  for (const auto& ev : events) {
    buffer->insert(ev);
  }
}

bool
Connection::has(const QString& flag) {
  return flags.contains(flag);
}

void
Connection::set(const QString& flag) {
  if (!has(flag)) {
    flags.append(flag);
  }
}

void
Connection::unset(const QString& flag) {
  flags.removeAll(flag);
}

void
Connection::readSocket() {
  if (m_notifier == nullptr) {
    C_DEBUG("Connection already closed, discarding data");
    return;
  }
  m_notifier->setEnabled(false);
#ifdef ACK_DEBUG
  C_DEBUG("Data received");
#endif
  while (true) {
    auto message = Blight::message_t::from_socket(m_serverFd);
    if (
      message == nullptr || message->header.type == Blight::MessageType::Invalid
    ) {
      break;
    }
#ifndef ACK_DEBUG
    if (
      message->header.type != Blight::MessageType::Ping &&
      message->header.type != Blight::MessageType::Ack
    ) {
#endif
      C_DEBUG(
        "Handling message:" << "type=" << message->header.type
                            << ", ackid=" << message->header.ackid << ", size="
                            << message->header.size << ", socket=" << m_serverFd
      );
#ifndef ACK_DEBUG
    }
#endif
    bool do_ack = true;
    unsigned int ack_size = 0;
    Blight::data_t ack_data = nullptr;
    std::function<void()> ack_free = NULL;
    switch (message->header.type) {
      case Blight::MessageType::Repaint: {
        auto repaint = Blight::repaint_t::from_message(message.get());
        C_DEBUG(
          "Repaint requested:" << QString("%1 (%2,%3) %4x%5 %6 %7 %8 %9")
                                    .arg(repaint.identifier)
                                    .arg(repaint.x)
                                    .arg(repaint.y)
                                    .arg(repaint.width)
                                    .arg(repaint.height)
                                    .arg(repaint.waveform)
                                    .arg(repaint.contenttype)
                                    .arg(repaint.mode)
                                    .arg(repaint.marker)
                                    .toStdString()
                                    .c_str()
        );
        auto surface = getSurface(repaint.identifier);
        if (surface == nullptr) {
          C_WARNING("Could not find surface" << repaint.identifier);
          break;
        }
        QRect rect(repaint.x, repaint.y, repaint.width, repaint.height);
#ifdef EPAPER
        guiThread->enqueue(
          surface,
          rect,
          repaint.waveform,
          repaint.contenttype,
          repaint.mode,
          repaint.marker,
          false,
          [message, this] { ack(message, 0, nullptr); }
        );
        do_ack = false;
#else
        emit surface->update(rect);
#endif
        break;
      }
      case Blight::MessageType::Move: {
        auto move = Blight::move_t::from_message(message.get());
        C_DEBUG(
          "Move requested:" << QString("%3 (%1,%2)")
                                 .arg(move.x)
                                 .arg(move.y)
                                 .arg(move.identifier)
                                 .toStdString()
                                 .c_str()
        );
        auto surface = getSurface(move.identifier);
        if (surface == nullptr) {
          C_WARNING("Could not find surface" << move.identifier);
          break;
        }
#ifdef EPAPER
        auto rect = surface->geometry();
        surface->move(move.x, move.y);
        guiThread->enqueue(
          surface,
          surface->rawRect(),
          Blight::WaveformMode::UI,
          Blight::ContentType::Color,
          Blight::UpdateMode::PartialUpdate,
          message->header.ackid,
          false,
          [message, this] { ack(message, 0, nullptr); }
        );
        guiThread->enqueue(
          nullptr,
          rect,
          Blight::WaveformMode::UI,
          Blight::ContentType::Color,
          Blight::UpdateMode::PartialUpdate,
          message->header.ackid,
          true,
          nullptr
        );
        do_ack = false;
#else
        surface->move(move.header.x, move.header.y);
#endif
        break;
      }
      case Blight::MessageType::Info: {
        auto identifier = (Blight::surface_id_t)*message->data.get();
        C_DEBUG("Info requested:" << identifier);
        std::shared_ptr<Surface> surface;
        {
          QReadLocker _locker(&surfacesLock);
          if (!surfaces.contains(identifier)) {
            C_WARNING("Could not find surface" << identifier);
            break;
          }
          surface = surfaces[identifier];
        }
        auto geometry = surface->rawGeometry();
        try {
          ack_data =
            reinterpret_cast<Blight::data_t>(new Blight::surface_info_t{
              {.x = geometry.x(),
               .y = geometry.y(),
               .width = (unsigned int)geometry.width(),
               .height = (unsigned int)geometry.height(),
               .stride = surface->stride(),
               .format = (Blight::Format)surface->format(),
               .scale = surface->scale()}
          });
          ack_size = sizeof(Blight::surface_info_t);
        } catch (const std::bad_alloc&) {
          C_WARNING("Could not allocate ack_data, not enough memory");
        }
        break;
      }
      case Blight::MessageType::Delete: {
        auto identifier = (Blight::surface_id_t)*message->data.get();
        C_DEBUG("Delete requested:" << identifier);
        std::shared_ptr<Surface> surface;
        {
          QReadLocker _locker(&surfacesLock);
          if (!surfaces.contains(identifier)) {
            C_WARNING("Could not find surface" << identifier);

            break;
          }
          surface = surfaces[identifier];
        }
        surface->removed();
        {
          QWriteLocker _locker(&surfacesLock);
          surfaces.erase(identifier);
        }
        guiThread->notify();
        break;
      }
      case Blight::MessageType::List: {
        C_DEBUG("List requested");
        std::vector<Blight::surface_id_t> list;
        {
          QReadLocker _locker(&surfacesLock);
          for (auto& item : surfaces) {
            if (item.second != nullptr) {
              list.push_back(item.first);
            }
          }
        }
        ack_size = list.size() * sizeof(decltype(list)::value_type);
        ack_data = (Blight::data_t)malloc(ack_size);
        memcpy(ack_data, list.data(), ack_size);
        if (ack_size != 0 && ack_data == nullptr) {
          C_WARNING("Could not allocate ack_data, not enough memory");
          break;
        }
        ack_free = [&ack_data] { free(ack_data); };
        break;
      }
      case Blight::MessageType::Raise: {
        auto identifier = (Blight::surface_id_t)*message->data.get();
        C_DEBUG("Raise requested:" << identifier);
        std::shared_ptr<Surface> surface;
        {
          QReadLocker _locker(&surfacesLock);
          if (!surfaces.contains(identifier)) {
            C_WARNING("Could not find surface" << identifier);
            break;
          }
          surface = surfaces[identifier];
        }
        surface->setVisible(true);
        surface->setZ(std::numeric_limits<int>::max());
        dbusInterface->sortZ();
#ifdef EPAPER
        guiThread->enqueue(
          surface,
          surface->rawRect(),
          Blight::WaveformMode::UI,
          Blight::ContentType::Color,
          Blight::UpdateMode::PartialUpdate,
          message->header.ackid,
          false,
          [message, this] { ack(message, 0, nullptr); }
        );
        do_ack = false;
#endif
        break;
      }
      case Blight::MessageType::Lower: {
        auto identifier = (Blight::surface_id_t)*message->data.get();
        C_DEBUG("Lower requested:" << identifier);
        std::shared_ptr<Surface> surface;
        {
          QReadLocker _locker(&surfacesLock);
          if (!surfaces.contains(identifier)) {
            C_WARNING("Could not find surface" << identifier);
            break;
          }
          surface = surfaces[identifier];
        }
        surface->setVisible(false);
        dbusInterface->sortZ();
#ifdef EPAPER
        guiThread->enqueue(
          nullptr,
          surface->geometry(),
          Blight::WaveformMode::UI,
          Blight::ContentType::Color,
          Blight::UpdateMode::PartialUpdate,
          message->header.ackid,
          true,
          [message, this] { ack(message, 0, nullptr); }
        );
        do_ack = false;
#endif
        break;
      }
      case Blight::MessageType::Wait: {
#ifdef EPAPER
        auto marker = (unsigned int)*message->data.get();
        C_DEBUG("Wait requested:" << marker);
        mxcfb_update_marker_data data{marker, 0};
        auto framebuffer = guiThread->framebuffer();
        if (framebuffer != nullptr) {
          ioctl(framebuffer->fd, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &data);
        }
#endif
        break;
      }
      case Blight::MessageType::Focus: {
        C_DEBUG("Focus requested");
        dbusInterface->setFocus(dbusInterface->getConnection(this));
        break;
      }
      case Blight::MessageType::Ping: {
#ifdef ACK_DEBUG
        O_DEBUG("Pong" << message->header.ackid);
#endif
        break;
      }
      case Blight::MessageType::Ack:
        do_ack = false;
        if (message->header.ackid == pingId) {
#ifdef ACK_DEBUG
          O_DEBUG("Pong recieved" << message->header.ackid);
#endif
          m_notRespondingTimer.stop();
          m_pingTimer.stop();
          m_pingTimer.start();
          m_notRespondingTimer.start();
          break;
        }
        C_WARNING("Unexpected ack from client" << message->header.ackid);
        break;
      default:
        C_WARNING("Unexpected message type" << message->header.type);
    }
    if (ack_size && ack_data == nullptr) {
      C_WARNING("Ack expected data, but none sent");
      ack_size = 0;
    }
    if (do_ack) {
      ack(message, ack_size, ack_data);
    }
    if (ack_free) {
      ack_free();
    } else if (ack_data != nullptr) {
      delete ack_data;
    }
  };
  m_notifier->setEnabled(true);
}

void
Connection::notResponding() {
  if (!isRunning()) {
    close();
    m_pingTimer.stop();
    return;
  }
  if (!isStopped()) {
    C_WARNING("Connection failed to respond to ping in time:" << id());
  } else if (this == dbusInterface->focused().get()) {
    dbusInterface->sortZ();
  }
  m_notRespondingTimer.start();
}

void
Connection::ack(
  Blight::message_ptr_t message,
  unsigned int size,
  Blight::data_t data
) {
  auto ack = Blight::message_t::create_ack(message.get(), size);
  if (!Blight::send_blocking(
        m_serverFd,
        reinterpret_cast<Blight::data_t>(&ack),
        sizeof(Blight::header_t)
      )) {
    C_WARNING("Failed to write ack header to socket:" << strerror(errno));
  } else if (size && !Blight::send_blocking(m_serverFd, data, size)) {
    C_WARNING("Failed to write ack data to socket:" << strerror(errno));
  } else {
    C_DEBUG("Acked:" << ack.ackid << size);
  }
}

void
Connection::ping() {
  if (!isRunning()) {
    return;
  }
  if (!isStopped()) {
    Blight::header_t ping{
      {.type = Blight::MessageType::Ping, .ackid = ++pingId, .size = 0}
    };
    if (!Blight::send_blocking(
          m_serverFd,
          reinterpret_cast<Blight::data_t>(&ping),
          sizeof(Blight::header_t)
        )) {
      C_WARNING("Failed to write to socket:" << strerror(errno));
    } else {
#ifdef ACK_DEBUG
      C_DEBUG("Ping" << ping.ackid);
#endif
    }
  }
  m_pingTimer.start();
}
#include "moc_connection.cpp"
