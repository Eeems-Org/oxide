#include <sys/file.h>

#include "window.h"
#include "guiapi.h"


using namespace  Oxide::Tarnish;

#define _W_WARNING(msg) O_WARNING(__PRETTY_FUNCTION__ << m_identifier << msg << guiAPI->getSenderPgid())
#define _W_DEBUG(msg) O_DEBUG(__PRETTY_FUNCTION__ << m_identifier << msg << guiAPI->getSenderPgid())
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
    O_DEBUG(__PRETTY_FUNCTION__ << m_identifier << "Window created" << pgid);
    connect(&m_eventPipe, &SocketPair::readyRead, this, &Window::readyEventPipeRead);
}
Window::~Window(){
    O_DEBUG(__PRETTY_FUNCTION__ << m_identifier << "Window closed" << m_pgid);
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
    writeEvent<GeometryEventArgs>(WindowEventType::Geometry, GeometryEventArgs{
        .geometry = m_geometry,
        .z = m_z
    });
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
        guiAPI->dirty(this, m_geometry, EPFrameBuffer::Initialize);
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
    writeEvent(WindowEventType::Close);
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
    writeEvent<GeometryEventArgs>(WindowEventType::Geometry, GeometryEventArgs{
        .geometry = m_geometry,
        .z = m_z
    });
    emit geometryChanged(oldGeometry, m_geometry);
    if(wasVisible){
        guiAPI->dirty(this, oldGeometry);
    }
    if(_isVisible()){
        guiAPI->dirty(this, m_geometry);
    }
}

void Window::repaint(QRect region, int waveform){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    if(_isVisible()){
        W_DEBUG(region << waveform);
        guiAPI->dirty(this, region, (EPFrameBuffer::WaveformMode)waveform);
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
        guiAPI->dirty(this, m_geometry);
    }
    emit stateChanged(m_state);
    writeEvent(WindowEventType::Raise);
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
        guiAPI->dirty(this, m_geometry);
    }
    emit stateChanged(m_state);
    writeEvent(WindowEventType::Lower);
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

void Window::readyEventPipeRead(){
    auto in = m_eventPipe.readStream();
    auto out = m_eventPipe.writeStream();
    while(!in->atEnd()){
        WindowEvent event;
        *in >> event;
        switch(event.type){
            case Repaint:{
                auto args = static_cast<RepaintEventArgs*>(event.data);
                repaint(args->geometry, args->waveform);
                break;
            }
            case Geometry:{
                auto args = static_cast<GeometryEventArgs*>(event.data);
                setGeometry(args->geometry);
                break;
            }
            case ImageInfo:{
                break;
            }
            case WaitForPaint:{
                waitForLastUpdate();
                out << event;
                break;
            }
            case Raise:
                raise();
                break;
            case Lower:
                lower();
                break;
            case Close:
                close();
                break;
            case FrameBuffer:
                break;
        }
    }
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
    writeEvent<ImageInfoEventArgs>(WindowEventType::ImageInfo, ImageInfoEventArgs{
        .sizeInBytes = size,
        .bytesPerLine = m_bytesPerLine,
        .format = m_format
    });
    writeEvent<GeometryEventArgs>(WindowEventType::Geometry, GeometryEventArgs{
        .geometry = m_geometry,
        .z = m_z
    });
    writeEvent(WindowEventType::FrameBuffer);
    emit sizeInBytesChanged(size);
    emit bytesPerLineChanged(m_bytesPerLine);
    emit geometryChanged(oldGeometry, m_geometry);
    emit frameBufferChanged(QDBusUnixFileDescriptor(fd));
    W_DEBUG("Framebuffer created:" << geometry);
}

bool Window::writeEvent(SocketPair* pipe, const input_event& event, bool force){
    // TODO - add logic to skip emitting anything until the next EV_SYN SYN_REPORT
    //        after the window has changed states to now be visible/enabled
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
    // Invalidate anything in transmission
    writeEvent(&m_touchEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_DROPPED,
        .value = 0,
    }, true);
    // End the invalid event so we can insert our own
    writeEvent(&m_touchEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_REPORT,
        .value = 0,
    }, true);
    // Release all touch slots
    for(auto i = 0; i < deviceSettings.getTouchSlots(); i++){
        writeEvent(&m_touchEventPipe, input_event{
            .time = time,
            .type = EV_ABS,
            .code = ABS_MT_SLOT,
            .value = i,
        }, true);
        writeEvent(&m_touchEventPipe, input_event{
            .time = time,
            .type = EV_SYN,
            .code = SYN_MT_REPORT,
            .value = 0,
        }, true);
    }
    writeEvent(&m_touchEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_REPORT,
        .value = 0,
    }, true);
    // Invalidate anything still in transmission
    writeEvent(&m_touchEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_DROPPED,
        .value = 0,
    }, true);

    // Invalidate anything in transmission
    writeEvent(&m_tabletEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_DROPPED,
        .value = 0,
    }, true);
    // End the invalid event so we can insert our own
    writeEvent(&m_tabletEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_REPORT,
        .value = 0,
    }, true);
    // Release pen touch
    writeEvent(&m_tabletEventPipe, input_event{
        .time = time,
        .type = EV_KEY,
        .code = BTN_TOOL_PEN,
        .value = 0,
    }, true);
    // Invalidate anything still in transmission
    writeEvent(&m_tabletEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_DROPPED,
        .value = 0,
    }, true);
    // Invalidate anything still in transmission
    writeEvent(&m_keyEventPipe, input_event{
        .time = time,
        .type = EV_SYN,
        .code = SYN_DROPPED,
        .value = 0,
    }, true);
}

void Window::writeEvent(WindowEventType type){
    WindowEvent event;
    event.type = type;
    auto out = m_eventPipe.writeStream();
    out << event;
}
