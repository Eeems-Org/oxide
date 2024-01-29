#include "apibase.h"
#include "appsapi.h"

#include <liboxide/oxideqml.h>

int APIBase::hasPermission(QString permission, const char* sender){
    if(getpgid(getpid()) == getSenderPgid()){
        return true;
    }
    qDebug() << "Checking permission" << permission << "from" << sender;
    for(auto name : appsAPI->runningApplicationsNoSecurityCheck().keys()){
        auto app = appsAPI->getApplication(name);
        if(app == nullptr){
            continue;
        }
        if(app->processId() == getSenderPgid()){
            auto result = app->permissions().contains(permission);
            qDebug() << app->name() << result;
            return result;
        }
    }
    qDebug() << "app not found, permission granted";
    return true;
}

int APIBase::getSenderPid() {
    if (!calledFromDBus()) {
        return getpid();
    }
    return connection().interface()->servicePid(message().service());
}
int APIBase::getSenderPgid() { return getpgid(getSenderPid()); }

#include "moc_apibase.cpp"

QImage* getFrameBuffer(){
    static auto framebuffer = Oxide::QML::getImageForWindow(getFrameBufferWindow());
    return &framebuffer;
}

QWindow* getFrameBufferWindow(){
    static auto window = qApp->focusWindow();
    return window;
}
