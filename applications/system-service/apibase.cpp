#include "apibase.h"
#include "appsapi.h"

#include <QWindow>
#include <liboxide/oxideqml.h>
#include <libblight/meta.h>
#include <libblight/types.h>

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

QImage* getFrameBuffer(){
    static QImage* image = nullptr;
    if(image == nullptr){
        auto compositor = getCompositorDBus();
        auto reply = compositor->frameBuffer();
        reply.waitForFinished();
        if(reply.isError()){
            O_WARNING("Failed to get framebuffer fd" << reply.error().message());
            return nullptr;
        }
        QDBusUnixFileDescriptor qfd = reply.value();
        if(!qfd.isValid()){
            O_WARNING("Framebuffer fd is not valid");
            return nullptr;
        }
        QFile file;
        file.open(dup(qfd.fileDescriptor()), QFile::ReadOnly);
        auto data = file.map(0, file.size());
        image = new QImage(data, 1404, 1872, 2808, QImage::Format_RGB16);
    }
    return image;
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

Blight::shared_buf_t createBuffer(const QRect& rect, unsigned int stride, Blight::Format format){
    return Blight::createBuffer(
        rect.x(),
        rect.y(),
        rect.width(),
        rect.height(),
        stride,
        format
    ).value_or(nullptr);
}

Blight::shared_buf_t createBuffer(){
    auto frameBuffer = getFrameBuffer();
    return createBuffer(
        frameBuffer->rect(),
        frameBuffer->bytesPerLine(),
        (Blight::Format)frameBuffer->format()
    );
}

#include "moc_apibase.cpp"

void addSystemBuffer(Blight::shared_buf_t buffer){
    if(buffer != nullptr){
        Blight::addSurface(buffer);
        auto compositor = getCompositorDBus();
        compositor->setFlags(
            QString("connection/%1/surface/%2").arg(getpid()).arg(buffer->surface),
            QStringList() << "system"
        );
    }
}
