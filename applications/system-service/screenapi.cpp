#include "screenapi.h"
#include "notificationapi.h"
#include "systemapi.h"

#include <liboxide/oxideqml.h>
#include <liboxide/epaper.h>

QDBusObjectPath ScreenAPI::screenshot(){
    if(!hasPermission("screen")){
        return QDBusObjectPath("/");
    }
    qDebug() << "Taking screenshot";
    auto filePath = getNextPath();
#ifdef DEBUG
    qDebug() << "Using path" << filePath;
#endif
    return dispatchToMainThread<QDBusObjectPath>([this, filePath]{
        QImage screen = copy();
        QRect rect = notificationAPI->paintNotification("Taking Screenshot...", "");
        Oxide::QML::repaint(getFrameBufferWindow(), rect, Blight::Mono, true);
        QDBusObjectPath path("/");
        bool saved = (
            systemAPI->landscape()
                ? screen.transformed(QTransform().rotate(270.0))
                : screen
        ).save(filePath);
        if(!saved){
            qDebug() << "Failed to take screenshot";
        }else{
            path = addScreenshot(filePath)->qPath();
        }
        auto frameBuffer = getFrameBuffer();
        QPainter painter(frameBuffer);
        painter.drawImage(rect, screen, rect);
        painter.end();
        Oxide::QML::repaint(getFrameBufferWindow(), rect, Blight::HighQualityGrayscale, true);
        notificationAPI->add(
            QUuid::createUuid().toString(),
            "codes.eeems.tarnish",
            "codes.eeems.tarnish",
            saved ? "Screenshot taken" : "Failed to take screenshot",
            saved ? filePath : ""
        )->display();
        return path;
    });
}

QImage ScreenAPI::copy(){
    return Oxide::dispatchToMainThread<QImage>([]{
        auto frameBuffer = getFrameBuffer();
        qDebug() << "Waiting for other painting to finish...";
        while(frameBuffer->paintingActive()){
            // TODO - don't spin lock
        }
        return frameBuffer->copy();
    });
}

QDBusObjectPath ScreenAPI::addScreenshot(QByteArray blob){
    if(!hasPermission("screen")){
        return QDBusObjectPath("/");
    }
    qDebug() << "Adding external screenshot";
    mutex.lock();
    auto filePath = getNextPath();
    QFile file(filePath);
    if(!file.open(QIODevice::WriteOnly)){
        qDebug("Failed to add screenshot");
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
    qDebug() << "Screen API" << enabled;
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

bool ScreenAPI::drawFullscreenImage(QString path, double rotate){
    if (!hasPermission("screen")) {
        return false;
    }
    if (!QFile(path).exists()) {
        qDebug() << "Can't find image" << path;
        return false;
    }
    QImage img(path);
    if (img.isNull()) {
        qDebug() << "Image data invalid" << path;
        return false;
    }
    if(rotate){
        img = img.transformed(QTransform().rotate(rotate));
    }
    Oxide::Sentry::sentry_transaction(
        "screen", "drawFullscrenImage",
        [img, path](Oxide::Sentry::Transaction *t) {
            Q_UNUSED(t);
            Oxide::dispatchToMainThread([img]{
                auto frameBuffer = getFrameBuffer();
                QRect rect = frameBuffer->rect();
                QPainter painter(frameBuffer);
                painter.fillRect(rect, Qt::white);
                painter.setRenderHints(
                    QPainter::Antialiasing | QPainter::SmoothPixmapTransform,
                    1
                    );
                QPixmap pxmap;
                QPoint center(rect.width() / 2, rect.height() / 2);
                painter.translate(center);
                painter.scale(
                    1* (rect.width() / qreal(img.height())),
                    1 * (rect.width() / qreal(img.height()))
                    );
                painter.translate(0 - img.width() / 2, 0 - img.height() / 2);
                painter.drawPixmap(img.rect(), QPixmap::fromImage(img));
                painter.end();
                Oxide::QML::repaint(getFrameBufferWindow(), frameBuffer->rect(), Blight::HighQualityGrayscale, true);
            });
        });
    return true;
}
#include "moc_screenapi.cpp"
