#include "window.h"
#include "guiapi.h"

#include <sys/file.h>

#define _W_WARNING(msg) O_WARNING(identifier() << __PRETTY_FUNCTION__ << msg << guiAPI->getSenderPgid())
#define _W_DEBUG(msg) O_DEBUG(identifier() << __PRETTY_FUNCTION__ << msg << guiAPI->getSenderPgid())
#define W_WARNING(msg) _W_WARNING(msg << guiAPI->getSenderPgid())
#define W_DEBUG(msg) _W_DEBUG(msg << guiAPI->getSenderPgid())
#define W_DENIED() W_DEBUG("DENY")
#define W_ALLOWED() W_DEBUG("ALLOW")
#define LOCK_MUTEX \
    QMutexLocker locker(&m_mutex); \
    Q_UNUSED(locker);



Window::Window(const QString& id, const QString& path, const pid_t& pgid, const QRect& geometry, int z, QImage::Format format)
: QObject{guiAPI},
  m_identifier{id},
  m_enabled{false},
  m_path{path},
  m_pgid{pgid},
  m_geometry{geometry},
  m_z{z},
  m_file{this},
  m_state{WindowState::LoweredHidden},
  m_format{(QImage::Format)format},
  m_eventPipe{true}
{
    LOCK_MUTEX;
    createFrameBuffer(geometry);
    O_DEBUG(m_identifier << __PRETTY_FUNCTION__ << "Window created" << pgid);
}
Window::~Window(){
    O_DEBUG(m_identifier << __PRETTY_FUNCTION__ << "Window closed" << m_pgid);
}

void Window::setEnabled(bool enabled){
    if(m_enabled != enabled){
        invalidateEventPipes();
        m_touchEventPipe.setEnabled(enabled);
        m_tabletEventPipe.setEnabled(enabled);
        m_keyEventPipe.setEnabled(enabled);
        m_enabled = enabled;
    }
    auto bus = QDBusConnection::systemBus();
    bool registered = bus.objectRegisteredAt(m_path) != nullptr;
    if(enabled && registered){
        return;
    }
    if(!enabled && !registered){
        return;
    }
    if(!enabled){
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
    m_touchEventPipe.setEnabled(true);
    return QDBusUnixFileDescriptor(m_touchEventPipe.readSocket()->socketDescriptor());
}

QDBusUnixFileDescriptor Window::tabletEventPipe(){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED();
    m_tabletEventPipe.setEnabled(true);
    return QDBusUnixFileDescriptor(m_tabletEventPipe.readSocket()->socketDescriptor());
}

QDBusUnixFileDescriptor Window::keyEventPipe(){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED();
    m_keyEventPipe.setEnabled(true);
    return QDBusUnixFileDescriptor(m_keyEventPipe.readSocket()->socketDescriptor());
}

QDBusUnixFileDescriptor Window::eventPipe(){
    // TODO - add to liboxide and flush out spec for local aware events
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED();
    m_eventPipe.setEnabled(true);
    return QDBusUnixFileDescriptor(m_eventPipe.readSocket()->socketDescriptor());
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
    if(wasVisible != _isVisible()){
        invalidateEventPipes();
    }
    emit stateChanged(m_state);
    if(wasVisible != _isVisible()){
        emit dirty(m_geometry, EPFrameBuffer::Initialize);
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
    invalidateEventPipes();
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

void Window::waitForLastUpdate(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    guiAPI->waitForLastUpdate();
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

void Window::repaint(QRect region, int waveform){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    if(_isVisible()){
        invalidateEventPipes();
        emit dirty(region, (EPFrameBuffer::WaveformMode)waveform);
    }
}

void Window::repaint(int waveform){ repaint(m_geometry, waveform); }

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
            return;
        default:
        break;
    }
    if(_isVisible()){
        invalidateEventPipes();
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
            return;
        default:
        break;
    }
    if(wasVisible){
        invalidateEventPipes();
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

bool Window::writeEvent(SocketPair* pipe, const input_event& event, bool force){
    if(!force){
        if(!pipe->enabled()){
            return false;
        }
        if(!m_enabled){
            _W_WARNING("Failed to write to event pipe: Window disabled");
            return false;
        }
    }
    if(!pipe->isOpen()){
        _W_WARNING("Failed to write to event pipe: Pipe not open");
        return false;
    }
    auto size = sizeof(input_event);
    auto res = force ? pipe->_write((const char*)&event, size) : pipe->write((const char*)&event, size);
    if(res == -1){
        _W_WARNING("Failed to write to event pipe:" << pipe->writeSocket()->errorString());
        return false;
    }
    if(res != size){
        _W_WARNING("Only wrote" << res << "of" << size << "bytes to pipe");
    }
#ifdef DEBUG_EVENTS
    _W_DEBUG(event.input_event_sec << event.input_event_usec << event.type << event.code << event.value);
#endif
    return true;
}

void Window::invalidateEventPipes(){
    timeval time;
    if(gettimeofday(&time, NULL) == -1){
        _W_WARNING("Failed to get time of day:" << strerror(errno));
    }
    writeEvent(&m_touchEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_DROPPED,
        .value = 0,
    }, true);
    writeEvent(&m_tabletEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_DROPPED,
        .value = 0,
    }, true);
    writeEvent(&m_keyEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_DROPPED,
        .value = 0,
    }, true);
}
