#include <sys/file.h>

#include "window.h"
#include "guiapi.h"
#include "appsapi.h"
#include "dbusservice.h"


using namespace  Oxide::Tarnish;

#define _W_WARNING(msg) O_WARNING(m_identifier.toStdString().c_str() << msg << guiAPI->getSenderPgid())
#define _W_DEBUG(msg) O_DEBUG(m_identifier.toStdString().c_str() << msg << guiAPI->getSenderPgid())
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
  m_eventPipe{true},
  m_pendingMarker{0}
{
    LOCK_MUTEX;
    m_touchEventPipe.setEnabled(false);
    m_tabletEventPipe.setEnabled(false);
    m_keyEventPipe.setEnabled(false);
    createFrameBuffer(geometry);
    O_DEBUG(m_identifier.toStdString().c_str() << "Window created" << pgid);
    connect(&m_eventPipe, &SocketPair::readyRead, this, &Window::readyEventPipeRead);
    m_pingTimer.setInterval(30 * 1000); // 30 seconds
    m_pingTimer.setSingleShot(true);
    m_pingTimer.setTimerType(Qt::PreciseTimer);
    m_pingDeadlineTimer.setInterval(60 * 1000); // 1 minute
    m_pingDeadlineTimer.setSingleShot(true);
    m_pingDeadlineTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_pingTimer, &QTimer::timeout, this, &Window::ping);
    connect(&m_pingDeadlineTimer, &QTimer::timeout, this, &Window::pingDeadline);
    connect(this, &Window::destroyed, [this]{ W_DEBUG("Window destoyred"); });
}
Window::~Window(){
    O_DEBUG(m_identifier.toStdString().c_str() << "Window deleted" << m_pgid);
    _close();
}

void Window::setEnabled(bool enabled){
    if(m_enabled != enabled){
        if(_isVisible()){
            invalidateEventPipes();
        }
        // TODO - Return to previous state, as they might not have ever been enabled.
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
    writeEvent(GeometryEventArgs{
        .x = m_geometry.x(),
        .y = m_geometry.y(),
        .width = m_geometry.width(),
        .height = m_geometry.height(),
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
    if(!m_pingDeadlineTimer.isActive()){
        m_pingTimer.start();
        m_pingDeadlineTimer.start();
    }
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

bool Window::isAppWindow(){
    for(auto name : appsAPI->runningApplications().keys()){
        auto app = appsAPI->getApplication(name);
        if(app->processId() == this->pgid()){
            return true;
        }
    }
    return false;
}

bool Window::isAppPaused(){
    for(auto name : appsAPI->pausedApplications().keys()){
        auto app = appsAPI->getApplication(name);
        if(app->processId() == this->pgid()){
            return true;
        }
    }
    return false;
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
        case WindowState::Closed:
            return;
        default:
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
        guiAPI->dirty(this, m_geometry, EPFrameBuffer::Initialize, 0, false);
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

void Window::_repaint(QRect region, EPFrameBuffer::WaveformMode waveform, unsigned int marker, bool async){
    if(marker > 0){
        m_pendingMarker = marker;
    }
    if(_isVisible()){
        W_DEBUG("Repaint" << marker << "queued");
        guiAPI->dirty(this, region, waveform, marker, async);
    }else{
        W_DEBUG("Repaint" << marker << "skipped");
        guiAPI->guiThread()->addCompleted(identifier(), marker, 1, true);
    }
}

void Window::_raise(bool async){
    switch(m_state){
        case WindowState::Lowered:
            m_state = WindowState::Raised;
        break;
        case WindowState::LoweredHidden:
            m_state = WindowState::RaisedHidden;
        break;
        case WindowState::Raised:
        case WindowState::RaisedHidden:
        case WindowState::Closed:
            return;
        default:
        break;
    }
    m_z = std::numeric_limits<int>::max();
    guiAPI->sortWindows();
    invalidateEventPipes();
    guiAPI->dirty(this, m_geometry, EPFrameBuffer::Initialize, 0, async);
    emit stateChanged(m_state);
    writeEvent(WindowEventType::Raise);
    emit raised();
}

void Window::_lower(bool async){
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
        case WindowState::Closed:
            return;
        default:
        break;
    }
    if(wasVisible){
        invalidateEventPipes();
        guiAPI->dirty(this, m_geometry, EPFrameBuffer::Initialize, 0, async);
    }
    m_z = std::numeric_limits<int>::min();
    guiAPI->sortWindows();
    emit stateChanged(m_state);
    writeEvent(WindowEventType::Lower);
    emit lowered();
}

void Window::_close(){
    if(m_state == WindowState::Closed){
        return;
    }
    m_state = WindowState::Closed;
    writeEvent(WindowEventType::Close);
    m_eventPipe.close();
    setEnabled(false);
    m_touchEventPipe.close();
    m_tabletEventPipe.close();
    m_keyEventPipe.close();
    emit closed();
    setParent(nullptr);
    moveToThread(nullptr);
    auto thread = guiAPI->guiThread();
    thread->deleteWindowLater(this);
    blockSignals(true);
    W_DEBUG("Window closed");
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

void Window::waitForUpdate(unsigned int marker, std::function<void()> callback){
    W_DEBUG("Waiting for update" << marker);
    auto thread = guiAPI->guiThread()->waitThread();
    thread->addWait(this, marker, [this, callback, marker]{
        callback();
        W_DEBUG("Done waiting for update" << marker);
    });
    QCoreApplication::postEvent(thread, new QEvent(QEvent::UpdateRequest));
}

void Window::waitForLastUpdate(QDBusMessage message){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    message.setDelayedReply(true);
    auto reply = message.createReply();
    guiAPI->connection().send(reply);
    waitForUpdate(m_pendingMarker, [this, reply]{
        guiAPI->connection().send(reply);
    });
}

void Window::writeEvent(KeyEventArgs args){
    if(m_eventPipe.enabled()){
        WindowEvent event;
        event.type = WindowEventType::Key;
        event.keyData = args;
        W_DEBUG(event);
        event.toSocket(m_eventPipe.writeSocket());
    }
}

void Window::writeEvent(TouchEventArgs args){
    if(m_eventPipe.enabled()){
        WindowEvent event;
        event.type = WindowEventType::Touch;
        event.touchData = args;
        W_DEBUG(event);
        event.toSocket(m_eventPipe.writeSocket());
    }
}

void Window::writeEvent(TabletEventArgs args){
    if(m_eventPipe.enabled()){
        WindowEvent event;
        event.type = WindowEventType::Tablet;
        event.tabletData = args;
        W_DEBUG(event);
        event.toSocket(m_eventPipe.writeSocket());
    }
}

void Window::disableEventPipe(){ m_eventPipe.setEnabled(false); }

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
    writeEvent(GeometryEventArgs{
       .x = m_geometry.x(),
       .y = m_geometry.y(),
       .width = m_geometry.width(),
       .height = m_geometry.height(),
        .z = m_z
    });
    emit geometryChanged(oldGeometry, m_geometry);
    if(wasVisible){
        guiAPI->dirty(this, oldGeometry, EPFrameBuffer::Initialize, 0, false);
    }
    if(_isVisible()){
        guiAPI->dirty(this, m_geometry, EPFrameBuffer::Initialize, 0, false);
    }
}

void Window::repaint(QRect region, int waveform){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    _repaint(region, (EPFrameBuffer::WaveformMode)waveform, 0, false);
}

void Window::repaint(int waveform){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    _repaint(m_geometry, (EPFrameBuffer::WaveformMode)waveform, 0, false);
}

void Window::raise(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    _raise(false);
}

void Window::lower(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    _lower(false);
}

void Window::close(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED();
    QTimer::singleShot(0, [this]{ _close(); });
}

void Window::readyEventPipeRead(){
    W_DEBUG("Reading events");
    while(!m_eventPipe.atEnd() && m_eventPipe.isOpen() && m_eventPipe.enabled()){
        auto event = WindowEvent::fromSocket(m_eventPipe.writeSocket());
        W_DEBUG(event);
        switch(event.type){
            case Repaint:{
                auto marker = event.repaintData.marker;
                _repaint(event.repaintData.geometry(), event.repaintData.waveform, marker);
                break;
            }
            case Geometry:{
                createFrameBuffer(event.geometryData.geometry());
                break;
            }
            case ImageInfo:{
                // Send back real info?
                break;
            }
            case WaitForPaint:{
                auto marker = event.waitForPaintData.marker ? event.waitForPaintData.marker : m_pendingMarker;
                W_DEBUG("Waiting for update" << marker);
                waitForUpdate(marker, [this, marker]{
                    W_DEBUG("Update" << marker << "is finished");
                    writeEvent(WaitForPaintEventArgs{ .marker = marker });
                });
                break;
            }
            case Raise:
                _raise(0);
                break;
            case Lower:
                _lower(0);
                break;
            case Close:
                _close();
                break;
            case Ping:
                m_pingTimer.stop();
                m_pingDeadlineTimer.stop();
                m_pingTimer.start();
                m_pingDeadlineTimer.start();
                break;
            case Touch:
            case Key:
            case FrameBuffer:
            case Invalid:
            default:
                break;
        }
    }
}

void Window::ping(){ writeEvent(WindowEventType::Ping); }

void Window::pingDeadline(){
    if(m_eventPipe.enabled()){
        _W_WARNING("Process failed to respond to ping, it may be deadlocked");
        //_close();
    }
}

bool Window::hasPermissions(){ return !DBusService::shuttingDown() && guiAPI->isThisPgId(m_pgid); }

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
    blankImage.fill(blankImage.hasAlphaChannel() ? Qt::transparent : Qt::white);
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
    writeEvent(ImageInfoEventArgs{
        .sizeInBytes = size,
        .bytesPerLine = m_bytesPerLine,
        .format = m_format
    });
    writeEvent(GeometryEventArgs{
       .x = m_geometry.x(),
       .y = m_geometry.y(),
       .width = m_geometry.width(),
       .height = m_geometry.height(),
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
    if(!pipe->enabled()){
        return false;
    }
    // TODO - add logic to skip emitting anything until the next EV_SYN SYN_REPORT
    //        after the window has changed states to now be visible/enabled.
    //        Although this may not be required if we replace this with the events
    //        pipe instead
    std::string name;
    if(pipe == &m_touchEventPipe){
        name = "touch";
    }else if(pipe == &m_tabletEventPipe){
        name = "tablet";
    }else if(pipe == &m_keyEventPipe){
        name = "key";
    }else{
        Q_ASSERT(false);
    }
    if(!force && !m_enabled){
        _W_WARNING("Failed to write to " << name.c_str() << " event pipe: Window disabled");
        return false;
    }
    if(!pipe->isOpen()){
        _W_WARNING("Failed to write to " << name.c_str() << " event pipe: Pipe not open");
        return false;
    }
    auto size = sizeof(input_event);
    auto res = force ? pipe->_write((const char*)&event, size) : pipe->write((const char*)&event, size);
    if(res == -1){
        _W_WARNING("Failed to write to " << name.c_str() << " event pipe:" << pipe->writeSocket()->errorString());
        return false;
    }
    if(res != size){
        _W_WARNING("Only wrote" << res << "of" << size << "bytes to " << name.c_str() << "event pipe");
    }
#ifdef DEBUG_EVENTS
    _W_DEBUG(name.c_str() << event.input_event_sec << event.input_event_usec << event.type << event.code << event.value);
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
    if(m_eventPipe.enabled()){
        WindowEvent event;
        event.type = type;
        W_DEBUG(event);
        event.toSocket(m_eventPipe.writeSocket());
    }
}

void Window::writeEvent(RepaintEventArgs args){
    if(m_eventPipe.enabled()){
        WindowEvent event;
        event.type = WindowEventType::Repaint;
        event.repaintData = args;
        W_DEBUG(event);
        event.toSocket(m_eventPipe.writeSocket());
    }
}

void Window::writeEvent(GeometryEventArgs args){
    if(m_eventPipe.enabled()){
        WindowEvent event;
        event.type = WindowEventType::Geometry;
        event.geometryData = args;
        W_DEBUG(event);
        event.toSocket(m_eventPipe.writeSocket());
    }
}

void Window::writeEvent(ImageInfoEventArgs args){
    if(m_eventPipe.enabled()){
        WindowEvent event;
        event.type = WindowEventType::ImageInfo;
        event.imageInfoData = args;
        W_DEBUG(event);
        event.toSocket(m_eventPipe.writeSocket());
    }
}

void Window::writeEvent(WaitForPaintEventArgs args){
    if(m_eventPipe.enabled()){
        WindowEvent event;
        event.type = WindowEventType::WaitForPaint;
        event.waitForPaintData = args;
        W_DEBUG(event);
        event.toSocket(m_eventPipe.writeSocket());
    }
}
