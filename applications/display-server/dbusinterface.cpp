#include "dbusinterface.h"

#include <libblight/socket.h>
#include <libblight/types.h>
#include <liboxide/dbus_types.h>
#include <liboxide/debug.h>
#include <liboxide/devicesettings.h>
#include <liboxide/oxideqml.h>
#include <liboxide/threading.h>
#include <memory>
#include <sys/poll.h>
#include <unistd.h>

#include <algorithm>

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusUnixFileDescriptor>
#include <QFileInfo>
#include <QQmlComponent>
#include <cstring>
#include <mutex>

#include "connection.h"
#include "evdevhandler.h"
#include "guithread.h"

DbusInterface::DbusInterface(QObject* parent)
  : QObject(parent)
  , m_focused(nullptr)
  , m_exclusiveMode{false} {
  auto type = qDBusRegisterMetaType<FrameBufferInfo>();
  if (!type.isValid()) {
    qFatal("Failed to register FrameBufferInfo with dbus");
  }
#ifdef EPAPER
  guiThread;
#endif
  // Needed to trick QtDBus into exposing the object
  setProperty("__init__", true);
#ifdef EPAPER
  auto bus = QDBusConnection::systemBus();
#else
  auto bus = QDBusConnection::sessionBus();
#endif
  if (!bus.isConnected()) {
    qFatal("Failed to connect to system bus.");
  }
  QDBusConnectionInterface* interface = bus.interface();
  if (!bus.registerObject(
        "/",
        this,
        QDBusConnection::ExportAllContents | QDBusConnection::ExportChildObjects
      )) {
    qFatal(
      "Unable to register interface: %s",
      bus.lastError().message().toStdString().c_str()
    );
  }
  if (!bus.objectRegisteredAt("/")) {
    qFatal("Object not appearing!");
  }
  auto reply = interface->registerService(BLIGHT_SERVICE);
  bus.registerService(BLIGHT_SERVICE);
  if (!reply.isValid()) {
    QDBusError ex = reply.error();
    qFatal(
      "Unable to register service: %s", ex.message().toStdString().c_str()
    );
  }
  connect(
    interface,
    &QDBusConnectionInterface::serviceOwnerChanged,
    this,
    &DbusInterface::serviceOwnerChanged
  );
  qRegisterMetaType<std::vector<input_event>>("std::vector<input_event>");
  O_DEBUG("Connected service to bus");
}

DbusInterface*
DbusInterface::singleton() {
  static DbusInterface* instance = nullptr;
  static std::once_flag initFlag;
  std::call_once(initFlag, []() {
    instance = Oxide::dispatchToMainThread<DbusInterface*>([] {
      O_DEBUG("Initializing DBus interface");
      return new DbusInterface(qApp);
    });
  });
  return instance;
}

void
DbusInterface::startup() {
#ifndef EPAPER
  if (
    Oxide::QML::loadQML(&engine, QUrl(QStringLiteral("qrc:/Workspace.qml"))) ==
    nullptr
  ) {
    qFatal("Failed to load main layout");
  }
#endif
}

int
DbusInterface::pid() {
  return qApp->applicationPid();
}

#ifndef EPAPER
QObject*
DbusInterface::loadComponent(
  QString url,
  QString identifier,
  QVariantMap properties
) {
  auto object = engine.findChild<QObject*>(identifier);
  if (object != nullptr) {
    for (auto i = properties.cbegin(), end = properties.cend(); i != end; ++i) {
      object->setProperty(i.key().toStdString().c_str(), i.value());
    }
    return object;
  }
  properties["objectName"] = properties["id"] = identifier;
  auto root = workspace();
  if (root == nullptr) {
    O_WARNING("Failed to get workspace");
    return nullptr;
  }
  QString objectName;
  if (!QMetaObject::invokeMethod(
        root,
        "loadComponent",
        Q_RETURN_ARG(QString, objectName),
        Q_ARG(QString, url),
        Q_ARG(QVariant, QVariant::fromValue(properties))
      )) {
    O_WARNING("Failed to queue loadComponent call");
    return nullptr;
  }
  if (objectName != identifier) {
    O_WARNING(objectName);
    return nullptr;
  }
  object = root->findChild<QObject*>(objectName);
  if (object == nullptr) {
    O_WARNING("Component" << objectName << "missing!");
  }
  O_DEBUG("Created component" << objectName);
  return object;
}
#endif

std::shared_ptr<Surface>
DbusInterface::getSurface(QString identifier) {
  QReadLocker _locker(&connectionsLock);
  for (auto connection : std::as_const(connections)) {
    if (!connection->isRunning()) {
      continue;
    }
    auto surface = connection->getSurface(identifier);
    if (surface != nullptr) {
      return surface;
    }
  }
  return nullptr;
}

QDBusUnixFileDescriptor
DbusInterface::open(QDBusMessage message) {
  pid_t pid = connection().interface()->servicePid(message.service());
  pid_t pgid = ::getpgid(pid);
  O_INFO("Open connection for: " << pid << pgid);
  auto connection = getConnection(message);
  if (connection != nullptr) {
    O_DEBUG("Found existing for: " << connection->pid());
    connection->enablePing();
    return QDBusUnixFileDescriptor(connection->socketDescriptor());
  }
  connection = createConnection(pid);
  if (connection == nullptr) {
    sendErrorReply(QDBusError::InternalError, "Unable to create connection");
    return QDBusUnixFileDescriptor();
  }
  O_DEBUG("success" << connection->socketDescriptor());
  return QDBusUnixFileDescriptor(connection->socketDescriptor());
}

QDBusUnixFileDescriptor
DbusInterface::openInput(unsigned short device, QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return QDBusUnixFileDescriptor();
  }
  O_INFO("Open input for: " << connection->pid() << " device: " << device);
  int fd = connection->inputFd(device);
  if (fd < 0) {
    sendErrorReply(QDBusError::InvalidArgs, "Device not available");
    return QDBusUnixFileDescriptor();
  }
  return QDBusUnixFileDescriptor(fd);
}

Blight::surface_id_t
DbusInterface::addSurface(
  QDBusUnixFileDescriptor fd,
  int x,
  int y,
  int width,
  int height,
  int stride,
  int format,
  double scale,
  QDBusMessage message
) {
  if (!fd.isValid()) {
    sendErrorReply(QDBusError::InvalidArgs, "Invalid file descriptor");
    return 0;
  }
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return 0;
  }
  auto dfd = dup(fd.fileDescriptor());
  if (dfd == -1) {
    sendErrorReply(
      QDBusError::InternalError, QString("Dup failed: %s").arg(strerror(errno))
    );
    return 0;
  }
  auto surface = connection->addSurface(
    dfd, QRect(x, y, width, height), stride, (QImage::Format)format, scale
  );
  if (surface == nullptr) {
    sendErrorReply(QDBusError::InternalError, "Unable to create surface");
    return 0;
  }
  if (!surface->isValid()) {
    sendErrorReply(QDBusError::InternalError, "Surface buffer is not valid");
    return 0;
  }
  if (connection->has("unified")) {
    auto pgid = connection->pgid();
    QReadLocker _locker(&connectionsLock);
    for (auto& other : std::as_const(connections)) {
      if (other.get() == connection.get()) {
        continue;
      }
      if (other->pgid() != pgid || !other->has("unified")) {
        continue;
      }
      other->addSurface(surface);
    }
  }
  return surface->identifier();
}

void
DbusInterface::repaint(QString identifier, QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
  if (!connection->has("system")) {
    sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
    return;
  }
  auto surface = getSurface(identifier);
  if (surface == nullptr) {
    sendErrorReply(QDBusError::BadAddress, "Surface not found");
    return;
  }
  surface->repaint();
}

QDBusUnixFileDescriptor
DbusInterface::getSurface(
  Blight::surface_id_t identifier,
  QDBusMessage message
) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return QDBusUnixFileDescriptor();
  }
  auto surface = connection->getSurface(identifier);
  if (surface == nullptr) {
    sendErrorReply(QDBusError::BadAddress, "Surface not found");
    return QDBusUnixFileDescriptor();
  }
  return QDBusUnixFileDescriptor(surface->fd());
}

void
DbusInterface::setFlags(
  QString identifier,
  const QStringList& flags,
  QDBusMessage message
) {
  int count;
  {
    QReadLocker _locker(&connectionsLock);
    count = connections.count();
  }
  if (count > 1) {
    // Only validate system flag if there is more than one connection
    // TODO - also validate that executable is allowed to make this call
    auto connection = getConnection(message);
    if (connection == nullptr) {
      sendErrorReply(
        QDBusError::AccessDenied, "You must first open a connection"
      );
      return;
    }
    if (
      !connection->has("system") &&
      (flags.contains("system") || connection->id() != identifier)
    ) {
      sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
      return;
    }
  }
  auto connection = getConnection(identifier);
  if (connection != nullptr) {
    for (auto& flag : flags) {
      O_INFO("Flag" << flag << "set for connection" << identifier);
      connection->set(flag);
    }
    sortZ();
    if (!connection->has("unified")) {
      return;
    }
    auto pgid = connection->pgid();
    {
      QReadLocker _locker(&connectionsLock);
      if (
        std::any_of(connections.begin(), connections.end(), [pgid](auto& conn) {
          return conn->pid() == pgid;
        })
      ) {
        return;
      }
    }
    auto parentConnection = createConnection(pgid);
    if (parentConnection != nullptr) {
      parentConnection->set("unified");
      parentConnection->disablePing();
    }
    return;
  }
  auto surface = getSurface(identifier);
  if (surface == nullptr) {
    sendErrorReply(QDBusError::BadAddress, "Connection or Surface not found");
    return;
  }
  for (auto& flag : flags) {
    O_INFO("Flag" << flag << "set for surface" << identifier);
    surface->set(flag);
  }
  sortZ();
}

QStringList
DbusInterface::getSurfaces(QDBusMessage message) {
  auto connection = getConnection(message);
  QStringList surfaces;
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return surfaces;
  }
  if (!connection->has("system")) {
    sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
    return surfaces;
  }
  QReadLocker _locker(&connectionsLock);
  for (auto connection : std::as_const(connections)) {
    if (!connection->isRunning()) {
      continue;
    }
    for (auto& surface : connection->getSurfaces()) {
      if (surface != nullptr) {
        surfaces.append(surface->id());
      }
    }
  }
  return surfaces;
}

QDBusUnixFileDescriptor
DbusInterface::frameBuffer(QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return QDBusUnixFileDescriptor();
  }
  if (!connection->has("system") && !connection->has("exclusive")) {
    sendErrorReply(
      QDBusError::AccessDenied, "Must be system or exclusive connection"
    );
    return QDBusUnixFileDescriptor();
  }
#ifdef EPAPER
  auto frameBuffer = guiThread->framebuffer();
  if (frameBuffer == nullptr) {
    sendErrorReply(QDBusError::InternalError, "Framebuffer is missing");
    return QDBusUnixFileDescriptor();
  }
  return QDBusUnixFileDescriptor(frameBuffer->fd);
#else
  sendErrorReply(
    QDBusError::InternalError, "Framebuffer not supported on this platform"
  );
  return QDBusUnixFileDescriptor();
#endif
}

FrameBufferInfo
DbusInterface::frameBufferInfo(QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return {-1, -1, -1, Blight::Format::Format_Invalid};
  }
  if (!connection->has("system") && !connection->has("exclusive")) {
    sendErrorReply(
      QDBusError::AccessDenied, "Must be system or exclusive connection"
    );
    return {-1, -1, -1, Blight::Format::Format_Invalid};
  }
#ifdef EPAPER
  auto frameBuffer = guiThread->framebuffer();
  if (frameBuffer == nullptr) {
    sendErrorReply(QDBusError::InternalError, "Framebuffer is missing");
    return {-1, -1, -1, Blight::Format::Format_Invalid};
  }
  return {
    frameBuffer->width,
    frameBuffer->height,
    frameBuffer->stride,
    frameBuffer->format
  };
#else
  return {1024, 1024, 1024 * 2, Blight::Format::Format_RGB16};
#endif
}

void
DbusInterface::lower(QString identifier, QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
  if (!connection->has("system")) {
    sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
    return;
  }
  auto surface = getSurface(identifier);
  if (surface != nullptr) {
    surface->setVisible(false);
    sortZ();
    surface->repaint(QRect(), true);
    return;
  }
  auto childConnection = getConnection(identifier);
  if (childConnection == nullptr) {
    sendErrorReply(QDBusError::BadAddress, "Connection or Surface not found");
    return;
  }
  if (m_focused == childConnection) {
    setFocus(nullptr);
  }
  for (auto& surface : childConnection->getSurfaces()) {
    surface->setVisible(false);
    surface->repaint();
  }
  sortZ();
}

void
DbusInterface::raise(QString identifier, QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
  if (!connection->has("system")) {
    sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
    return;
  }
  auto surface = getSurface(identifier);
  if (surface != nullptr) {
    surface->setVisible(true);
    surface->setZ(std::numeric_limits<int>::max());
    sortZ();
    surface->repaint(QRect(), true);
    return;
  }
  auto childConnection = getConnection(identifier);
  if (childConnection == nullptr) {
    sendErrorReply(QDBusError::BadAddress, "Connection or Surface not found");
    return;
  }
  for (auto& surface : childConnection->getSurfaces()) {
    surface->setVisible(true);
    surface->setZ(std::numeric_limits<int>::max());
    surface->repaint();
  }
  sortZ();
}

void
DbusInterface::focus(QString identifier, QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
  if (!connection->has("system")) {
    sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
    return;
  }
  auto childConnection = getConnection(identifier);
  if (childConnection == nullptr) {
    sendErrorReply(QDBusError::BadAddress, "Connection not found");
    return;
  }
  setFocus(childConnection);
}

void
DbusInterface::waitForNoRepaints(QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
#ifdef EPAPER
  if (m_exclusiveMode) {
    EPFramebuffer::instance()->sync();
  } else {
    QEventLoop loop;
    connect(guiThread, &GUIThread::settled, &loop, &QEventLoop::quit);
    loop.exec();
  }
#endif
}

void
DbusInterface::ghostControl(int mode, QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
#ifdef EPAPER
  EPFramebuffer::instance()->ghostControl(
    (EPFramebuffer::GhostControlMode)mode
  );
#endif
}

void
DbusInterface::enterExclusiveMode(QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
  if (!connection->has("system") && !connection->has("exclusive")) {
    sendErrorReply(
      QDBusError::AccessDenied, "Must be system or exclusive connection"
    );
    return;
  }
  if (!m_exclusiveMode) {
    O_INFO("Entering exclusive mode");
    waitForNoRepaints(message);
    m_exclusiveMode = true;
    evdevHandler->clear_buffers();
  }
}

void
DbusInterface::exitExclusiveMode(QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
  if (!connection->has("system") && !connection->has("exclusive")) {
    sendErrorReply(
      QDBusError::AccessDenied, "Must be system or exclusive connection"
    );
    return;
  }
  if (m_exclusiveMode) {
    O_INFO("Exiting exclusive mode");
    waitForNoRepaints(message);
    m_exclusiveMode = false;
#ifdef EPAPER
    guiThread->enqueue(
      nullptr,
      EPFramebuffer::instance()->frameBuffer.rect(),
      Blight::WaveformMode::UI,
      Blight::ContentType::Color,
      Blight::UpdateMode::FullUpdate,
      0,
      true
    );
#endif
    waitForNoRepaints(message);
  }
}
void
DbusInterface::exclusiveModeRepaint(
  int x,
  int y,
  int width,
  int height,
  int waveform,
  int updateMode,
  QDBusMessage message
) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
  if (!connection->has("system") && !connection->has("exclusive")) {
    sendErrorReply(
      QDBusError::AccessDenied, "Must be system or exclusive connection"
    );
    return;
  }
  if (!m_exclusiveMode) {
    return;
  }
#ifdef EPAPER
  guiThread->swap(
    QRect(x, y, width, height),
    (Blight::WaveformMode)waveform,
    Blight::ContentType::Color,
    (Blight::UpdateMode)updateMode
  );
#endif
}

void
DbusInterface::exclusiveModeRepaintFull(QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return;
  }
  if (!connection->has("system") && !connection->has("exclusive")) {
    sendErrorReply(
      QDBusError::AccessDenied, "Must be system or exclusive connection"
    );
    return;
  }
  if (!m_exclusiveMode) {
    return;
  }
#ifdef EPAPER
  guiThread->swap(
    EPFramebuffer::instance()->frameBuffer.rect(),
    Blight::WaveformMode::UI,
    Blight::ContentType::Color,
    Blight::UpdateMode::FullUpdate
  );
#endif
}

bool
DbusInterface::connectionExists(QString identifier, QDBusMessage message) {
  auto connection = getConnection(message);
  if (connection == nullptr) {
    sendErrorReply(
      QDBusError::AccessDenied, "You must first open a connection"
    );
    return false;
  }
  if (!connection->has("system")) {
    sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
    return false;
  }
  return getConnection(identifier) != nullptr;
}

std::shared_ptr<Connection>
DbusInterface::focused() {
  return m_focused;
}

void
DbusInterface::setFocus(std::shared_ptr<Connection> connection) {
  m_focused = connection;
  if (m_focused != nullptr) {
    O_INFO(m_focused->id() << "has focus");
  } else {
    O_INFO("Nothing is in focus");
  }
}

void
DbusInterface::serviceOwnerChanged(
  const QString& name,
  const QString& oldOwner,
  const QString& newOwner
) {
  Q_UNUSED(oldOwner);
  if (!newOwner.isEmpty()) {
    return;
  }
  Q_UNUSED(name);
  // TODO - keep track of things this name owns and remove them
}

void
DbusInterface::inputEvents(
  unsigned int device,
  const std::vector<input_event>& events
) {
  if (m_focused != nullptr) {
    m_focused->inputEvents(device, events);
  }
  QList<std::shared_ptr<Connection>> systemConnections;
  {
    QReadLocker _locker(&connectionsLock);
    for (auto connection : std::as_const(connections)) {
      if (connection->has("system")) {
        systemConnections.append(connection);
      }
    }
  }
  for (auto connection : std::as_const(systemConnections)) {
    connection->inputEvents(device, events);
  }
}

bool
DbusInterface::inExclusiveMode() {
  return m_exclusiveMode;
}

std::shared_ptr<Connection>
DbusInterface::getConnection(QDBusMessage message) {
  pid_t pid = connection().interface()->servicePid(message.service());
  QReadLocker _locker(&connectionsLock);
  for (auto connection : std::as_const(connections)) {
    if (!connection->isRunning()) {
      continue;
    }
    if (connection->pid() == pid) {
      return connection;
    }
  }
  return nullptr;
}

std::shared_ptr<Connection>
DbusInterface::getConnection(QString identifier) {
  QReadLocker _locker(&connectionsLock);
  for (auto connection : std::as_const(connections)) {
    if (connection->id() == identifier) {
      return connection;
    }
  }
  return nullptr;
}

const QList<std::shared_ptr<Connection>>
DbusInterface::getConnections() {
  QReadLocker _locker(&connectionsLock);
  return std::as_const(connections);
}

std::shared_ptr<Connection>
DbusInterface::getConnection(Connection* ptr) {
  QReadLocker _locker(&connectionsLock);
  for (auto connection : std::as_const(connections)) {
    if (connection.get() == ptr) {
      return connection;
    }
  }
  return nullptr;
}

QObject*
DbusInterface::workspace() {
  QListIterator<QObject*> i(engine.rootObjects());
  while (i.hasNext()) {
    auto item = i.next();
    if (item->objectName() == "Workspace") {
      O_DEBUG("Found workspace");
      return item;
    }
  }
  O_WARNING("Failed to get workspace");
  return nullptr;
}

std::shared_ptr<Connection>
DbusInterface::createConnection(int pid) {
  pid_t pgid = ::getpgid(pid);
  std::shared_ptr<Connection> connection;
  try {
    connection =
      std::shared_ptr<Connection>(new Connection(pid, pgid), [](Connection* c) {
        c->deleteLater();
      });
  } catch (const std::bad_alloc&) {
    O_WARNING("Failed to create new connection, out of memory");
    return nullptr;
  }
  if (!connection->isValid()) {
    connection->deleteLater();
    return nullptr;
  }
  std::weak_ptr<Connection> weakConnection = connection;
  connect(
    connection.get(), &Connection::finished, this, [this, weakConnection] {
      auto connection = weakConnection.lock();
      if (connection == nullptr) {
        return;
      }
      O_INFO("Connection" << connection->pid() << "closed");
      auto found = false;
      {
        QWriteLocker _locker(&connectionsLock);
        found = connections.contains(connection);
        if (found) {
          connections.removeAll(connection);
#ifdef EPAPER
          guiThread->notify();
#endif
        }
      }
      {
        QReadLocker _locker(&connectionsLock);
        if (connections.isEmpty() && m_exclusiveMode) {
          O_INFO("Exiting exclusive mode");
          m_exclusiveMode = false;
#ifdef EPAPER
          guiThread->enqueue(
            nullptr,
            EPFramebuffer::instance()->frameBuffer.rect(),
            Blight::WaveformMode::UI,
            Blight::ContentType::Color,
            Blight::UpdateMode::FullUpdate,
            0,
            true
          );
#endif
        }
      }
      if (!found) {
        O_WARNING("Could not find connection to remove!");
      }
      if (m_focused != nullptr && m_focused == connection) {
        setFocus(nullptr);
      }
      sortZ();
    }
  );
  connect(connection.get(), &Connection::focused, this, [this, weakConnection] {
    auto connection = weakConnection.lock();
    if (connection == nullptr) {
      return;
    }
    QReadLocker _locker(&connectionsLock);
    for (auto& ptr : std::as_const(connections)) {
      if (ptr == connection && !ptr->has("system")) {
        setFocus(ptr);
        break;
      }
    }
  });
  {
    QReadLocker _locker(&connectionsLock);
    for (auto& _connection : std::as_const(connections)) {
      if (_connection->pgid() != pgid || !_connection->has("unified")) {
        continue;
      }
      connection->set("unified");
      for (auto& surface : _connection->getSurfaces()) {
        if (connection->getSurface(surface->identifier()) == nullptr) {
          connection->addSurface(surface);
        }
      }
    }
  }
  QWriteLocker _locker(&connectionsLock);
  connections.append(connection);
  return connection;
}

bool
DbusInterface::hasRunningConnection(pid_t pgid) {
  QReadLocker _locker(&connectionsLock);
  for (auto& connection : std::as_const(connections)) {
    if (connection->pgid() == pgid && connection->isRunning()) {
      return true;
    }
  }
  return false;
}

void
DbusInterface::removeSurface(QString identifier) {
  QReadLocker _locker(&connectionsLock);
  for (auto& connection : std::as_const(connections)) {
    auto surface = connection->getSurface(identifier);
    if (surface != nullptr) {
      connection->removeSurface(surface->identifier());
    }
  }
}

QList<std::shared_ptr<Surface>>
DbusInterface::surfaces() {
  QList<std::shared_ptr<Surface>> surfaces;
  QReadLocker _locker(&connectionsLock);
  for (auto& connection : std::as_const(connections)) {
    for (auto& surface : connection->getSurfaces()) {
      if (!surfaces.contains(surface)) {
        surfaces.append(surface);
      }
    }
  }
  return surfaces;
}

QList<std::shared_ptr<Surface>>
DbusInterface::sortedSurfaces() {
  auto sorted = surfaces().toVector();
  std::sort(
    sorted.begin(),
    sorted.end(),
    [](std::shared_ptr<Surface> surface0, std::shared_ptr<Surface> surface1) {
      if (surface0 == nullptr || !surface0->visible()) {
        return false;
      }
      if (surface1 == nullptr || !surface1->visible()) {
        return true;
      }
      bool system0 = surface0->has("system");
      bool system1 = surface1->has("system");
      if (system0 && !system1) {
        return false;
      }
      if (!system0 && system1) {
        return true;
      }
      return surface0->z() < surface1->z();
    }
  );
  return QList<std::shared_ptr<Surface>>::fromVector(sorted);
}

QList<std::shared_ptr<Surface>>
DbusInterface::visibleSurfaces() {
  QList<std::shared_ptr<Surface>> surfaces;
  for (auto& surface : sortedSurfaces()) {
    auto connection = surface->connection();
    if (connection == nullptr || !surface->visible()) {
      continue;
    }
    if (
      connection->isRunning() ||
      (connection->has("unified") && hasRunningConnection(connection->pgid()))
    ) {
      surfaces.append(surface);
    }
  }
  return surfaces;
}

const QByteArray&
DbusInterface::clipboard() {
  return clipboards.clipboard;
}

void
DbusInterface::setClipboard(const QByteArray& data) {
  clipboards.clipboard = data;
  emit clipboardChanged(clipboard());
}

const QByteArray&
DbusInterface::selection() {
  return clipboards.selection;
}

void
DbusInterface::setSelection(const QByteArray& data) {
  clipboards.selection = data;
  emit selectionChanged(selection());
}

const QByteArray&
DbusInterface::secondary() {
  return clipboards.secondary;
}

void
DbusInterface::setSecondary(const QByteArray& data) {
  clipboards.secondary = data;
  emit secondaryChanged(secondary());
}

void
DbusInterface::sortZ() {
  auto sorted = sortedSurfaces();
  int z = 0;
  for (auto surface : sorted) {
    if (surface == nullptr) {
      continue;
    }
    surface->setZ(z++);
  }
  if (
    m_focused != nullptr && (!m_focused->isRunning() || m_focused->isStopped())
  ) {
    setFocus(nullptr);
  } else if (m_focused != nullptr || sorted.empty()) {
    return;
  }
  std::reverse(sorted.begin(), sorted.end());
  for (auto& surface : sorted) {
    if (surface == nullptr) {
      continue;
    }
    auto connection = surface->connection();
    if (
      connection == nullptr || !connection->isRunning() ||
      connection->isStopped() || connection->has("system")
    ) {
      continue;
    }
    {
      QReadLocker _locker(&connectionsLock);
      if (!connections.contains(connection)) {
        continue;
      }
    }
    setFocus(connection);
    break;
  }
}

#include "moc_dbusinterface.cpp"
