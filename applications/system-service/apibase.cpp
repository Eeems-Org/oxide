#include "apibase.h"

#include <libblight/meta.h>
#include <libblight/types.h>
#include <liboxide/oxideqml.h>

#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QString>
#include <QWindow>

#include "appsapi.h"

int
APIBase::hasPermission(QString permission, const char* sender)
{
    if (getpgid(getpid()) == getSenderPgid()) {
        return true;
    }
    O_INFO("Checking permission" << permission << "from" << sender);
    for (auto name : appsAPI->runningApplicationsNoSecurityCheck().keys()) {
        auto app = appsAPI->getApplication(name);
        if (app == nullptr) {
            continue;
        }
        if (app->processId() == getSenderPgid()) {
            auto result = app->permissions().contains(permission);
            O_INFO(app->name() << result);
            return result;
        }
    }
    O_INFO("app not found, permission granted");
    return true;
}

int
APIBase::getSenderPid()
{
    if (!calledFromDBus()) {
        return getpid();
    }
    return connection().interface()->servicePid(message().service());
}
int
APIBase::getSenderPgid()
{
    return getpgid(getSenderPid());
}

QImage*
getFrameBuffer()
{
    static QImage* image = nullptr;
    static QFile* file = nullptr;
    if (image == nullptr) {
        file = new QFile();
        auto compositor = getCompositorDBus();
        auto reply = compositor->frameBuffer();
        reply.waitForFinished();
        if (reply.isError()) {
            O_WARNING(
                "Failed to get framebuffer fd" << reply.error().message()
            );
            return nullptr;
        }
        QDBusUnixFileDescriptor qfd = reply.value();
        if (!qfd.isValid()) {
            O_WARNING("Framebuffer fd is not valid");
            return nullptr;
        }
        int fd = dup(qfd.fileDescriptor());
        if (fd < 0) {
            O_WARNING("Failed to get framebuffer fd" << std::strerror(errno));
            return nullptr;
        }
        O_DEBUG("Framebuffer fd: " << std::to_string(fd).c_str())
        if (!file->open(fd, QFile::ReadWrite)) {
            O_WARNING("Failed to open framebuffer " << file->errorString());
            ::close(fd);
            delete file;
            file = nullptr;
            return nullptr;
        }
        int stride;
        switch (deviceSettings.getDeviceType()) {
            case Oxide::DeviceSettings::RM1:
                stride = 2816;
                break;
            case Oxide::DeviceSettings::RM2:
            case Oxide::DeviceSettings::RMPP:
            case Oxide::DeviceSettings::RMPPM:
            case Oxide::DeviceSettings::RMPPURE:
                stride = 2808;
                break;
        }
        uchar* data = file->map(0, stride * 1872);
        if (data == nullptr) {
            O_WARNING("Failed to map framebuffer" << file->errorString());
            file->close();
            delete file;
            file = nullptr;
            return nullptr;
        }
        image = new QImage(
            data,
            deviceSettings.getScreenWidth(),
            deviceSettings.getScreenHeight(),
            stride,
            QImage::Format_RGB16
        );
        if (image->isNull()) {
            O_WARNING("Framebuffer is null" << image->size());
            delete image;
            image = nullptr;
            file->unmap(data);
            file->close();
            delete file;
            file = nullptr;
        } else if (image->size().isEmpty()) {
            O_WARNING("Image is empty" << image->size());
            delete image;
            image = nullptr;
            file->unmap(data);
            file->close();
            delete file;
            file = nullptr;
        }
    }
    return image;
}

Compositor*
getCompositorDBus()
{
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

Blight::shared_buf_t
createBuffer(const QRect& rect, unsigned int stride, Blight::Format format)
{
    return Blight::createBuffer(
               rect.x(), rect.y(), rect.width(), rect.height(), stride, format
    )
        .value_or(nullptr);
}

Blight::shared_buf_t
createBuffer()
{
    auto frameBuffer = getFrameBuffer();
    return createBuffer(
        frameBuffer->rect(),
        frameBuffer->bytesPerLine(),
        (Blight::Format)frameBuffer->format()
    );
}

#include "moc_apibase.cpp"

void
addSystemBuffer(Blight::shared_buf_t buffer)
{
    if (buffer != nullptr) {
        Blight::addSurface(buffer);
        auto compositor = getCompositorDBus();
        compositor->setFlags(
            QString("connection/%1/surface/%2")
                .arg(getpid())
                .arg(buffer->surface),
            QStringList() << "system"
        );
    }
}
