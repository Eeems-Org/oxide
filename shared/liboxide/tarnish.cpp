#include "tarnish.h"
#include "meta.h"
#include "debug.h"

#include <sys/mman.h>
#include <QDBusConnection>
#include <QImage>

codes::eeems::oxide1::General* api_general = nullptr;
codes::eeems::oxide1::Power* api_power = nullptr;
codes::eeems::oxide1::Wifi* api_wifi = nullptr;
codes::eeems::oxide1::Screen* api_screen = nullptr;
codes::eeems::oxide1::Apps* api_apps = nullptr;
codes::eeems::oxide1::System* api_system = nullptr;
codes::eeems::oxide1::Notifications* api_notification = nullptr;
codes::eeems::oxide1::Gui* api_gui = nullptr;
codes::eeems::oxide1::Window* window = nullptr;
int frameBufferFd = -1;
int eventsFd = -1;
QRect fbGeometry;
qulonglong fbSize = 0;
qulonglong fbLineSize = 0;
QImage::Format fbFormat = QImage::Format_Invalid;
uchar* fbData = nullptr;
QImage* fbImage = nullptr;

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

namespace Oxide::Tarnish {
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
            api_##name->deleteLater(); \
            api_##name = nullptr; \
        }
        freeAPI(power);
        freeAPI(wifi);
        freeAPI(screen);
        freeAPI(apps);
        freeAPI(system);
        freeAPI(notification);
        freeAPI(general);
        if(fbData != nullptr){
            munmap(fbData, window->sizeInBytes());
            fbData = nullptr;
        }
        if(fbImage != nullptr){
            delete fbImage;
            fbImage = nullptr;
        }
        if(frameBufferFd != -1){
            close(frameBufferFd);
            frameBufferFd = -1;
        }
        if(eventsFd != -1){
            close(eventsFd);
            eventsFd = -1;
        }
        if(window != nullptr){
            window->close();
            window->deleteLater();
            window = nullptr;
        }
        freeAPI(gui);
        QDBusConnection::disconnectFromBus(QDBusConnection::systemBus().name());
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
        if(frameBufferFd != -1){
            return frameBufferFd;
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
                qDebug() << "frameBufferChanged";
                if(fbImage != nullptr){
                    delete fbImage;
                    fbImage = nullptr;
                }
                if(fbData != nullptr){
                    munmap(fbData, fbSize);
                    fbSize = 0;
                    fbGeometry.adjust(0, 0, 0, 0);
                    fbLineSize = 0;
                    fbFormat = QImage::Format_Invalid;
                    fbData = nullptr;
                }
                if(frameBufferFd != -1){
                    ::close(frameBufferFd);
                    frameBufferFd = -1;
                }
                auto fb = fd.fileDescriptor();
                if(fb == -1){
                    return;
                }
                frameBufferFd = dup(fb);
                fbSize = window->sizeInBytes();
                fbGeometry = window->geometry();
                fbLineSize = window->bytesPerLine();
                fbFormat = (QImage::Format)window->format();
            });
        }
        auto qfd = window->frameBuffer();
        if(!qfd.isValid()){
            O_WARNING("Unable to get framebuffer: File descriptor is -1");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get framebuffer: File descriptor is -1");
            return -1;
        }
        fd = dup(fd);
        if(fd == -1){
            O_WARNING("Unable to dup framebuffer fd:" << strerror(errno));
            return -1;
        }
        frameBufferFd = fd;
        fbSize = window->sizeInBytes();
        fbGeometry = window->geometry();
        fbLineSize = window->bytesPerLine();
        fbFormat = (QImage::Format)window->format();
        window->setVisible(true);
        window->raise();
        return frameBufferFd;
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
        auto data = mmap(NULL, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, fd, 0);
        if(data == MAP_FAILED){
            O_WARNING("Unable to get framebuffer data:" << strerror(errno));
            return nullptr;
        }
        fbData = (uchar*)data;
        return fbData;
    }
    QImage* frameBufferImage(){
        if(fbImage != nullptr){
            return fbImage;
        }
        auto data = frameBuffer();
        if(data == nullptr){
            return nullptr;
        }
        fbImage = new QImage((uchar*)data, fbGeometry.width(), fbGeometry.height(), fbLineSize, fbFormat);
        return fbImage;
    }
    int getEventPipe(){
        if(eventsFd != -1){
            return eventsFd;
        }
        connect();
        QDBusPendingReply<QDBusUnixFileDescriptor> reply = api_general->getEventPipe();
        reply.waitForFinished();
        if(reply.isError()){
            O_WARNING("Unable to get framebuffer:" << reply.error());
            return -1;
        }
        auto fd = reply.value().fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get framebuffer: No framebuffer provided");
            return -1;
        }
        eventsFd = dup(fd);
        api_general->enableEventPipe();
        return eventsFd;
    }
    void screenUpdate(){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbSize, MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING("Failed to sync:" << strerror(errno))
            return;
        }
        window->repaint();
    }
    void screenUpdate(QRect rect){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbSize, MS_SYNC | MS_INVALIDATE) == -1){
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
