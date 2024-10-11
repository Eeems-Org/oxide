#include "dbusinterface.h"
#include "evdevhandler.h"
#include "guithread.h"

#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QCoreApplication>
#include <QQmlComponent>
#include <QFileInfo>
#include <sys/poll.h>
#include <unistd.h>
#include <QDBusMetaType>
#include <liboxide/debug.h>
#include <liboxide/devicesettings.h>
#include <libblight/types.h>
#include <libblight/socket.h>
#include <cstring>
#ifdef EPAPER
#include <epframebuffer.h>
#endif

DbusInterface::DbusInterface(QObject* parent)
: QObject(parent),
  m_focused(nullptr),
  m_exlusiveMode{false}
{
#ifdef EPAPER
    guiThread;
#else
    engine.load(QUrl(QStringLiteral("qrc:/Workspace.qml")));
    if(engine.rootObjects().isEmpty()){
        qFatal("Failed to load main layout");
    }
#endif
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
    qRegisterMetaType<std::vector<input_event>>("std::vector<input_event>");
    O_DEBUG("Connected service to bus");
}

DbusInterface* DbusInterface::singleton(){
    static DbusInterface* instance = nullptr;
    if(instance == nullptr){
        instance = Oxide::dispatchToMainThread<DbusInterface*>([]{
            O_DEBUG("Initializing DBus interface");
            return new DbusInterface(qApp);
        });
    }
    return instance;
}

int DbusInterface::pid(){ return qApp->applicationPid(); }

#ifndef EPAPER
QObject* DbusInterface::loadComponent(QString url, QString identifier, QVariantMap properties){
    auto object = engine.findChild<QObject*>(identifier);
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
#endif

void DbusInterface::processClosingConnections(){
    closingMutex.lock();
    if(!closingConnections.isEmpty()){
        O_DEBUG("Cleaning up old connections");
        while(!closingConnections.isEmpty()){
            closingConnections.takeFirst()->deleteLater();
        }
    }
    closingMutex.unlock();
}

void DbusInterface::processRemovedSurfaces(){
    for(auto connection : qAsConst(connections)){
        connection->processRemovedSurfaces();
    }
}

std::shared_ptr<Surface> DbusInterface::getSurface(QString identifier){
    for(auto connection : qAsConst(connections)){
        if(!connection->isRunning()){
            continue;
        }
        auto surface = connection->getSurface(identifier);
        if(surface != nullptr){
            return surface;
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
    return QDBusUnixFileDescriptor(connection->inputSocketDescriptor());
}

Blight::surface_id_t DbusInterface::addSurface(
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
        return 0;
    }
    auto connection = getConnection(message);
    if(connection == nullptr){
        sendErrorReply(QDBusError::AccessDenied, "You must first open a connection");
        return 0;
    }
    auto dfd = dup(fd.fileDescriptor());
    if(dfd == -1){
        sendErrorReply(QDBusError::InternalError, strerror(errno));
        return 0;
    }
    auto surface = connection->addSurface(
        dfd,
        QRect(x, y, width, height),
        stride,
        (QImage::Format)format
    );
    if(surface == nullptr){
        sendErrorReply(QDBusError::InternalError, "Unable to create surface");
        return 0;
    }
    if(!surface->isValid()){
        sendErrorReply(QDBusError::InternalError, "Unable to create surface");
        return 0;
    }
    return surface->identifier();
}

void DbusInterface::repaint(QString identifier, QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return;
    }
    auto surface = getSurface(identifier);
    if(surface == nullptr){
        sendErrorReply(QDBusError::BadAddress, "Surface not found");
        return;
    }
    surface->repaint();
}

QDBusUnixFileDescriptor DbusInterface::getSurface(Blight::surface_id_t identifier, QDBusMessage message){
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

void DbusInterface::setFlags(QString identifier, const QStringList& flags, QDBusMessage message){
    if(connections.count() > 1){
        // Only validate system flag if there is more than one connection
        // TODO - also validate that executable is allowed to make this call
        auto connection = getConnection(message);
        if(!connection->has("system")){
            sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
            return;
        }
    }
    auto connection = getConnection(identifier);
    if(connection != nullptr){
        for(auto& flag : flags){
            O_DEBUG("Flag" << flag << "set for connection" << identifier);
            connection->set(flag);
        }
        sortZ();
        return;
    }
    auto surface = getSurface(identifier);
    if(surface == nullptr){
        sendErrorReply(QDBusError::BadAddress, "Connection or Surface not found");
        return;
    }
    for(auto& flag : flags){
        surface->set(flag);
    }
    sortZ();
}

QStringList DbusInterface::getSurfaces(QDBusMessage message){
    auto connection = getConnection(message);
    QStringList surfaces;
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return surfaces;
    }
    for(auto connection : qAsConst(connections)){
        if(!connection->isRunning()){
            continue;
        }
        for(auto& surface : connection->getSurfaces()){
            if(surface != nullptr){
                surfaces.append(surface->id());
            }
        }
    }
    return surfaces;
}

QDBusUnixFileDescriptor DbusInterface::frameBuffer(QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return QDBusUnixFileDescriptor();
    }
    return QDBusUnixFileDescriptor(guiThread->framebuffer());
}

void DbusInterface::lower(QString identifier, QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return;
    }
    auto surface = getSurface(identifier);
    if(surface != nullptr){
        surface->setVisible(false);
        sortZ();
        surface->repaint();
        return;
    }
    auto childConnection = getConnection(identifier);
    if(childConnection == nullptr){
        sendErrorReply(QDBusError::BadAddress, "Connection or Surface not found");
        return;
    }
    if(m_focused == childConnection){
        setFocus(nullptr);
    }
    for(auto& surface : childConnection->getSurfaces()){
        surface->setVisible(false);
        surface->repaint();
    }
    sortZ();
}

void DbusInterface::raise(QString identifier, QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return;
    }
    auto surface = getSurface(identifier);
    if(surface != nullptr){
        surface->setVisible(true);
        surface->setZ(std::numeric_limits<int>::max());
        sortZ();
        surface->repaint();
        return;
    }
    auto childConnection = getConnection(identifier);
    if(childConnection == nullptr){
        sendErrorReply(QDBusError::BadAddress, "Connection or Surface not found");
        return;
    }
    for(auto& surface : childConnection->getSurfaces()){
        surface->setVisible(true);
        surface->setZ(std::numeric_limits<int>::max());
        surface->repaint();
    }
    sortZ();
}

void DbusInterface::focus(QString identifier, QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return;
    }
    auto childConnection = getConnection(identifier);
    if(childConnection == nullptr){
        sendErrorReply(QDBusError::BadAddress, "Connection not found");
        return;
    }
    setFocus(childConnection);
}

void DbusInterface::waitForNoRepaints(QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return;
    }
#ifdef EPAPER
    QEventLoop loop;
    connect(guiThread, &GUIThread::settled, &loop, &QEventLoop::quit);
    loop.exec();
#endif
}

void DbusInterface::enterExclusiveMode(QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return;
    }
    O_INFO("Entering exclusive mode");
    m_exlusiveMode = true;
    waitForNoRepaints(message);
    evdevHandler->clear_buffers();
}

void DbusInterface::exitExclusiveMode(QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return;
    }
    O_INFO("Exiting exclusive mode");
    m_exlusiveMode = false;
#ifdef EPAPER
    guiThread->enqueue(
        nullptr,
        EPFrameBuffer::instance()->framebuffer()->rect(),
        Blight::HighQualityGrayscale,
        0,
        true
    );
#endif
    waitForNoRepaints(message);
}

void DbusInterface::exclusiveModeRepaint(QDBusMessage message){
    auto connection = getConnection(message);
    if(!connection->has("system")){
        sendErrorReply(QDBusError::AccessDenied, "Must be system connection");
        return;
    }
#ifdef EPAPER
    guiThread->sendUpdate(
        EPFrameBuffer::instance()->framebuffer()->rect(),
        Blight::HighQualityGrayscale,
        0
    );
#endif
}

Connection* DbusInterface::focused(){ return m_focused; }

void DbusInterface::setFocus(Connection* connection){
    m_focused = connection;
    if(m_focused != nullptr){
        O_INFO(m_focused->id() << "has focus");
    }else{
        O_INFO("Nothing is in focus");
    }
}

void DbusInterface::serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner){
    Q_UNUSED(oldOwner);
    if(!newOwner.isEmpty()){
        return;
    }
    Q_UNUSED(name);
    // TODO - keep track of things this name owns and remove them
}

void DbusInterface::inputEvents(unsigned int device, const std::vector<input_event>& events){
    if(m_focused != nullptr){
        m_focused->inputEvents(device, events);
    }
    for(auto connection : qAsConst(connections)){
        if(connection->has("system")){
            connection->inputEvents(device, events);
        }
    }
}

bool DbusInterface::inExclusiveMode(){ return m_exlusiveMode; }

Connection* DbusInterface::getConnection(QDBusMessage message){
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

Connection* DbusInterface::getConnection(QString identifier){
    for(auto connection : qAsConst(connections)){
        if(connection->id() == identifier){
            return connection;
        }
    }
    return nullptr;
}

QObject* DbusInterface::workspace(){
    QListIterator<QObject*> i(engine.rootObjects());
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

Connection* DbusInterface::createConnection(int pid){
    pid_t pgid = ::getpgid(pid);
    auto connection = new Connection(pid, pgid);
    if(!connection->isValid()){
        connection->deleteLater();
        return nullptr;
    }
    connect(connection, &Connection::finished, this, [this, connection]{
        O_INFO("Connection" << connection->pid() << "closed");
        auto found = false;
        for(auto& ptr : qAsConst(connections)){
            if(ptr == connection){
                found = true;
                connections.removeAll(ptr);
                closingMutex.lock();
                closingConnections.append(ptr);
                closingMutex.unlock();
                guiThread->notify();
                break;
            }
        }
        if(!found){
            O_WARNING("Could not find connection to remove!");
        }
        if(m_focused != nullptr && m_focused == connection){
            setFocus(nullptr);
        }
        sortZ();
    });
    connect(connection, &Connection::focused, this, [this, connection]{
        for(auto& ptr : qAsConst(connections)){
            if(ptr == connection && !ptr->has("system")){
                setFocus(ptr);
                break;
            }
        }
    });
    connections.append(connection);
    return connection;
}

QList<std::shared_ptr<Surface>> DbusInterface::surfaces(){
    QList<std::shared_ptr<Surface>> surfaces;
    for(auto& connection : connections){
        for(auto& surface : connection->getSurfaces()){
            surfaces.append(surface);
        }
    }
    return surfaces;
}

QList<std::shared_ptr<Surface>> DbusInterface::sortedSurfaces(){
    auto sorted = surfaces().toVector();
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](std::shared_ptr<Surface> surface0, std::shared_ptr<Surface> surface1){
            if(surface0 == nullptr || !surface0->visible()){
                return false;
            }
            if(surface1 == nullptr || !surface1->visible()){
                return true;
            }
            if(surface0->has("system")){
                return false;
            }
            if(surface1->has("system")){
                return true;
            }
            return surface0->z() < surface1->z();
        }
    );
    return QList<std::shared_ptr<Surface>>::fromVector(sorted);
}

QList<std::shared_ptr<Surface>> DbusInterface::visibleSurfaces(){
    QList<std::shared_ptr<Surface>> surfaces;
    for(auto& surface : sortedSurfaces()){
        auto connection = surface->connection();
        if(
            surface->visible()
            && connection->isRunning()
        ){
            surfaces.append(surface);
        }
    }
    return surfaces;
}

const QByteArray& DbusInterface::clipboard(){ return clipboards.clipboard; }

void DbusInterface::setClipboard(const QByteArray& data){
    clipboards.clipboard = data;
    emit clipboardChanged(clipboard());
}

const QByteArray& DbusInterface::selection(){ return clipboards.selection; }

void DbusInterface::setSelection(const QByteArray& data){
    clipboards.selection = data;
    emit selectionChanged(selection());
}

const QByteArray& DbusInterface::secondary(){ return clipboards.secondary; }

void DbusInterface::setSecondary(const QByteArray& data){
    clipboards.secondary = data;
    emit clipboardChanged(secondary());
}

void DbusInterface::sortZ(){
    auto sorted = sortedSurfaces();
    int z = 0;
    for(auto surface : sorted){
        if(surface == nullptr){
            continue;
        }
        surface->setZ(z++);
    }
    if(
        m_focused != nullptr
        && (
            !m_focused->isRunning()
            || m_focused->isStopped()
        )
    ){
        setFocus(nullptr);
    }else if(m_focused != nullptr || sorted.empty()){
        return;
    }
    std::reverse(sorted.begin(), sorted.end());
    for(auto& surface : sorted){
        if(surface == nullptr){
            continue;
        }
        auto connection = surface->connection();
        if(
            !connection->isRunning()
            || connection->isStopped()
          || connection->has("system")
        ){
            continue;
        }
        for(auto& ptr : connections){
            if(ptr == connection){
                setFocus(ptr);
                break;
            }
        }
        break;
    }
}

#include "moc_dbusinterface.cpp"
