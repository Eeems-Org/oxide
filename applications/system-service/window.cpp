#include "window.h"
#include "guiapi.h"

#define W_WARNING(msg) O_WARNING(identifier() << __PRETTY_FUNCTION__ << msg << guiAPI->getSenderPgid())
#define W_DEBUG(msg) O_DEBUG(identifier() << __PRETTY_FUNCTION__ << msg << guiAPI->getSenderPgid())
#define W_DENIED() W_DEBUG("DENY")
#define W_ALLOWED() W_DEBUG("ALLOW")

Window::Window(const QString& id, const QString& path, const pid_t& pid, const QRect& geometry, QObject* parent)
: QObject{parent},
  m_identifier{id},
  m_enabled{false},
  m_path{path},
  m_pid{pid},
  m_geometry{geometry},
  m_fd{-1},
  m_state{WindowState::LoweredHidden}
{
    O_DEBUG(id << __PRETTY_FUNCTION__ << "Created" << pid);
    createFrameBuffer(geometry);
}
Window::~Window(){
    if(m_data != nullptr){
        munmap(m_data, m_image->sizeInBytes());
        delete m_data;
    }
    if(m_image != nullptr){
        delete m_image;
    }
    if(m_fd != -1){
        ::close(m_fd);
    }
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

QDBusUnixFileDescriptor Window::frameBuffer(){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED()
    return QDBusUnixFileDescriptor(m_fd);
}

QRect Window::geometry(){
    if(!hasPermissions()){
        W_DENIED();
        return QRect();
    }
    W_ALLOWED()
    return m_geometry;
}

void Window::setGeometry(const QRect& geometry){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED()
    createFrameBuffer(geometry);
}

bool Window::isVisible(){
    if(!hasPermissions()){
        W_DENIED();
        return false;
    }
    W_ALLOWED()
    if(m_image == nullptr || m_state != WindowState::Raised){
        return false;
    }
    return guiAPI->geometry().intersects(m_geometry);
}

void Window::setVisible(bool visible){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED()
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
    if(state != m_state){
        emit stateChanged(m_state);
    }
}

QImage* Window::toImage(){ return m_image; }

qulonglong Window::sizeInBytes(){
    if(!hasPermissions()){
        W_DENIED();
        return 0;
    }
    W_ALLOWED()
    return m_image == nullptr ? 0 : m_image->sizeInBytes();
}

qulonglong Window::bytesPerLine(){
    if(!hasPermissions()){
        W_DENIED();
        return 0;
    }
    W_ALLOWED()
    return m_image == nullptr ? 0 : m_image->bytesPerLine();
}

int Window::format(){
    if(!hasPermissions()){
        W_DENIED();
        return QImage::Format_Invalid;
    }
    W_ALLOWED()
    return m_image == nullptr ? QImage::Format_Invalid : m_image->format();
}

QDBusUnixFileDescriptor Window::resize(int width, int height){
    if(!hasPermissions()){
        W_DENIED();
        return QDBusUnixFileDescriptor();
    }
    W_ALLOWED()
    createFrameBuffer(QRect(m_geometry.x(), m_geometry.y(), width, height));
    return QDBusUnixFileDescriptor(m_fd);
}

void Window::move(int x, int y){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED()
    auto wasVisible = isVisible();
    auto oldGeometry = m_geometry;
    m_geometry.setX(x);
    m_geometry.setY(y);
    emit geometryChanged(oldGeometry, m_geometry);
    if(wasVisible){
        emit dirty(oldGeometry);
    }
    if(isVisible()){
        emit dirty(m_geometry);
    }
}

void Window::repaint(QRect region){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED()
    if(isVisible()){
        emit dirty(region);
    }
}

void Window::repaint(){ repaint(geometry()); }

void Window::raise(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED()
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
    if(isVisible()){
        emit dirty(geometry());
    }
    emit stateChanged(m_state);
    emit raised();
}

void Window::lower(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED()
    bool wasVisible = isVisible();
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
        emit dirty(geometry());
    }
    emit stateChanged(m_state);
    emit lowered();
}

void Window::close(){
    if(!hasPermissions()){
        W_DENIED();
        return;
    }
    W_ALLOWED()
    bool wasVisible = isVisible();
    m_state = WindowState::LoweredHidden;
    emit closed();
    if(wasVisible){
        emit dirty(geometry());
    }
}

bool Window::hasPermissions(){ return guiAPI->isThisPgId(m_pid); }

void Window::createFrameBuffer(const QRect& geometry){
    // TODO - add mutex
    if(m_fd != -1 && geometry == m_geometry){
        W_WARNING("No need to resize:" << geometry);
        return;
    }
    if(m_data != nullptr){
        munmap(m_data, m_image->sizeInBytes());
        delete m_data;
        m_data = nullptr;
    }
    if(m_image != nullptr){
        delete m_image;
        m_image = nullptr;
    }
    if(m_fd != -1){
        ::close(m_fd);
        m_fd = -1;
    }
    if(geometry.isEmpty() || geometry.isNull() || !geometry.isValid()){
        W_WARNING("Invalid geometry for framebuffer:" << geometry);
        return;
    }
    int fd = memfd_create(m_identifier.toStdString().c_str(), MFD_ALLOW_SEALING);
    if(fd == -1){
        W_WARNING("Unable to create memfd for framebuffer:" << strerror(errno));
        return;
    }
    QImage blankImage(geometry.width(), geometry.height(), QImage::Format_RGB16);
    blankImage.fill(Qt::white);
    size_t size = blankImage.sizeInBytes();
    if(ftruncate(fd, size)){
        W_WARNING("Unable to truncate memfd for framebuffer:" << strerror(errno));
        ::close(fd);
        return;
    }
    int flags = fcntl(fd, F_GET_SEALS);
    if(fcntl(fd, F_ADD_SEALS, flags | F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW)){
        W_WARNING("Unable to seal memfd for framebuffer:" << strerror(errno));
        ::close(fd);
        return;
    }
    auto data = (uchar*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, fd, 0);
    if(data == MAP_FAILED){
        W_WARNING("Failed to map framebuffer:" << strerror(errno));
        ::close(fd);
        return;
    }
    m_data = data;
    m_fd = fd;
    memcpy(m_data, blankImage.constBits(), size);
    m_image = new QImage(m_data, geometry.width(), geometry.height(), blankImage.bytesPerLine(), QImage::Format_RGB16);
    auto oldGeometry = m_geometry;
    m_geometry = geometry;
    emit sizeInBytesChanged(m_image->sizeInBytes());
    emit bytesPerLineChanged(m_image->bytesPerLine());
    emit formatChanged(m_image->format());
    emit geometryChanged(oldGeometry, m_geometry);
    emit frameBufferChanged(QDBusUnixFileDescriptor(m_fd));
    W_DEBUG("Framebuffer created:" << geometry);
}
