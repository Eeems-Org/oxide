#include "screenapi.h"
#include "notificationapi.h"
#include "systemapi.h"

#include <liboxide/oxideqml.h>
#include <liboxide/epaper.h>

QDBusObjectPath ScreenAPI::screenshot(){
    if(!hasPermission("screen")){
        return QDBusObjectPath("/");
    }
    O_INFO("Taking screenshot");
    auto filePath = getNextPath();
    O_DEBUG("Using path" << filePath);
    auto notification = notificationAPI->add(
        QUuid::createUuid().toString(),
        "codes.eeems.tarnish",
        "codes.eeems.tarnish",
        "Taking Screenshot...",
        ""
    );
    notification->display();
    QImage screen = copy();
    bool saved = false;
    QDBusObjectPath path("/");
    if(screen.size().isEmpty()){
        O_WARNING("Could not get copy of screen");
    }else{
        if(systemAPI->landscape()){
            screen = screen.transformed(QTransform().rotate(270.0));
        }
        saved = screen.save(filePath);
        if(saved){
            path = addScreenshot(filePath)->qPath();
        }else if(!saved){
            O_WARNING("Failed to save screenshot");
        }
    }
    notification->remove();
    notificationAPI->add(
        QUuid::createUuid().toString(),
        "codes.eeems.tarnish",
        "codes.eeems.tarnish",
        saved ? "Screenshot taken" : "Failed to take screenshot",
        saved ? filePath : ""
    )->display();
    return path;
}

QImage ScreenAPI::copy(){
    auto compositor = getCompositorDBus();
    auto reply = compositor->frameBuffer();
    reply.waitForFinished();
    if(reply.isError()){
        O_WARNING("Failed to get framebuffer fd" << reply.error().message());
        return QImage();
    }
    QDBusUnixFileDescriptor qfd = reply.value();
    if(!qfd.isValid()){
        O_WARNING("Framebuffer fd is not valid");
        return QImage();
    }
    QFile file;
    file.open(qfd.fileDescriptor(), QFile::ReadOnly);
    auto data = file.map(0, file.size());
    return QImage(data, 1404, 1872, 2808, QImage::Format_RGB16).copy();
}

QDBusObjectPath ScreenAPI::addScreenshot(QByteArray blob){
    if(!hasPermission("screen")){
        return QDBusObjectPath("/");
    }
    O_INFO("Adding external screenshot");
    mutex.lock();
    auto filePath = getNextPath();
    QFile file(filePath);
    if(!file.open(QIODevice::WriteOnly)){
        O_WARNING("Failed to add screenshot");
        mutex.unlock();
        return QDBusObjectPath("");
    }
    file.write(blob);
    file.close();
    mutex.unlock();
    return addScreenshot(filePath)->qPath();
}

Screenshot* ScreenAPI::addScreenshot(QString filePath){
    Screenshot* instance;
    Oxide::Sentry::sentry_transaction("screen", "addScreenshot", [this, filePath, &instance](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::sentry_span(t, "screenshot", "Create screenshot", [this, filePath, &instance]{
            auto path = QString(OXIDE_SERVICE_PATH "/screenshots/") + QFileInfo(filePath).completeBaseName().remove('-').remove('.');
            instance = new Screenshot(path, filePath, this);
            m_screenshots.append(instance);
        });
        Oxide::Sentry::sentry_span(t, "events", "Connect and emit events", [this, &instance]{
            connect(instance, &Screenshot::removed, [this, instance]{
                if(m_enabled){
                    emit screenshotRemoved(instance->qPath());
                }
                m_screenshots.removeAll(instance);
                delete instance;
            });
            connect(instance, &Screenshot::modified, [this, instance]{
                if(m_enabled){
                    emit screenshotModified(instance->qPath());
                }
            });
            if(m_enabled){
                instance->registerPath();
                emit screenshotAdded(instance->qPath());
            }
        });
    });
    return instance;
}

void ScreenAPI::mkdirs(const QString& path, mode_t mode){
    QDir dir(path);
    if(!dir.exists()){
        QString subpath = "";
        for(auto part : path.split("/")){
            subpath += "/" + part;
            QDir dir(subpath);
            if(!dir.exists()){
                mkdir(subpath.toStdString().c_str(), mode);
            }
        }
    }
}

QString ScreenAPI::getTimestamp(){ return QDateTime::currentDateTime().toString("yyyy-MM-ddThhmmss.zzz"); }

QString ScreenAPI::getNextPath() {
    QString filePath;
    do {
        filePath = "/home/root/screenshots/" + getTimestamp() + ".png";
    } while(QFile::exists(filePath));
    return filePath;
}

ScreenAPI* ScreenAPI::singleton(ScreenAPI* self){
    static ScreenAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}

ScreenAPI::ScreenAPI(QObject* parent) : APIBase(parent), m_screenshots(), m_enabled(false) {
    Oxide::Sentry::sentry_transaction("screen", "init", [this](Oxide::Sentry::Transaction* t){
        qDBusRegisterMetaType<QList<double>>();
        Oxide::Sentry::sentry_span(t, "mkdirs", "Create screenshots directory", [this]{
            mkdirs("/home/root/screenshots/");
        });
        Oxide::Sentry::sentry_span(t, "singleton", "Setup singleton", [this]{
            singleton(this);
        });
        Oxide::Sentry::sentry_span(t, "screenshots", "Load existing screenshots", [this]{
            QDir dir("/home/root/screenshots/");
            dir.setNameFilters(QStringList() << "*.png");
            for(auto entry : dir.entryInfoList()){
                addScreenshot(entry.filePath());
            }
        });
    });
}

ScreenAPI::~ScreenAPI(){}

void ScreenAPI::setEnabled(bool enabled){
    m_enabled = enabled;
    O_INFO("Screen API" << enabled);
    for(auto screenshot : m_screenshots){
        if(enabled){
            screenshot->registerPath();
        }else{
            screenshot->unregisterPath();
        }
    }
}

bool ScreenAPI::enabled() {
    if(!hasPermission("screen")){
        return false;
    }
    return m_enabled;
}

QList<QDBusObjectPath> ScreenAPI::screenshots(){
    QList<QDBusObjectPath> list;
    if(!hasPermission("screen")){
        return list;
    }
    for(auto screenshot : m_screenshots){
        list.append(screenshot->qPath());
    }
    return list;
}
#include "moc_screenapi.cpp"
