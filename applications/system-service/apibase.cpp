#include "apibase.h"
#include "appsapi.h"

#include <QWindow>
#include <liboxide/oxideqml.h>
#include <libblight/meta.h>

int APIBase::hasPermission(QString permission, const char* sender){
    if(getpgid(getpid()) == getSenderPgid()){
        return true;
    }
    O_DEBUG("Checking permission" << permission << "from" << sender);
    for(auto name : appsAPI->runningApplicationsNoSecurityCheck().keys()){
        auto app = appsAPI->getApplication(name);
        if(app == nullptr){
            continue;
        }
        if(app->processId() == getSenderPgid()){
            auto result = app->permissions().contains(permission);
            O_DEBUG(app->name() << result);
            return result;
        }
    }
    O_DEBUG("app not found, permission granted");
    return true;
}

int APIBase::getSenderPid() {
    if (!calledFromDBus()) {
        return getpid();
    }
    return connection().interface()->servicePid(message().service());
}
int APIBase::getSenderPgid() { return getpgid(getSenderPid()); }

QWindow* getFrameBufferWindow(){
    static auto window = qApp->focusWindow();
    return window;
}

QImage getFrameBuffer(){
    static auto frameBuffer = Oxide::QML::getImageForWindow(getFrameBufferWindow());
    return frameBuffer;
}

Compositor* getCompositorDBus(){
    static auto compositor = new Compositor(
        BLIGHT_SERVICE,
        "/",
#ifdef EPAPER
        QDBusConnection::systemBus(),
#else
        QDBusConnection::sessionBus(),
#endif
        qApp
    );
    return compositor;
}

#include "moc_apibase.cpp"
