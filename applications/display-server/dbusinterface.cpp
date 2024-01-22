#include "dbusinterface.h"
#include "evdevhandler.h"

#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QCoreApplication>
#include <QQmlComponent>
#include <sys/poll.h>
#include <unistd.h>
#include <QDBusMetaType>
#include <liboxide/debug.h>
#include <libblight/types.h>
#include <cstring>

DbusInterface* DbusInterface::singleton = nullptr;

DbusInterface::DbusInterface(QObject* parent, QQmlApplicationEngine* engine)
: QObject(parent),
  engine(engine),
  m_focused(nullptr)
{
    if(singleton != nullptr){
        qFatal("DbusInterface singleton already exists");
    }
    singleton = this;
    // Needed to trick QtDBus into exposing the object
    setProperty("__init__", true);
#ifdef EPAPER
    auto bus = QDBusConnection::systemBus();
#else
    auto bus = QDBusConnection::sessionBus();
#endif
    if(!bus.isConnected()){
        qFatal("Failed to connect to system bus.");
    }
    QDBusConnectionInterface* interface = bus.interface();
    if(!bus.registerObject(
        "/",
        this,
        QDBusConnection::ExportAllContents | QDBusConnection::ExportChildObjects
    )){
        qFatal("Unable to register interface: %s", bus.lastError().message().toStdString().c_str());
    }
    if(!bus.objectRegisteredAt("/")){
        qFatal("Object not appearing!");
    }
    auto reply = interface->registerService(BLIGHT_SERVICE);
    bus.registerService(BLIGHT_SERVICE);
    if(!reply.isValid()){
        QDBusError ex = reply.error();
        qFatal("Unable to register service: %s", ex.message().toStdString().c_str());
    }
    connect(
        interface,
        &QDBusConnectionInterface::serviceOwnerChanged,
        this,
        &DbusInterface::serviceOwnerChanged
        );
    O_DEBUG("Connected service to bus");
    connect(&connectionTimer, &QTimer::timeout, this, [this]{
        QMutableListIterator<std::shared_ptr<Connection>> i(connections);
        while(i.hasNext()){
            auto connection = i.next();
            if(!connection->isRunning()){
                i.remove();
                QMetaObject::invokeMethod(connection.get(), "finished", Qt::QueuedConnection);
            }
        }
    });
    connectionTimer.setInterval(100);
    connectionTimer.start();
    connect(
        evdevHandler,
        &EvDevHandler::inputEvents,
        this,
        &::DbusInterface::inputEvents,
        Qt::BlockingQueuedConnection
    );
}

int DbusInterface::pid(){ return qApp->applicationPid(); }

QObject* DbusInterface::loadComponent(QString url, QString identifier, QVariantMap properties){
    auto object = engine->findChild<QObject*>(identifier);
    if(object != nullptr){
        for(auto i = properties.cbegin(), end = properties.cend(); i != end; ++i){
            object->setProperty(i.key().toStdString().c_str(), i.value());
        }
        return object;
    }
    properties["objectName"] = properties["id"] = identifier;
    auto root = workspace();
    if(root == nullptr){
        O_WARNING("Failed to get workspace");
        return nullptr;
    }
    QString objectName;
    if(!QMetaObject::invokeMethod(
        root,
        "loadComponent",
        Q_RETURN_ARG(QString, objectName),
        Q_ARG(QString, url),
        Q_ARG(QVariant, QVariant::fromValue(properties))
    )){
        O_WARNING("Failed to queue loadComponent call");
        return nullptr;
    }
    if(objectName != identifier){
        O_WARNING(objectName);
        return nullptr;
    }
    object = root->findChild<QObject*>(objectName);
    if(object == nullptr){
        O_WARNING("Component" << objectName << "missing!");
    }
    O_DEBUG("Created component" << objectName);
    return object;
}

std::shared_ptr<Surface> DbusInterface::getSurface(QString identifier){
    for(auto connection : qAsConst(connections)){
        if(!connection->isRunning()){
            continue;
        }
        auto surface = connection->getSurface(identifier);
        if(surface != nullptr){
            return std::shared_ptr(surface);
        }
    }
    return nullptr;
}

QDBusUnixFileDescriptor DbusInterface::open(QDBusMessage message){
    pid_t pid = connection().interface()->servicePid(message.service());
    pid_t pgid = ::getpgid(pid);
    O_INFO("Open connection for: " << pid << pgid);
    auto connection = getConnection(message);
    if(connection != nullptr){
        O_DEBUG("Found existing for: " << connection->pid());
        return QDBusUnixFileDescriptor(connection->socketDescriptor());
    }
    connection = createConnection(pid);
    if(connection == nullptr){
        sendErrorReply(QDBusError::InternalError, "Unable to create connection");
        return QDBusUnixFileDescriptor();
    }
    O_DEBUG("success" << connection->socketDescriptor());
    return QDBusUnixFileDescriptor(connection->socketDescriptor());
}

QDBusUnixFileDescriptor DbusInterface::openInput(QDBusMessage message){
    auto connection = getConnection(message);
    if(connection == nullptr){
        sendErrorReply(QDBusError::AccessDenied, "You must first open a connection");
        return QDBusUnixFileDescriptor();
    }
    O_INFO("Open input for: " << connection->pid());
    return QDBusUnixFileDescriptor(connection->inputReadSocketDescriptor());
}

QString DbusInterface::addSurface(
    QDBusUnixFileDescriptor fd,
    int x,
    int y,
    int width,
    int height,
    int stride,
    int format,
    QDBusMessage message
){
    if(!fd.isValid()){
        sendErrorReply(QDBusError::InvalidArgs, "Invalid file descriptor");
        return "";
    }
    auto connection = getConnection(message);
    if(connection == nullptr){
        sendErrorReply(QDBusError::AccessDenied, "You must first open a connection");
        return "";
    }
    auto dfd = dup(fd.fileDescriptor());
    if(dfd == -1){
        sendErrorReply(QDBusError::InternalError, strerror(errno));
        return "";
    }
    auto surface = connection->addSurface(
        dfd,
        QRect(x, y, width, height),
        stride,
        (QImage::Format)format
    );
    if(surface == nullptr){
        sendErrorReply(QDBusError::InternalError, "Unable to create surface");
        return "";
    }
    if(!surface->isValid()){
        surface->deleteLater();
        sendErrorReply(QDBusError::InternalError, "Unable to create surface");
        return "";
    }
    return surface->id();
}

void DbusInterface::repaint(QString identifier, QDBusMessage message){
    auto connection = getConnection(message);
    if(connection == nullptr){
        sendErrorReply(QDBusError::AccessDenied, "You must first open a connection");
        return;
    }
    auto surface = connection->getSurface(identifier);
    if(surface == nullptr){
        sendErrorReply(QDBusError::BadAddress, "Surface not found");
        return;
    }
    surface->repaint();
}

QDBusUnixFileDescriptor DbusInterface::getSurface(QString identifier, QDBusMessage message){
    auto connection = getConnection(message);
    if(connection == nullptr){
        sendErrorReply(QDBusError::AccessDenied, "You must first open a connection");
        return QDBusUnixFileDescriptor();
    }
    auto surface = connection->getSurface(identifier);
    if(surface == nullptr){
        sendErrorReply(QDBusError::BadAddress, "Surface not found");
        return QDBusUnixFileDescriptor();
    }
    return QDBusUnixFileDescriptor(surface->fd());
}

std::shared_ptr<Connection> DbusInterface::focused(){ return m_focused; }

void DbusInterface::serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
    Q_UNUSED(oldOwner);
    if(!newOwner.isEmpty()){
        return;
    }
    Q_UNUSED(name);
    // TODO - keep track of things this name owns and remove them
}

void DbusInterface::inputEvents(unsigned int device, const std::vector<input_event>& events){
    if(m_focused == nullptr){
        return;
    }
    auto fd = m_focused->inputWriteSocketDescriptor();
    for(const input_event& ie : events){
        O_DEBUG("writeEvent(" << device << ie.type << ie.code << ie.value << ")");
        int res = -1;
        int count = 0;
        while(res < 1 && count < 5){
            pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLOUT;
            res = poll(&pfd, 1, -1);
            if(res < 0){
                if(errno == EAGAIN || errno == EINTR){
                    count++;
                    continue;
                }
                break;
            }
            if(res == 0){
                continue;
            }
            if(pfd.revents & POLLHUP){
                errno = ECONNRESET;
                res = -1;
                break;
            }
            if(!(pfd.revents & POLLOUT)){
                O_WARNING("Unexpected revents:" << pfd.revents);
                res = 0;
                continue;
            }
            Blight::event_packet_t data = { device, ie };
            res = ::send(fd, &data, sizeof(data), MSG_EOR);
            if(res >= 0){
                break;
            }
            if(errno == EAGAIN || errno == EINTR){
                count++;
                continue;
            }
            break;
        }
        if(res < 0){
            O_WARNING("Failed to write input event: " << std::strerror(errno));
            break;
        }
        if(res != sizeof(Blight::event_packet_t)){
            O_WARNING("Failed to write input event: Size mismatch! " << res);
            break;
        }
        O_DEBUG("Write finished");
    }
}

std::shared_ptr<Connection> DbusInterface::getConnection(QDBusMessage message){
    pid_t pid = connection().interface()->servicePid(message.service());;
    for(auto connection : qAsConst(connections)){
        if(!connection->isRunning()){
            continue;
        }
        if(connection->pid() == pid){
            return connection;
        }
    }
    return nullptr;
}

QObject* DbusInterface::workspace(){
    QListIterator<QObject*> i(engine->rootObjects());
    while(i.hasNext()){
        auto item = i.next();
        if(item->objectName() == "Workspace"){
            O_DEBUG("Found workspace");
            return item;
        }
    }
    O_WARNING("Failed to get workspace");
    return nullptr;
}

std::shared_ptr<Connection> DbusInterface::createConnection(int pid){
    pid_t pgid = ::getpgid(pid);
    auto connection = std::shared_ptr<Connection>(new Connection(this, pid, pgid));
    if(!connection->isValid()){
        connection->deleteLater();
        return nullptr;
    }
    connect(connection.get(), &Connection::finished, this, [this, connection]{
        O_DEBUG("Connection" << connection->pid() << "closed");
        QMutableListIterator<std::shared_ptr<Connection>> i(connections);
        while(i.hasNext()){
            auto item = i.next();
            if(item == connection){
                i.remove();
            }
        }
        if(m_focused == connection){
            m_focused = nullptr;
        }
        sortZ();
    });
    connect(connection.get(), &Connection::focused, this, [this, connection]{
        m_focused = connection;
        O_DEBUG(connection->id() << "has focus");
    });
    connections.append(connection);
    return connection;
}

QList<QString> DbusInterface::surfaces(){
    QList<QString> surfaces;
    for(auto& connection : connections){
        for(auto& surface : connection->getSurfaces()){
            surfaces.append(surface);
        }
    }
    return surfaces;
}

void DbusInterface::sortZ(){
    auto sorted = surfaces().toVector();
    std::sort(
        sorted.begin(),
        sorted.end(),
        [this](const QString& identifier0, const QString& identifier1){
            auto surface0 = getSurface(identifier0);
            if(surface0 == nullptr){
                return false;
            }
            auto surface1 = getSurface(identifier1);
            if(surface1 == nullptr){
                return true;
            }
            return surface0->z() < surface1->z();
        }
    );
    int z = 0;
    for(auto& identifier : sorted){
        auto surface = getSurface(identifier);
        if(surface == nullptr){
            continue;
        }
        surface->setZ(z++);
    }
    if(m_focused != nullptr){
        return;
    }
    auto surface = getSurface(sorted.last());
    if(surface == nullptr){
        return;
    }
    auto connection = dynamic_cast<Connection*>(surface->parent());
    for(auto& ptr : connections){
        if(ptr.get() == connection){
            m_focused = ptr;
            break;
        }
    }
}

#include "moc_dbusinterface.cpp"
