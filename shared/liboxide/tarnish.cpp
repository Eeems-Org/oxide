#include "tarnish.h"
#include "meta.h"
#include "debug.h"

#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/file.h>
#include <QDBusConnection>
#include <QImage>
#include <QThread>

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
InputEventSocket touchEventFile;
InputEventSocket tabletEventFile;
InputEventSocket keyEventFile;

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
    childSocket.close();
    touchEventFile.close();
    tabletEventFile.close();
    keyEventFile.close();
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
                qDebug() << __PRETTY_FUNCTION__ << "Failed to read event" << strerror(errno);
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

    codes::eeems::oxide1::General* getAPI(){ return api_general; }

    QString requestAPI(std::string name){
        connect();
        if(api_general == nullptr){
            qFatal(QString("Unable to request %1 api: Could not connect to general API").arg(name.c_str()).toStdString().c_str());
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
        O_DEBUG(__PRETTY_FUNCTION__ << "Connecting to tarnish");
        auto bus = QDBusConnection::systemBus();
        if(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            O_DEBUG(__PRETTY_FUNCTION__ << "Waiting for tarnish to start up...");
            while(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
                timespec args{
                    .tv_sec = 1,
                    .tv_nsec = 0
                };
                nanosleep(&args, NULL);
            }
        }
        api_general = new codes::eeems::oxide1::General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus, qApp);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(api_general, &codes::eeems::oxide1::General::aboutToQuit, [conn]{
            qDebug() << "Tarnish has indicated that it is about to quit!";
            if(!childSocket.isOpen()){
                _disconnect();
            }
            QObject::disconnect(*conn);
            delete conn;
        });
        if(qApp != nullptr){
            O_DEBUG(__PRETTY_FUNCTION__ << "Connecting to QGuiApplication::aboutToQuit");
            QObject::connect(qApp, &QCoreApplication::aboutToQuit, []{ disconnect(); });
        }
        O_DEBUG(__PRETTY_FUNCTION__ << "Connected to tarnish");
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
        connect();
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
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&childSocket, &QLocalSocket::readChannelFinished, [conn]{
            qDebug() << "Lost connection to tarnish socket!";
            _disconnect();
            QObject::disconnect(*conn);
            delete conn;
        });
        O_DEBUG(__PRETTY_FUNCTION__ << "Connected to tarnish command socket");
        return &childSocket;
    }

    int tarnishPid(){
        connect();
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
        connect();
        if(guiAPI() == nullptr){
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
            QObject::connect(window, &codes::eeems::oxide1::Window::zChanged, [=](int z){
                O_DEBUG(__PRETTY_FUNCTION__ << "Window z sort changed:" << z);
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
        if(touchEventFile.isOpen()){
            return touchEventFile.socketDescriptor();
        }
        connect();
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
        if(!touchEventFile.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get touch event pipe:" << touchEventFile.errorString());
            return -1;
        }
        return touchEventFile.socketDescriptor();
    }

    InputEventSocket* getTouchEventPipe(){
        auto fd = getTouchEventPipeFd();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get touch event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        touchEventFile.moveToThread(&inputThread);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&touchEventFile, &InputEventSocket::readChannelFinished, [conn]{
            qDebug() << "Lost connection to touch event pipe!";
            touchEventFile.close();
            QObject::disconnect(*conn);
            delete conn;
        });
        return &touchEventFile;
    }

    int getTabletEventPipeFd(){
        if(tabletEventFile.isOpen()){
            return tabletEventFile.socketDescriptor();
        }
        connect();
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
        if(!tabletEventFile.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get tablet event pipe:" << tabletEventFile.errorString());
            return -1;
        }
        return tabletEventFile.socketDescriptor();
    }

    InputEventSocket* getTabletEventPipe(){
        auto fd = getTabletEventPipeFd();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        tabletEventFile.moveToThread(&inputThread);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&touchEventFile, &InputEventSocket::readChannelFinished, [conn]{
            qDebug() << "Lost connection to tablet event pipe!";
            touchEventFile.close();
            QObject::disconnect(*conn);
            delete conn;
        });
        return &tabletEventFile;
    }

    int getKeyEventPipeFd(){
        if(keyEventFile.isOpen()){
            return keyEventFile.socketDescriptor();
        }
        connect();
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
        if(!keyEventFile.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get key event pipe:" << keyEventFile.errorString());
            return -1;
        }
        return keyEventFile.socketDescriptor();
    }

    InputEventSocket* getKeyEventPipe(){
        auto fd = getKeyEventPipeFd();
        if(fd == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Unable to get key event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        keyEventFile.moveToThread(&inputThread);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&touchEventFile, &InputEventSocket::readChannelFinished, [conn]{
            qDebug() << "Lost connection to key event pipe!";
            touchEventFile.close();
            QObject::disconnect(*conn);
            delete conn;
        });
        return &keyEventFile;
    }


    void screenUpdate(QRect rect, EPFrameBuffer::WaveformMode waveform){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbFile.size(), MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed to sync:" << strerror(errno))
            return;
        }
        window->repaint(rect, waveform);
    }

    void screenUpdate(EPFrameBuffer::WaveformMode waveform){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbFile.size(), MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING(__PRETTY_FUNCTION__ << "Failed to sync:" << strerror(errno))
            return;
        }
        window->repaint(waveform);
    }

    void waitForLastUpdate(){
        if(fbData == nullptr){
            return;
        }
        window->waitForLastUpdate();
    }

    codes::eeems::oxide1::Power* powerAPI(){
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

    codes::eeems::oxide1::Wifi* wifiAPI(){
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

    codes::eeems::oxide1::Screen* screenAPI(){
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

    codes::eeems::oxide1::Apps* appsAPI(){
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

    codes::eeems::oxide1::System* systemAPI(){
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

    codes::eeems::oxide1::Notifications* notificationAPI(){
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

    codes::eeems::oxide1::Gui* guiAPI(){
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
