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
int frameBufferFd = -1;
int eventsFd = -1;
uchar* fbData = nullptr;
QImage* fbImage = nullptr;
QList<qint64> fbInfo;

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
            munmap(fbData, fbInfo.at(0) * fbInfo.at(1));
            fbData = nullptr;
        }
        fbInfo.clear();
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
        QDBusPendingReply<QDBusUnixFileDescriptor> reply = api_general->getFrameBuffer();
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
        frameBufferFd = dup(fd);
        return frameBufferFd;
    }
    int createFrameBuffer(int width, int height){
        if(frameBufferFd != -1){
            O_WARNING("Framebuffer already exists");
            return -1;
        }
        connect();
        if(api_general->hasFrameBuffer()){
            O_WARNING("Framebuffer already exists");
            return -1;
        }
        QDBusPendingReply<QDBusUnixFileDescriptor> reply = api_general->createFrameBuffer(width, height);
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
        frameBufferFd =  dup(fd);
        api_general->enableFrameBuffer();
        return frameBufferFd;
    }
    QList<qlonglong> frameBufferInfo(){
        if(getFrameBufferFd() == -1 || !fbInfo.isEmpty()){
            return fbInfo;
        }
        connect();
        QDBusPendingReply<QList<qlonglong>> reply = api_general->getFrameBufferInfo();
        if(reply.isError()){
            O_WARNING("Unable to get framebuffer info:" << reply.error());
            return fbInfo;
        }
        auto info = reply.value();
        if(info.contains(-1)){
            O_WARNING("Unable to get framebuffer info: Invalid size returned");
            return fbInfo;
        }
        fbInfo.swap(info);
        return fbInfo;
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
        auto info = frameBufferInfo();
        if(info.isEmpty()){
            O_WARNING("Unable to get framebuffer info");
            return nullptr;
        }
        auto data = mmap(NULL, info[2], PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, fd, 0);
        if(data == MAP_FAILED){
            O_WARNING("Unable to get framebuffer data:" << strerror(errno));
            return nullptr;
        }
        fbData = (uchar*)data;
        return fbData;
    }
    bool lockFrameBuffer(){
        auto data = frameBuffer();
        if(data == nullptr){
            return false;
        }
        auto info = frameBufferInfo();
        if(info.isEmpty()){
            O_WARNING("Unable to get framebuffer info");
            return false;
        }
        return mlock2(data, info[2], MLOCK_ONFAULT) != -1;
    }
    bool unlockFrameBuffer(){
        auto data = frameBuffer();
        if(data == nullptr){
            return false;
        }
        auto info = frameBufferInfo();
        if(info.isEmpty()){
            O_WARNING("Unable to get framebuffer info");
            return false;
        }
        return munlock(data, info[2]) != -1;
    }
    QImage* frameBufferImage(){
        if(fbImage != nullptr){
            return fbImage;
        }
        auto data = frameBuffer();
        if(data == nullptr){
            return nullptr;
        }
        auto info = frameBufferInfo();
        if(info.isEmpty()){
            return nullptr;
        }
        fbImage = new QImage((uchar*)data, info[0], info[1], info[3], (QImage::Format)info[4]);
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
    void screenUpdate(EPFrameBuffer::WaveformMode mode){
        if(fbData == nullptr){
            return;
        }
        auto info = frameBufferInfo();
        if(info.isEmpty()){
            return;
        }
        if(!unlockFrameBuffer()){
            O_WARNING("Failed to unlock framebuffer:" << strerror(errno))
        }
        if(msync(fbData, info[2], MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING("Failed to sync:" << strerror(errno))
            return;
        }
        api_general->screenUpdate(mode);
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
}
