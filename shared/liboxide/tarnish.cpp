#include "tarnish.h"
#include "meta.h"
#include "qt.h"

#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/file.h>
#include <QDBusConnection>
#include <QImage>
#include <QThread>
#include <QAtomicInteger>

using namespace Oxide::Tarnish;

codes::eeems::oxide1::General* api_general = nullptr;
codes::eeems::oxide1::Power* api_power = nullptr;
codes::eeems::oxide1::Wifi* api_wifi = nullptr;
codes::eeems::oxide1::Screen* api_screen = nullptr;
codes::eeems::oxide1::Apps* api_apps = nullptr;
codes::eeems::oxide1::System* api_system = nullptr;
codes::eeems::oxide1::Notifications* api_notification = nullptr;
codes::eeems::oxide1::Gui* api_gui = nullptr;
codes::eeems::oxide1::Window* window = nullptr;
QRect fbGeometry;
qulonglong fbLineSize = 0;
QImage::Format fbFormat = QImage::Format_Invalid;
uchar* fbData = nullptr;
QFile fbFile;
QMutex fbMutex;
QThread inputThread;
QLocalSocket childSocket;
InputEventSocket touchEventSocket;
InputEventSocket tabletEventSocket;
InputEventSocket keyEventSocket;
QLocalSocket eventSocket;
QDataStream eventStream;
QAtomicInteger repaintMarker(1);

// TODO - Get liboxide to automate journalctl logging
//        https://doc.qt.io/qt-5/qtglobal.html#qInstallMessageHandler

void startInputThread(){
    if(inputThread.isRunning()){
        return;
    }
    O_DEBUG("Starting input thread");
    inputThread.setObjectName("input");
    inputThread.start();
    inputThread.setPriority(QThread::TimeCriticalPriority);
}

bool verifyConnection(){
    if(api_general == nullptr){
        return false;
    }
    if(!api_general->isValid()){
        api_general->deleteLater();
        api_general = nullptr;
        return false;
    }
    return true;
}

bool setupFbFd(int fd){
    fbFile.close();
    if(!fbFile.open(fd, QFile::ReadWrite, QFile::AutoCloseHandle)){
        O_WARNING(__PRETTY_FUNCTION__ << "Unable to open framebuffer QFile:" << fbFile.errorString());
        if(::close(fd) == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable close fd:" << strerror(errno));
        }
        return false;
    }
    fbGeometry = window->geometry();
    fbLineSize = window->bytesPerLine();
    fbFormat = (QImage::Format)window->format();
    O_DEBUG(__PRETTY_FUNCTION__ << "FrameBuffer" << fbGeometry << fbLineSize << fbFormat << fbFile.size() - sizeof(std::mutex));
    return true;
}

void _disconnect(){
    fbData = nullptr;
    fbFile.close();
    childSocket.disconnect();
    childSocket.close();
    touchEventSocket.disconnect();
    touchEventSocket.close();
    tabletEventSocket.disconnect();
    tabletEventSocket.close();
    keyEventSocket.disconnect();
    keyEventSocket.close();
    eventSocket.disconnect();
    eventSocket.close();
#define freeAPI(name) \
    if(api_##name != nullptr){ \
        delete api_##name; \
        api_##name = nullptr; \
    }
    freeAPI(power);
    freeAPI(wifi);
    freeAPI(screen);
    freeAPI(apps);
    freeAPI(system);
    freeAPI(notification);
    freeAPI(general);
    freeAPI(gui);
#undef freeAPI
    if(inputThread.isRunning()){
        inputThread.quit();
        inputThread.requestInterruption();
        inputThread.wait();
    }
    if(window != nullptr){
        delete window;
        window = nullptr;
    }
}

namespace Oxide::Tarnish {
    InputEventSocket::InputEventSocket() : QLocalSocket(), stream(this){
        QObject::connect(this, &QLocalSocket::readyRead, this, &InputEventSocket::readEvent);
    }

    InputEventSocket::~InputEventSocket() { }

    void InputEventSocket::setCallback(std::function<void(const input_event&)> callback){ m_callback = callback; }

    void InputEventSocket::readEvent(){
        auto size = sizeof(input_event);
        while(!stream.atEnd()){
            input_event event;
            if(stream.readRawData((char*)&event, size) == -1){
#ifdef DEBUG_EVENTS
                O_WARNING(__PRETTY_FUNCTION__ << "Failed to read event" << strerror(errno));
#endif
                return;
            }
            O_EVENT(event);
            emit inputEvent(event);
            if(m_callback != nullptr){
                m_callback(event);
            }
        }
    }

    QByteArray& operator>>(QByteArray& l, RepaintEventArgs& r){
        Q_ASSERT_X(l.size() >= RepaintEventArgs::size, "QByteArray >> RepaintEventArgs", "Not enough data available");
        int x, y, width, height;
        quint64 waveform;
        l >> r.marker >> waveform >> height >> width >> y >> x;
        r.geometry.setRect(x, y, width, height);
        r.waveform = (EPFrameBuffer::WaveformMode)waveform;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, RepaintEventArgs& r){
        QByteArray a;
        a << r.geometry.x()
            << r.geometry.y()
            << r.geometry.width()
            << r.geometry.height()
            << (quint64)r.waveform
            << r.marker;
        Q_ASSERT_X(
            a.size() == RepaintEventArgs::size, "QByteArray << RepaintEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(RepaintEventArgs::size).toStdString().c_str()
        );
        l += a;
        return l;
    }

    QByteArray& operator>>(QByteArray& l, GeometryEventArgs& r){
        Q_ASSERT_X(l.size() >= GeometryEventArgs::size, "QByteArray >> GeometryEventArgs", "Not enough data available");
        int x, y, width, height;
        l >> r.z >> height >> width >> y >> x;
        r.geometry.setRect(x, y, width, height);
        return l;
    }

    QByteArray& operator<<(QByteArray& l, GeometryEventArgs& r){
        QByteArray a;
        a << r.geometry.x()
            << r.geometry.y()
            << r.geometry.width()
            << r.geometry.height()
            << r.z;
        Q_ASSERT_X(
            a.size() == GeometryEventArgs::size, "QByteArray << GeometryEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(GeometryEventArgs::size).toStdString().c_str()
        );
        l += a;
        return l;
    }

    QByteArray& operator>>(QByteArray& l, ImageInfoEventArgs& r){
        Q_ASSERT_X(l.size() >= ImageInfoEventArgs::size, "QByteArray >> ImageInfoEventArgs", "Not enough data available");
        int format;
        l >> format >> r.bytesPerLine >> r.sizeInBytes;
        r.format = (QImage::Format)format;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, ImageInfoEventArgs& r){
        QByteArray a;
        a << r.sizeInBytes
            << r.bytesPerLine
            << (int)r.format;
        Q_ASSERT_X(
            a.size() == ImageInfoEventArgs::size, "QByteArray << ImageInfoEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(ImageInfoEventArgs::size).toStdString().c_str()
        );
        l += a;
        return l;
    }

    QByteArray& operator>>(QByteArray& l, WaitForPaintEventArgs& r){
        Q_ASSERT_X(l.size() >= WaitForPaintEventArgs::size, "QByteArray >> WaitForPaintEventArgs", "Not enough data available");
        l >> r.marker;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, WaitForPaintEventArgs& r){
        QByteArray a;
        a << r.marker;
        Q_ASSERT_X(
            a.size() == WaitForPaintEventArgs::size, "QByteArray << WaitForPaintEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(WaitForPaintEventArgs::size).toStdString().c_str()
        );
        l += a;
        return l;
    }

    QDebug operator<<(QDebug debug, const WindowEventType& type){
        QDebugStateSaver saver(debug);
        Q_UNUSED(saver);
        switch(type){
            case Repaint: debug.nospace() << "Repaint"; break;
            case Geometry: debug.nospace() << "Geometry"; break;
            case ImageInfo: debug.nospace() << "ImageInfo"; break;
            case WaitForPaint: debug.nospace() << "WaitForPaint"; break;
            case Raise: debug.nospace() << "Raise"; break;
            case Lower: debug.nospace() << "Lower"; break;
            case Close: debug.nospace() << "Close"; break;
            case FrameBuffer: debug.nospace() << "FrameBuffer"; break;
            case Ping: debug.nospace() << "Ping"; break;
            case Invalid:
            default:
                 debug.nospace() << "Invalid";
        }
        return debug;
    }

    WindowEvent WindowEvent::fromSocket(QLocalSocket* socket){
        QMutexLocker locker(&m_readMutex);
        Q_UNUSED(locker);
        O_DEBUG(__PRETTY_FUNCTION__ << "Reading event from socket");
        WindowEvent event;
        if(socket->atEnd()){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed to read event from socket: There are no events waiting");
            return event;
        }
        qsizetype size = sizeof(unsigned short);
        auto data = socket->read(size);
        if(data.size() != size){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed to read event from socket: Expected" << size << "bytes but instead recieved" << data.size() << "bytes");
            return event;
        }
        unsigned short type;
        data >> type;
        switch((WindowEventType)type){
            case Repaint:{
                auto data = socket->read(event.repaintData.size);
                data >> event.repaintData;
                break;
            }
            case Geometry:{
                auto data = socket->read(event.geometryData.size);
                data >> event.geometryData;
                break;
            }
            case ImageInfo:{
                auto data = socket->read(event.imageInfoData.size);
                data >> event.imageInfoData;
                break;
            }
            case WaitForPaint:{
                auto data = socket->read(event.waitForPaintData.size);
                data >> event.waitForPaintData;
                break;
            }
            case Raise:
            case Lower:
            case Close:
            case FrameBuffer:
            case Ping:
            case Invalid:
                break;
            default:
                O_WARNING(__PRETTY_FUNCTION__ << "Unknown event type:" << type);
                // TODO - skip to end and send some sort of "oops, I lost track" event?
                return event;
        }
        event.type = (WindowEventType)type;
        O_DEBUG(__PRETTY_FUNCTION__ << "Read event from socket" << event);
        return event;
    }

    bool WindowEvent::toSocket(QLocalSocket* socket){
        QMutexLocker locker(&m_writeMutex);
        Q_UNUSED(locker);
        O_DEBUG(__PRETTY_FUNCTION__ << "Writing event to socket" << this);
        if(!socket->isWritable()){
            O_WARNING(__PRETTY_FUNCTION__ << "Socket is not writable");
            return false;
        }
        if(!isValid()){
            O_WARNING(__PRETTY_FUNCTION__ << "Invalid WindowEvent");
            return false;
        }
        QByteArray data;
        data << (unsigned short)type;
        switch(type){
            case Repaint:
                data << repaintData;
                break;
            case Geometry:
                data << geometryData;
                break;
            case ImageInfo:
                data << imageInfoData;
                break;
            case WaitForPaint:
                data << waitForPaintData;
                break;
            case Raise:
            case Lower:
            case Close:
            case FrameBuffer:
            case Ping:
                break;
            default:
                O_WARNING(__PRETTY_FUNCTION__ << "Unknown event type:" << type)
                return false;
        }
        auto res = socket->write(data);
        if(res < data.size()){
            O_WARNING(__PRETTY_FUNCTION__ << "Expected to write" << data.size() << "bytes but only wrote" << res << "bytes");
            return false;
        }
        socket->flush();
        return true;
    }

    WindowEvent::WindowEvent() : type{Invalid}{}

    WindowEvent::WindowEvent(const WindowEvent& event)
    : type{event.type},
      repaintData{event.repaintData},
      geometryData{event.geometryData},
      imageInfoData{event.imageInfoData}
    {}

    WindowEvent::~WindowEvent(){}

    bool WindowEvent::isValid(){
       switch(type){
            case Repaint:
            case Geometry:
            case ImageInfo:
            case WaitForPaint:
            case Raise:
            case Lower:
            case Close:
            case FrameBuffer:
            case Ping:
                return true;
            default:
                return false;
       }
    }

    QMutex WindowEvent::m_writeMutex;
    QMutex WindowEvent::m_readMutex;

    QDebug operator<<(QDebug debug, const WindowEvent& event){
        QDebugStateSaver saver(debug);
        Q_UNUSED(saver);
        debug.nospace() << "<WindowEvent " << event.type;
        switch(event.type){
            case Repaint:
                debug.nospace() << ' ' << event.repaintData.geometry << ' ' << event.repaintData.waveform << ' ' << event.repaintData.marker;
                break;
            case Geometry:
                debug.nospace() << ' ' << event.geometryData.geometry << ' ' << event.geometryData.z;
                break;
            case ImageInfo:
                debug.nospace() << ' ' << event.imageInfoData.sizeInBytes << ' ' << event.imageInfoData.bytesPerLine << ' ' << event.imageInfoData.format;
                break;
            case WaitForPaint:
                debug.nospace() << ' ' << event.waitForPaintData.marker;
            case Raise:
            case Lower:
            case Close:
            case FrameBuffer:
            case Invalid:
            case Ping:
            default:
                break;
        }
        debug.nospace() << '>';
        return debug;
    }

    QDebug operator<<(QDebug debug, WindowEvent* event){
        QDebugStateSaver saver(debug);
        Q_UNUSED(saver);
        if(event != nullptr){
            debug.nospace() << *event;
        }else{
            debug.nospace() << "<Invalid>";
        }
        return debug;
    }

    codes::eeems::oxide1::General* getAPI(){
        if(api_general == nullptr){
            connect();
        }
        return api_general;
    }

    QString requestAPI(std::string name){
        connect();
        if(api_general == nullptr){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to request " << name.c_str() << " api: Unable to connect to API");
            return "/";
        }
        auto reply = api_general->requestAPI(QString::fromStdString(name));
        if(reply.isError()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to request " << name.c_str() << " api:" << reply.error());
            return "/";
        }
        QDBusObjectPath path = reply;
        if(path.path() == "/"){
            O_WARNING(__PRETTY_FUNCTION__ << "API " << name.c_str() << " request denied, or unavailable");
        }
        return path.path();
    }

    void releaseAPI(std::string name){
        if(verifyConnection()){
            api_general->releaseAPI(QString::fromStdString(name));
        }
    }

    void connect(){
        if(verifyConnection()){
            return;
        }
        O_DEBUG(__PRETTY_FUNCTION__ << "Connecting to tarnish over DBus...");
        auto bus = QDBusConnection::systemBus();
        auto interface = bus.interface();
        if(interface == nullptr){
            O_DEBUG(__PRETTY_FUNCTION__ << "DBus interface not available!");
            return;
        }
        if(!interface->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            O_DEBUG(__PRETTY_FUNCTION__ << "Waiting for tarnish DBus service to start up...");
            while(!interface->registeredServiceNames().value().contains(OXIDE_SERVICE)){
                timespec args{
                    .tv_sec = 1,
                    .tv_nsec = 0
                };
                nanosleep(&args, NULL);
            }
        }
        O_DEBUG(__PRETTY_FUNCTION__ << "Tarnish DBus service is online, connecting...");
        api_general = new codes::eeems::oxide1::General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus, qApp);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(api_general, &codes::eeems::oxide1::General::aboutToQuit, [conn]{
            O_WARNING(__PRETTY_FUNCTION__ << "Tarnish has indicated that it is about to quit!");
            if(!childSocket.isOpen()){
                if(!QCoreApplication::startingUp()){
                    QEventLoop loop;
                    QTimer::singleShot(0, [&loop]{
                        _disconnect();
                        loop.quit();
                    });
                    loop.exec();
                }else{
                    _disconnect();
                }
            }
            QObject::disconnect(*conn);
            delete conn;
        });
        if(qApp != nullptr){
            O_DEBUG(__PRETTY_FUNCTION__ << "Connecting to QGuiApplication::aboutToQuit");
            QObject::connect(qApp, &QCoreApplication::aboutToQuit, []{
                if(!QCoreApplication::startingUp()){
                    QEventLoop loop;
                    QTimer::singleShot(0, [&loop]{
                        _disconnect();
                        loop.quit();
                    });
                    loop.exec();
                }else{
                    _disconnect();
                }
            });
        }
        O_DEBUG(__PRETTY_FUNCTION__ << "Connected to tarnish Dbus service");
    }

    void disconnect(){
        O_DEBUG(__PRETTY_FUNCTION__ << "Disconnecting from tarnish");
        if(window != nullptr){
            window->close();
            window->deleteLater();
            window = nullptr;
        }
#define freeAPI(name) \
        if(api_##name != nullptr){ \
            if(strcmp(#name, "general") == 0){ \
                releaseAPI(#name);\
            } \
            delete api_##name; \
            api_##name = nullptr; \
        }
        freeAPI(power);
        freeAPI(wifi);
        freeAPI(screen);
        freeAPI(apps);
        freeAPI(system);
        freeAPI(notification);
        freeAPI(general);
        freeAPI(gui);
#undef freeAPI
        QDBusConnection::disconnectFromBus(QDBusConnection::systemBus().name());
        _disconnect();
    }

    int getSocketFd(){
        if(childSocket.isOpen()){
            return childSocket.socketDescriptor();
        }
        auto socket = getSocket();
        return socket != nullptr ? socket->socketDescriptor() : -1;
    }

    QLocalSocket* getSocket(){
        if(childSocket.isOpen()){
            return &childSocket;
        }
        if(getAPI() == nullptr){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to register child: Unable to get general API");
            return nullptr;
        }
        O_DEBUG(__PRETTY_FUNCTION__ << "Connecting to tarnish command socket");
        auto reply = api_general->registerChild();
        if(reply.isError()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to register child: " << reply.error().message());
            return nullptr;
        }
        auto qfd = reply.value();
        if(!qfd.isValid()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to register child: Invalid fd provided");
            return nullptr;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get child socket: No pipe provided");
            return nullptr;
        }
        fd = dup(fd);
        if(!childSocket.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadWrite | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get child socket:" << childSocket.errorString());
            return nullptr;
        }
        auto closeConn = new QMetaObject::Connection;
        auto readConn = new QMetaObject::Connection;
        *closeConn = QObject::connect(&childSocket, &QLocalSocket::readChannelFinished, [closeConn, readConn]{
            O_WARNING(__PRETTY_FUNCTION__ << "Lost connection to tarnish socket!");
            if(!QCoreApplication::startingUp()){
                QEventLoop loop;
                QTimer::singleShot(0, [&loop]{
                    _disconnect();
                    loop.quit();
                });
                loop.exec();
            }else{
                _disconnect();
            }
            QObject::disconnect(*closeConn);
            QObject::disconnect(*readConn);
            delete closeConn;
            delete readConn;
        });
        *readConn = QObject::connect(&childSocket, &QLocalSocket::readyRead, [readConn]{
            // TODO - implement actually communication protocol
            while(!childSocket.atEnd()){
                O_DEBUG(childSocket.readAll());
            }
        });
        O_DEBUG(__PRETTY_FUNCTION__ << "Connected to tarnish command socket");
        qRegisterMetaType<QAbstractSocket::SocketState>();
        return &childSocket;
    }

    int tarnishPid(){
        if(getAPI() == nullptr){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get tarnish PID: Unable to get general API");
            return -1;
        }
        return api_general->tarnishPid();
    }

    int getFrameBufferFd(QImage::Format format){
        if(fbFile.isOpen()){
            if(fbFormat != format){
                O_WARNING(__PRETTY_FUNCTION__ << "Framebuffer format requested does not match the current format" << format << fbFormat);
                return -1;
            }
            return fbFile.handle();
        }
        QMutexLocker locker(&fbMutex);
        Q_UNUSED(locker);
        if(guiApi() == nullptr){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer: Unable to get GUI API");
            return -1;
        }
        if(getSocket() == nullptr){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer: Unable get command socket");
            return -1;
        }
        if(window == nullptr){
            O_DEBUG(__PRETTY_FUNCTION__ << "Creating with with" << format << "image format");
            QDBusPendingReply<QDBusObjectPath> reply = api_gui->createWindow(format);
            reply.waitForFinished();
            if(reply.isError()){
                O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer:" << reply.error());
                return -1;
            }
            auto path = reply.value().path();
            if(path == "/"){
                O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer: Unable to create window");
                return -1;
            }
            window = new codes::eeems::oxide1::Window(OXIDE_SERVICE, path, api_gui->connection(), qApp);
            QObject::connect(window, &codes::eeems::oxide1::Window::frameBufferChanged, [=](const QDBusUnixFileDescriptor& fd){
                O_DEBUG(__PRETTY_FUNCTION__ << "frameBufferChanged");
                fbFile.close();
                fbGeometry.adjust(0, 0, 0, 0);
                fbLineSize = 0;
                fbFormat = QImage::Format_Invalid;
                fbData = nullptr;
                if(!fd.isValid()){
                    O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer: Invalid DBus response");
                    return;
                }
                auto fb = fd.fileDescriptor();
                if(fb == -1){
                    O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer: File descriptor is -1");
                    return;
                }
                auto frameBufferFd = dup(fb);
                if(frameBufferFd == -1){
                    O_WARNING(__PRETTY_FUNCTION__ << "Failed to unmap framebuffer:" << fbFile.errorString());
                    return;
                }
                setupFbFd(frameBufferFd);
            });
            QObject::connect(window, &codes::eeems::oxide1::Window::closed, [=](){
                O_DEBUG(__PRETTY_FUNCTION__ << "Window closed");
                window->deleteLater();
                window = nullptr;
            });
            window->setVisible(true);
            window->raise();
            O_DEBUG(__PRETTY_FUNCTION__ << "Window created woth z sort:" << window->z());
        }
        O_DEBUG(__PRETTY_FUNCTION__ << "Getting framebuffer fd");
        auto qfd = window->frameBuffer();
        O_DEBUG(__PRETTY_FUNCTION__ << "Checking framebuffer fd");
        if(!qfd.isValid()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer: Invalid DBus response");
            return -1;
        }
        int fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer: File descriptor is -1");
            return -1;
        }
        fd = dup(fd);
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to dup framebuffer fd:" << strerror(errno));
            return -1;
        }
        if(!setupFbFd(fd)){
            return -1;
        }
        if(fbFormat != format){
            O_WARNING(__PRETTY_FUNCTION__ << "Framebuffer format requested does not match the current format" << format << fbFormat);
            fbFile.close();
            return -1;
        }
        O_DEBUG(__PRETTY_FUNCTION__ << "Framebuffer fd:" << fd);
        return fbFile.handle();
    }

    uchar* frameBuffer(QImage::Format format){
        if(fbData != nullptr){
            return fbData;
        }
        auto fd = getFrameBufferFd(format);
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer fd");
            return nullptr;
        }
        fbData = fbFile.map(0, fbFile.size());
        if(fbData == nullptr){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer data:" << fbFile.errorString());
            return nullptr;
        }
        return fbData;
    }

    void lockFrameBuffer(){
        // TODO - explore if mprotect/msync work. This might not be doing anything.
        if(!fbFile.isOpen()){
            return;
        }
        while(flock(fbFile.handle(), LOCK_EX | LOCK_NB) == -1){
            if(errno != EWOULDBLOCK && errno != EINTR){
                O_DEBUG(__PRETTY_FUNCTION__ << "Failed to lock framebuffer:" << strerror(errno));
                continue;
            }
            if(!QCoreApplication::startingUp()){
                qApp->processEvents(QEventLoop::AllEvents, 100);
            }else{
                timespec args{
                    .tv_sec = 0,
                    .tv_nsec = 100 * 1000
                };
                nanosleep(&args, NULL);
            }
        }
    }

    void unlockFrameBuffer(){
        // TODO - explore if mprotect/msync work. This might not be doing anything.
        if(!fbFile.isOpen()){
            return;
        }
        while(flock(fbFile.handle(), LOCK_UN | LOCK_NB) == -1){
            if(errno != EWOULDBLOCK && errno != EINTR){
                O_DEBUG(__PRETTY_FUNCTION__ << "Failed to unlock framebuffer:" << strerror(errno));
                continue;
            }
            if(!QCoreApplication::startingUp()){
                qApp->processEvents(QEventLoop::AllEvents, 100);
            }else{
                usleep(100);
            }
        }
    }

    QImage frameBufferImage(QImage::Format format){
        auto data = frameBuffer(format);
        if(data == nullptr){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer image: Could not get buffer");
            return QImage();
        }
        QImage image((uchar*)data, fbGeometry.width(), fbGeometry.height(), fbLineSize, fbFormat);
        if(image.isNull()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get framebuffer image: Image is null");
        }
        return image;
    }

    int getTouchEventPipeFd(){
        if(touchEventSocket.isOpen()){
            return touchEventSocket.socketDescriptor();
        }
        if(window == nullptr && getFrameBufferFd() == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get touch event pipe, no window");
            return -1;
        }
        auto qfd = window->touchEventPipe();
        if(!qfd.isValid()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get touch event pipe: Invalid DBus response");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get touch event pipe: No pipe provided");
            return -1;
        }
        fd = dup(fd);
        if(!touchEventSocket.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get touch event pipe:" << touchEventSocket.errorString());
            return -1;
        }
        return touchEventSocket.socketDescriptor();
    }

    InputEventSocket* getTouchEventPipe(){
        auto fd = getTouchEventPipeFd();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get touch event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        touchEventSocket.moveToThread(&inputThread);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&touchEventSocket, &InputEventSocket::readChannelFinished, [conn]{
            O_WARNING(__PRETTY_FUNCTION__ << "Lost connection to touch event pipe!");
            touchEventSocket.close();
            QObject::disconnect(*conn);
            delete conn;
        });
        return &touchEventSocket;
    }

    int getTabletEventPipeFd(){
        if(tabletEventSocket.isOpen()){
            return tabletEventSocket.socketDescriptor();
        }
        if(window == nullptr && getFrameBufferFd() == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get tablet event pipe, no window");
            return -1;
        }
        auto qfd = window->tabletEventPipe();
        if(!qfd.isValid()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get tablet event pipe: Invalid DBus response");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get tablet event pipe: No pipe provided");
            return -1;
        }
        fd = dup(fd);
        if(!tabletEventSocket.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get tablet event pipe:" << tabletEventSocket.errorString());
            return -1;
        }
        return tabletEventSocket.socketDescriptor();
    }

    InputEventSocket* getTabletEventPipe(){
        auto fd = getTabletEventPipeFd();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        tabletEventSocket.moveToThread(&inputThread);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&tabletEventSocket, &InputEventSocket::readChannelFinished, [conn]{
            O_WARNING(__PRETTY_FUNCTION__ << "Lost connection to tablet event pipe!");
            tabletEventSocket.close();
            QObject::disconnect(*conn);
            delete conn;
        });
        return &tabletEventSocket;
    }

    int getKeyEventPipeFd(){
        if(keyEventSocket.isOpen()){
            return keyEventSocket.socketDescriptor();
        }
        if(window == nullptr && getFrameBufferFd() == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get key event pipe, no window");
            return -1;
        }
        auto qfd = window->keyEventPipe();
        if(!qfd.isValid()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get key event pipe: Invalid DBus response");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get key event pipe: No pipe provided");
            return -1;
        }
        fd = dup(fd);
        if(!keyEventSocket.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get key event pipe:" << keyEventSocket.errorString());
            return -1;
        }
        return keyEventSocket.socketDescriptor();
    }

    InputEventSocket* getKeyEventPipe(){
        auto fd = getKeyEventPipeFd();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get key event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        keyEventSocket.moveToThread(&inputThread);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&keyEventSocket, &InputEventSocket::readChannelFinished, [conn]{
            O_WARNING(__PRETTY_FUNCTION__ << "Lost connection to key event pipe!");
            keyEventSocket.close();
            QObject::disconnect(*conn);
            delete conn;
        });
        return &keyEventSocket;
    }

    int getEventPipeFd(){
        if(eventSocket.isOpen()){
            return eventSocket.socketDescriptor();
        }
        if(window == nullptr && getFrameBufferFd() == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get window event pipe, no window");
            return -1;
        }
        auto qfd = window->eventPipe();
        if(!qfd.isValid()){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get window event pipe: Invalid DBus response");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get window event pipe: No pipe provided");
            return -1;
        }
        fd = dup(fd);
        if(!eventSocket.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadWrite | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get window event pipe:" << eventSocket.errorString());
            return -1;
        }
        return eventSocket.socketDescriptor();
    }

    QLocalSocket* getEventPipe(){
        auto fd = getEventPipeFd();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get window event pipe: Failed to get pipe fd");
            return nullptr;
        }
        if(eventSocket.isOpen()){
            return &eventSocket;
        }
        startInputThread();
        eventSocket.moveToThread(&inputThread);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&eventSocket, &InputEventSocket::readChannelFinished, [conn]{
            O_WARNING(__PRETTY_FUNCTION__ << "Lost connection to window event pipe!");
            eventSocket.close();
            QObject::disconnect(*conn);
            delete conn;
        });
        return &eventSocket;
    }

    void screenUpdate(QRect rect, EPFrameBuffer::WaveformMode waveform, unsigned int marker){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbFile.size(), MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed update screen:" << strerror(errno))
            return;
        }
        if(!eventSocket.isOpen()){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed update screen: event socket not open");
            return;
        }
        if(marker == 0){
            marker = ++repaintMarker;
        }
        WindowEvent event;
        event.type = WindowEventType::Repaint;
        event.repaintData.geometry = rect;
        event.repaintData.waveform = waveform;
        event.repaintData.marker = marker;
        event.toSocket(&eventSocket);
    }

    void screenUpdate(EPFrameBuffer::WaveformMode waveform, unsigned int marker){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbFile.size(), MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed update screen:" << strerror(errno))
            return;
        }
        if(!eventSocket.isOpen()){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed update screen: event socket not open");
            return;
        }
        auto image = frameBufferImage();
        if(image.isNull()){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed update screen: Image is null");
            return;
        }
        if(marker == 0){
            marker = ++repaintMarker;
        }
        WindowEvent event;
        event.type = WindowEventType::Repaint;
        event.repaintData.geometry = image.rect();
        event.repaintData.waveform = waveform;
        event.repaintData.marker = marker;
        event.toSocket(&eventSocket);
    }

    unsigned int requestWaitForUpdate(unsigned int marker){
        if(!eventSocket.isOpen()){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed wait for screen update: event socket not open");
            return 0;
        }
        WindowEvent event;
        event.type = WindowEventType::WaitForPaint;
        event.waitForPaintData.marker = marker == 0 ? repaintMarker.loadRelaxed() : marker;
        event.toSocket(&eventSocket);
        return event.waitForPaintData.marker;
    }

    unsigned int requestWaitForLastUpdate(){ return requestWaitForUpdate(0); }

    codes::eeems::oxide1::Power* powerApi(){
        if(api_power != nullptr){
            return api_power;
        }
        auto path = requestAPI("power");
        if(path == "/"){
            return nullptr;
        }
        api_power = new codes::eeems::oxide1::Power(OXIDE_SERVICE, path, api_general->connection(), (QObject*)qApp);
        return api_power;
    }

    codes::eeems::oxide1::Wifi* wifiApi(){
        if(api_wifi != nullptr){
            return api_wifi;
        }
        auto path = requestAPI("wifi");
        if(path == "/"){
            return nullptr;
        }
        api_wifi = new codes::eeems::oxide1::Wifi(OXIDE_SERVICE, path, api_general->connection(), (QObject*)qApp);
        return api_wifi;
    }

    codes::eeems::oxide1::Screen* screenApi(){
        if(api_screen != nullptr){
            return api_screen;
        }
        auto path = requestAPI("screen");
        if(path == "/"){
            return nullptr;
        }
        api_screen = new codes::eeems::oxide1::Screen(OXIDE_SERVICE, path, api_general->connection(), (QObject*)qApp);
        return api_screen;
    }

    codes::eeems::oxide1::Apps* appsApi(){
        if(api_apps != nullptr){
            return api_apps;
        }
        auto path = requestAPI("apps");
        if(path == "/"){
            return nullptr;
        }
        api_apps = new codes::eeems::oxide1::Apps(OXIDE_SERVICE, path, api_general->connection(), (QObject*)qApp);
        return api_apps;
    }

    codes::eeems::oxide1::System* systemApi(){
        if(api_system != nullptr){
            return api_system;
        }
        auto path = requestAPI("system");
        if(path == "/"){
            return nullptr;
        }
        api_system = new codes::eeems::oxide1::System(OXIDE_SERVICE, path, api_general->connection(), (QObject*)qApp);
        return api_system;
    }

    codes::eeems::oxide1::Notifications* notificationApi(){
        if(api_notification != nullptr){
            return api_notification;
        }
        auto path = requestAPI("notification");
        if(path == "/"){
            return nullptr;
        }
        api_notification = new codes::eeems::oxide1::Notifications(OXIDE_SERVICE, path, api_general->connection(), (QObject*)qApp);
        return api_notification;
    }

    codes::eeems::oxide1::Gui* guiApi(){
        if(api_gui != nullptr){
            return api_gui;
        }
        auto path = requestAPI("gui");
        if(path == "/"){
            return nullptr;
        }
        api_gui = new codes::eeems::oxide1::Gui(OXIDE_SERVICE, path, api_general->connection(), (QObject*)qApp);
        return api_gui;
    }

    codes::eeems::oxide1::Window* topWindow(){ return window; }

}
