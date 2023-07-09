#include "tarnish.h"
#include "meta.h"
#include "debug.h"

#include <sys/mman.h>
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
QThread inputThread;
InputEventSocket touchEventFile;
InputEventSocket tabletEventFile;
InputEventSocket keyEventFile;

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
        O_WARNING("Unable to open framebuffer QFile:" << fbFile.errorString());
        if(::close(fd) == -1){
            O_WARNING("Unable close fd:" << strerror(errno));
        }
        return false;
    }
    fbGeometry = window->geometry();
    fbLineSize = window->bytesPerLine();
    fbFormat = (QImage::Format)window->format();
    O_DEBUG("FrameBuffer" << fbGeometry << fbLineSize << fbFormat << fbFile.size() - sizeof(std::mutex));
    return true;
}

namespace Oxide::Tarnish {
    InputEventSocket::InputEventSocket() : QLocalSocket(), stream(this) {
        qRegisterMetaType<QAbstractSocket::SocketState>();
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
#ifdef DEBUG_EVENTS
            qDebug() << __PRETTY_FUNCTION__ << event.time.tv_sec << event.time.tv_usec << event.type << event.code << event.value;
#endif
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
            O_WARNING("Unable to request " << name.c_str() << " api:" << reply.error());
            return "/";
        }
        QDBusObjectPath path = reply;
        if(path.path() == "/"){
            O_WARNING("API " << name.c_str() << " request denied, or unavailable");
        }
        return path.path();
    }

    void releaseAPI(std::string name){
        if(!verifyConnection()){
            return;
        }
        api_general->releaseAPI(QString::fromStdString(name));
    }

    void connect(){
        if(verifyConnection()){
            return;
        }
        auto bus = QDBusConnection::systemBus();
        O_DEBUG("Waiting for tarnish to start up...");
        while(!bus.interface()->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            timespec args{
                .tv_sec = 1,
                .tv_nsec = 0,
            }, res;
            nanosleep(&args, &res);
        }
        api_general = new codes::eeems::oxide1::General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus, qApp);
    }

    void disconnect(){
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
        fbData = nullptr;
        fbFile.close();
        touchEventFile.close();
        tabletEventFile.close();
        keyEventFile.close();
        if(window != nullptr){
            window->close();
            delete window;
            window = nullptr;
        }
        freeAPI(gui);
        QDBusConnection::disconnectFromBus(QDBusConnection::systemBus().name());
        if(inputThread.isRunning()){
            inputThread.quit();
            inputThread.requestInterruption();
            inputThread.wait();
        }
#undef freeAPI
    }

    void registerChild(){
        registerChild(qApp->applicationName().toStdString());
    }

    void registerChild(std::string name){
        connect();
        api_general->registerChild(getpid(), QString::fromStdString(name), QDBusUnixFileDescriptor(fileno(stdin)), QDBusUnixFileDescriptor(fileno(stdout)));
    }

    int tarnishPid(){
        connect();
        return api_general->tarnishPid();
    }

    int getFrameBufferFd(){
        if(fbFile.isOpen()){
            return fbFile.handle();
        }
        connect();
        if(api_gui == nullptr){
            guiAPI();
        }
        if(api_gui == nullptr){
            O_WARNING("Unable to get framebuffer: Unable to get GUI API");
            return -1;
        }
        if(window == nullptr){
            QDBusPendingReply<QDBusObjectPath> reply = api_gui->createWindow();
            reply.waitForFinished();
            if(reply.isError()){
                O_WARNING("Unable to get framebuffer:" << reply.error());
                return -1;
            }
            auto path = reply.value().path();
            if(path == "/"){
                O_WARNING("Unable to get framebuffer: Unable to create window");
                return -1;
            }
            window = new codes::eeems::oxide1::Window(OXIDE_SERVICE, path, api_gui->connection(), qApp);
            QObject::connect(window, &codes::eeems::oxide1::Window::frameBufferChanged, [=](const QDBusUnixFileDescriptor& fd){
                O_DEBUG("frameBufferChanged");
                fbFile.close();
                fbGeometry.adjust(0, 0, 0, 0);
                fbLineSize = 0;
                fbFormat = QImage::Format_Invalid;
                fbData = nullptr;
                if(!fd.isValid()){
                    O_WARNING("Unable to get framebuffer: Invalid DBus response");
                    return;
                }
                auto fb = fd.fileDescriptor();
                if(fb == -1){
                    O_WARNING("Unable to get framebuffer: File descriptor is -1");
                    return;
                }
                auto frameBufferFd = dup(fb);
                if(frameBufferFd == -1){
                    O_WARNING("Failed to unmap framebuffer:" << fbFile.errorString());
                    return;
                }
                setupFbFd(frameBufferFd);
            });
            QObject::connect(window, &codes::eeems::oxide1::Window::zChanged, [=](int z){
                O_DEBUG("Z sort changed:" << z);
            });
            window->setVisible(true);
            window->raise();
            O_DEBUG("Z sort:" << window->z());
        }
        auto qfd = window->frameBuffer();
        if(!qfd.isValid()){
            O_WARNING("Unable to get framebuffer: Invalid DBus response");
            return -1;
        }
        int fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get framebuffer: File descriptor is -1");
            return -1;
        }
        fd = dup(fd);
        if(fd == -1){
            O_WARNING("Unable to dup framebuffer fd:" << strerror(errno));
            return -1;
        }
        if(!setupFbFd(fd)){
            return -1;
        }
        return fbFile.handle();
    }

    uchar* frameBuffer(){
        if(fbData != nullptr){
            return fbData;
        }
        auto fd = getFrameBufferFd();
        if(fd == -1){
            O_WARNING("Unable to get framebuffer fd");
            return nullptr;
        }
        fbData = fbFile.map(0, fbFile.size());
        if(fbData == nullptr){
            O_WARNING("Unable to get framebuffer data:" << fbFile.errorString());
            return nullptr;
        }
        return fbData;
    }

    std::unique_lock<std::mutex>* frameBufferMutex(){
        auto ptr = frameBuffer();
        if(ptr == nullptr){
            O_WARNING("Unable to get framebuffer mutex: No framebuffer shared data");
            return nullptr;
        }
        return (std::unique_lock<std::mutex>*)(ptr + fbFile.size() - sizeof(std::unique_lock<std::mutex>));
    }

    QImage frameBufferImage(){
        auto data = frameBuffer();
        if(data == nullptr){
            O_WARNING("Unable to get framebuffer image: Could not get buffer");
            return QImage();
        }
        QImage image((uchar*)data, fbGeometry.width(), fbGeometry.height(), fbLineSize, fbFormat);
        if(image.isNull()){
            O_WARNING("Unable to get framebuffer image: Image is null");
        }
        return image;
    }

    int getTouchEventPipeFd(){
        if(touchEventFile.isOpen()){
            return touchEventFile.socketDescriptor();
        }
        connect();
        if(window == nullptr && getFrameBufferFd() == -1){
            O_WARNING("Unable to get touch event pipe, no window");
            return -1;
        }
        auto qfd = window->touchEventPipe();
        if(!qfd.isValid()){
            O_WARNING("Unable to get touch event pipe: Invalid DBus response");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get touch event pipe: No pipe provided");
            return -1;
        }
        fd = dup(fd);
        if(!touchEventFile.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING("Unable to get touch event pipe:" << touchEventFile.errorString());
            return -1;
        }
        return touchEventFile.socketDescriptor();
    }

    InputEventSocket* getTouchEventPipe(){
        auto fd = getTouchEventPipeFd();
        if(fd == -1){
            O_WARNING("Unable to get touch event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        touchEventFile.moveToThread(&inputThread);
        return &touchEventFile;
    }

    int getTabletEventPipeFd(){
        if(tabletEventFile.isOpen()){
            return tabletEventFile.socketDescriptor();
        }
        connect();
        if(window == nullptr && getFrameBufferFd() == -1){
            O_WARNING("Unable to get tablet event pipe, no window");
            return -1;
        }
        auto qfd = window->tabletEventPipe();
        if(!qfd.isValid()){
            O_WARNING("Unable to get tablet event pipe: Invalid DBus response");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get tablet event pipe: No pipe provided");
            return -1;
        }
        fd = dup(fd);
        if(!tabletEventFile.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING("Unable to get tablet event pipe:" << tabletEventFile.errorString());
            return -1;
        }
        return tabletEventFile.socketDescriptor();
    }

    InputEventSocket* getTabletEventPipe(){
        auto fd = getTabletEventPipeFd();
        if(fd == -1){
            O_WARNING("Unable to get event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        tabletEventFile.moveToThread(&inputThread);
        return &tabletEventFile;
    }

    int getKeyEventPipeFd(){
        if(keyEventFile.isOpen()){
            return keyEventFile.socketDescriptor();
        }
        connect();
        if(window == nullptr && getFrameBufferFd() == -1){
            O_WARNING("Unable to get key event pipe, no window");
            return -1;
        }
        auto qfd = window->keyEventPipe();
        if(!qfd.isValid()){
            O_WARNING("Unable to get key event pipe: Invalid DBus response");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get key event pipe: No pipe provided");
            return -1;
        }
        fd = dup(fd);
        if(!keyEventFile.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadOnly | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING("Unable to get key event pipe:" << keyEventFile.errorString());
            return -1;
        }
        return keyEventFile.socketDescriptor();
    }

    InputEventSocket* getKeyEventPipe(){
        auto fd = getKeyEventPipeFd();
        if(fd == -1){
            O_WARNING("Unable to get key event pipe: Failed to get pipe fd");
            return nullptr;
        }
        startInputThread();
        keyEventFile.moveToThread(&inputThread);
        return &keyEventFile;
    }

    bool connectQtEvents(
        std::function<void(const input_event&)> touchCallback,
        std::function<void(const input_event&)> tabletCallback,
        std::function<void(const input_event&)> keyCallback
    ){
        bool success = true;
        if(touchCallback != nullptr || touchEventFile.isOpen()){
            auto touchSocket = (InputEventSocket*)getTouchEventPipe();
            if(touchSocket != nullptr){
                touchSocket->setCallback(touchCallback);
            }else{
                success = false;
            }
        }
        if(tabletCallback != nullptr || tabletEventFile.isOpen()){
            auto tabletSocket = (InputEventSocket*)getTabletEventPipe();
            if(tabletSocket != nullptr){
                tabletSocket->setCallback(tabletCallback);
            }else{
                success = false;
            }
        }
        if(keyCallback != nullptr || keyEventFile.isOpen()){
            auto keySocket = (InputEventSocket*)getKeyEventPipe();
            if(keySocket != nullptr){
                keySocket->setCallback(keyCallback);
            }else{
                success = false;
            }
        }
        return success;
    }

    void screenUpdate(){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbFile.size(), MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING("Failed to sync:" << strerror(errno))
            return;
        }
        window->repaint();
    }

    void screenUpdate(QRect rect){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbFile.size(), MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING("Failed to sync:" << strerror(errno))
            return;
        }
        window->repaint(rect);
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
