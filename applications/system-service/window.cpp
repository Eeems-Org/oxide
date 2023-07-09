#include "window.h"
#include "guiapi.h"

#include <sys/socket.h>
#include <sys/file.h>


#define W_WARNING(msg) O_WARNING(identifier() << __PRETTY_FUNCTION__ << msg << guiAPI->getSenderPgid())
#define W_DEBUG(msg) O_DEBUG(identifier() << __PRETTY_FUNCTION__ << msg << guiAPI->getSenderPgid())
#define W_DENIED() W_DEBUG("DENY")
#define W_ALLOWED() W_DEBUG("ALLOW")
#define LOCK_MUTEX \
    QMutexLocker locker(&m_mutex); \
    Q_UNUSED(locker);

EventPipe::EventPipe()
: QObject{nullptr},
  m_readSocket{this},
  m_writeSocket{this}
{
    int fds[2];
    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1){
        O_WARNING(__PRETTY_FUNCTION__ << "Unable to open event pipe:" << strerror(errno));
    }
    if(!m_readSocket.setSocketDescriptor(fds[1], QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
        O_WARNING(__PRETTY_FUNCTION__ << "Unable to open event pipe read socket:" << m_readSocket.errorString());
    }
    if(!m_writeSocket.setSocketDescriptor(fds[0], QLocalSocket::ConnectedState, QLocalSocket::WriteOnly | QLocalSocket::Unbuffered)){
        O_WARNING(__PRETTY_FUNCTION__ << "Unable to open event pipe write socket:" << m_writeSocket.errorString());
    }
}

EventPipe::~EventPipe(){
    close();
}

bool EventPipe::isValid(){ return m_readSocket.isValid() && m_writeSocket.isValid(); }

bool EventPipe::isOpen(){ return m_readSocket.isOpen() && m_writeSocket.isOpen(); }

void EventPipe::close(){
    m_readSocket.close();
    m_writeSocket.close();
}

QLocalSocket* EventPipe::readSocket(){ return &m_readSocket; }

QLocalSocket* EventPipe::writeSocket(){ return &m_writeSocket; }

qint64 EventPipe::write(const char* data, qint64 size){ return m_writeSocket.write(data, size); }

bool EventPipe::flush(){ return m_writeSocket.flush(); }

Window::Window(const QString& id, const QString& path, const pid_t& pgid, const QRect& geometry)
: QObject{guiAPI},
  m_identifier{id},
  m_enabled{false},
  m_path{path},
  m_pgid{pgid},
  m_geometry{geometry},
  m_z{-1},
  m_file{this},
  m_state{WindowState::LoweredHidden},
  m_format{DEFAULT_IMAGE_FORMAT}
{
    LOCK_MUTEX;
    createFrameBuffer(geometry);
    O_DEBUG(id << __PRETTY_FUNCTION__ << "Window created" << id << pgid);
}
Window::~Window(){
    LOCK_MUTEX;
    m_file.close();
    W_DEBUG("Window closed");
}

void Window::setEnabled(bool enabled){
    m_enabled = enabled;
    auto bus = QDBusConnection::systemBus();
    bool registered = bus.objectRegisteredAt(m_path) != nullptr;
    if(enabled && registered){
        return;
    }
    if(!enabled && !registered){
        return;
    }
    if (!enabled){
        bus.unregisterObject(m_path, QDBusConnection::UnregisterTree);
        W_DEBUG("Unregistered");
        return;
    }
    if(bus.registerObject(m_path, this, QDBusConnection::ExportAllContents)){
        W_DEBUG("Registered" << m_path << OXIDE_WINDOW_INTERFACE);
    }else{
        W_WARNING("Failed to register" << m_path << OXIDE_WINDOW_INTERFACE);
    }
}

QDBusObjectPath Window::path(){ return QDBusObjectPath(m_path); }

const QString& Window::identifier(){ return m_identifier; }

int Window::z(){ return m_z; }

void Window::setZ(int z){
    if(m_z == z){
        return;
    }
    m_z = z;
    emit zChanged(z);
}

QDBusUnixFileDescriptor Window::frameBuffer(){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED();
    LOCK_MUTEX;
    return QDBusUnixFileDescriptor(m_file.handle());
}

QDBusUnixFileDescriptor Window::touchEventPipe(){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED();
    return QDBusUnixFileDescriptor(m_touchEventPipe.readSocket()->socketDescriptor());
}

QDBusUnixFileDescriptor Window::tabletEventPipe(){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED();
    return QDBusUnixFileDescriptor(m_tabletEventPipe.readSocket()->socketDescriptor());
}

QDBusUnixFileDescriptor Window::keyEventPipe(){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED();
    return QDBusUnixFileDescriptor(m_keyEventPipe.readSocket()->socketDescriptor());
}

QRect Window::geometry(){
    if(!hasPermissions()){
        W_DENIED();
        return QRect();
    }
    W_ALLOWED();
    return m_geometry;
}

QRect Window::_geometry(){ return m_geometry; }

void Window::setGeometry(const QRect& geometry){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    LOCK_MUTEX;
    createFrameBuffer(geometry);
}

bool Window::isVisible(){
    if(!hasPermissions()){
        W_DENIED();
        return false;
    }
    W_ALLOWED();
    return _isVisible();
}

bool Window::_isVisible(){
    LOCK_MUTEX;
    return m_file.isOpen() && m_state == WindowState::Raised;
}

void Window::setVisible(bool visible){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    auto wasVisible = _isVisible();
    auto state = m_state;
    switch(m_state){
        case WindowState::Raised:
        case WindowState::RaisedHidden:
            m_state = visible ? WindowState::Raised : WindowState::RaisedHidden;
        break;
        case WindowState::Lowered:
        case WindowState::LoweredHidden:
            m_state = visible ? WindowState::Lowered : WindowState::LoweredHidden;
        break;
    }
    if(state == m_state){
        return;
    }
    emit stateChanged(m_state);
    if(wasVisible != _isVisible()){
        emit dirty(m_geometry);
    }
}

QImage Window::toImage(){
    LOCK_MUTEX;
    if(m_file.handle() == -1){
        return QImage();
    }
    return QImage(m_data, m_geometry.width(), m_geometry.height(), m_bytesPerLine, m_format);
}

qulonglong Window::sizeInBytes(){
    if(!hasPermissions()){
        W_DENIED();
        return 0;
    }
    W_ALLOWED();
    LOCK_MUTEX;
    return m_file.isOpen() ? m_file.size() : 0;
}

qulonglong Window::bytesPerLine(){
    if(!hasPermissions()){
        W_DENIED();
        return 0;
    }
    W_ALLOWED();
    LOCK_MUTEX;
    return m_file.isOpen() ? m_bytesPerLine : 0;
}

int Window::format(){
    if(!hasPermissions()){
        W_DENIED();
        return QImage::Format_Invalid;
    }
    W_ALLOWED();
    LOCK_MUTEX;
    return m_file.isOpen() ? m_format : QImage::Format_Invalid;
}

bool Window::writeTouchEvent(const input_event& event){ return writeEvent(&m_touchEventPipe, event); }

bool Window::writeTabletEvent(const input_event& event){ return writeEvent(&m_tabletEventPipe, event); }

bool Window::writeKeyEvent(const input_event& event){ return writeEvent(&m_keyEventPipe, event); }

pid_t Window::pgid(){ return m_pgid; }

void Window::_close(){
    m_state = WindowState::LoweredHidden;
    emit closed();
}

Window::WindowState Window::state(){ return m_state; }

void Window::lock(){
    // TODO - explore if mprotect/msync work. This might not be doing anything.
    // Don't call LOCK_MUTEX in here due to use in createFrameBuffer
    if(!m_file.isOpen()){
        return;
    }
    while(flock(m_file.handle(), LOCK_EX | LOCK_NB) == -1){
        if(errno != EWOULDBLOCK && errno != EINTR){
            W_DEBUG("Failed to lock framebuffer:" << strerror(errno));
            continue;
        }
        qApp->processEvents(QEventLoop::AllEvents, 100);
    }
}

void Window::unlock(){
    // TODO - explore if mprotect/msync work. This might not be doing anything.
    // Don't call LOCK_MUTEX in here due to use in createFrameBuffer
    if(!m_file.isOpen()){
        return;
    }
    while(flock(m_file.handle(), LOCK_UN) == -1){
        if(errno != EWOULDBLOCK && errno != EINTR){
            W_DEBUG("Failed to unlock framebuffer:" << strerror(errno));
            continue;
        }
        qApp->processEvents(QEventLoop::AllEvents, 100);
    }
}

bool Window::operator>(Window* other) const{ return m_z > other->z(); }

bool Window::operator<(Window* other) const{ return m_z < other->z(); }

QDBusUnixFileDescriptor Window::resize(int width, int height){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED();
    LOCK_MUTEX;
    createFrameBuffer(QRect(m_geometry.x(), m_geometry.y(), width, height));
    return QDBusUnixFileDescriptor(m_file.handle());
}

void Window::move(int x, int y){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    auto wasVisible = _isVisible();
    auto oldGeometry = m_geometry;
    m_geometry.setX(x);
    m_geometry.setY(y);
    emit geometryChanged(oldGeometry, m_geometry);
    if(wasVisible){
        emit dirty(oldGeometry);
    }
    if(_isVisible()){
        emit dirty(m_geometry);
    }
}

void Window::repaint(QRect region){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    if(_isVisible()){
        emit dirty(region);
    }
}

void Window::repaint(){ repaint(m_geometry); }

void Window::raise(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    switch(m_state){
        case WindowState::Lowered:
            m_state = WindowState::Raised;
        break;
        case WindowState::LoweredHidden:
            m_state = WindowState::RaisedHidden;
        break;
        case WindowState::Raised:
        case WindowState::RaisedHidden:
        default:
        break;
    }
    if(_isVisible()){
        emit dirty(m_geometry);
    }
    emit stateChanged(m_state);
    emit raised();
}

void Window::lower(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    bool wasVisible = _isVisible();
    switch(m_state){
        case WindowState::Raised:
            m_state = WindowState::Lowered;
        break;
        case WindowState::RaisedHidden:
            m_state = WindowState::LoweredHidden;
        break;
        case WindowState::Lowered:
        case WindowState::LoweredHidden:
        default:
        break;
    }
    if(wasVisible){
        emit dirty(m_geometry);
    }
    emit stateChanged(m_state);
    emit lowered();
}

void Window::close(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    _close();
}

bool Window::hasPermissions(){ return guiAPI->isThisPgId(m_pgid); }

void Window::createFrameBuffer(const QRect& geometry){
    // No mutex, as it should be handled by the calling function
    if(m_file.isOpen() && geometry == m_geometry){
        W_WARNING("No need to resize:" << geometry);
        return;
    }
    unlock();
    m_file.close();
    if(geometry.isEmpty() || geometry.isNull() || !geometry.isValid()){
        W_WARNING("Invalid geometry for framebuffer:" << geometry);
        return;
    }
    int fd = memfd_create(m_identifier.toStdString().c_str(), MFD_ALLOW_SEALING);
    if(fd == -1){
        W_WARNING("Unable to create memfd for framebuffer:" << strerror(errno));
        return;
    }
    QImage blankImage(geometry.width(), geometry.height(), m_format);
    blankImage.fill(Qt::white);
    size_t size = blankImage.sizeInBytes();
    if(ftruncate(fd, size)){
        W_WARNING("Unable to truncate memfd for framebuffer:" << strerror(errno));
        if(::close(fd) == -1){
            W_WARNING("Failed to close fd:" << strerror(errno));
        }
        return;
    }
    int flags = fcntl(fd, F_GET_SEALS);
    if(fcntl(fd, F_ADD_SEALS, flags | F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW)){
        W_WARNING("Unable to seal memfd for framebuffer:" << strerror(errno));
        if(::close(fd) == -1){
            W_WARNING("Failed to close fd:" << strerror(errno));
        }
        return;
    }
    m_file.close();
    if(!m_file.open(fd, QFile::ReadWrite, QFile::AutoCloseHandle)){
        W_WARNING("Failed to open QFile:" << m_file.errorString());
        if(::close(fd) == -1){
            W_WARNING("Failed to close fd:" << strerror(errno));
        }
        return;
    }
    m_data = m_file.map(0, size);
    if(m_data == nullptr){
        W_WARNING("Failed to map framebuffer:" << strerror(errno));
        m_file.close();
        return;
    }
    memcpy(m_data, blankImage.constBits(), size);
    m_bytesPerLine = blankImage.bytesPerLine();
    auto oldGeometry = m_geometry;
    m_geometry = geometry;
    emit sizeInBytesChanged(size);
    emit bytesPerLineChanged(m_bytesPerLine);
    emit geometryChanged(oldGeometry, m_geometry);
    emit frameBufferChanged(QDBusUnixFileDescriptor(fd));
    W_DEBUG("Framebuffer created:" << geometry);
}

bool Window::writeEvent(EventPipe* pipe, const input_event& event){
    if(!m_enabled){
        W_WARNING("Failed to write to event pipe: Window disabled");
        return false;
    }
    if(!pipe->isOpen()){
        W_WARNING("Failed to write to event pipe: Pipe not open");
        return false;
    }
    auto size = sizeof(input_event);
    auto res = pipe->write((const char*)&event, size);
    if(res == -1){
        W_WARNING("Failed to write to event pipe:" << pipe->writeSocket()->errorString());
        return false;
    }
    if(res != size){
        W_WARNING("Only wrote" << res << "of" << size << "bytes to pipe");
    }
    if(!pipe->flush()){
        W_WARNING("Failed to flush event pipe: " << pipe->writeSocket()->errorString());
    }
#ifdef DEBUG_EVENTS
    W_DEBUG(event.time.tv_sec << event.time.tv_usec << event.type << event.code << event.value);
#endif
    return true;
}
