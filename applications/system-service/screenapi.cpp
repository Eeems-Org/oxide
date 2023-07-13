#include "screenapi.h"
#include "notificationapi.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <liboxide.h>
#include "mxcfb.h"

#include <QPainter>
#include <QDebug>

using namespace Oxide;

ScreenAPI* ScreenAPI::instance = nullptr;

ScreenAPI::ScreenAPI(QObject* parent) : APIBase(parent), m_screenshots(), m_enabled(false) {
    instance = this;
    Sentry::sentry_transaction("screen", "init", [this](Sentry::Transaction* t){
        qDBusRegisterMetaType<QList<double>>();
        Sentry::sentry_span(t, "mkdirs", "Create screenshots directory", [this]{
            mkdirs("/home/root/screenshots/");
        });
        Sentry::sentry_span(t, "screenshots", "Load existing screenshots", [this]{
            QDir dir("/home/root/screenshots/");
            dir.setNameFilters(QStringList() << "*.png");
            for(auto entry : dir.entryInfoList()){
                addScreenshot(entry.filePath());
            }
        });
    });
}

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

bool ScreenAPI::isEnabled() {
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

bool ScreenAPI::drawFullscreenImage(QString path){
    if(!hasPermission("screen")){
        return false;
    }
    if(!QFile(path).exists()){
        qDebug() << "Can't find image" << path;
        return false;
    }
    QImage img(path);
    if(img.isNull()){
        qDebug() << "Image data invalid" << path;
        return false;
    }
    Sentry::sentry_transaction("screen", "drawFullscrenImage", [img, path](Sentry::Transaction* t){
        Q_UNUSED(t);
        Oxide::dispatchToMainThread([img]{
            QRect rect = EPFrameBuffer::framebuffer()->rect();
            QPainter painter(EPFrameBuffer::framebuffer());
            painter.drawImage(rect, img);
            painter.end();
            EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::FullUpdate, true);
            EPFrameBuffer::waitForLastUpdate();
        });
    });
    return true;
}

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
        EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::Mono, EPFrameBuffer::PartialUpdate, true);
        QDBusObjectPath path("/");
        if(!screen.save(filePath)){
            qDebug() << "Failed to take screenshot";
        }else{
            path = addScreenshot(filePath)->qPath();
        }
        QPainter painter(EPFrameBuffer::framebuffer());
        painter.drawImage(rect, screen, rect);
        painter.end();
        EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::PartialUpdate, true);
        return path;
    });
}

QImage ScreenAPI::copy(){
    return Oxide::dispatchToMainThread<QImage>([]{
        auto frameBuffer = EPFrameBuffer::framebuffer();
        qDebug() << "Waiting for other painting to finish...";
        while(frameBuffer->paintingActive()){
            EPFrameBuffer::waitForLastUpdate();
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
    Sentry::sentry_transaction("screen", "addScreenshot", [this, filePath, &instance](Sentry::Transaction* t){
        Sentry::sentry_span(t, "screenshot", "Create screenshot", [this, filePath, &instance]{
            auto path = QString(OXIDE_SERVICE_PATH "/screenshots/") + QFileInfo(filePath).completeBaseName().remove('-').remove('.');
            instance = new Screenshot(path, filePath, this);
            m_screenshots.append(instance);
        });
        Sentry::sentry_span(t, "events", "Connect and emit events", [this, &instance]{
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

QString ScreenAPI::getNextPath() {
    QString filePath;
    do {
        filePath = "/home/root/screenshots/" + getTimestamp() + ".png";
    } while(QFile::exists(filePath));
    return filePath;
}
