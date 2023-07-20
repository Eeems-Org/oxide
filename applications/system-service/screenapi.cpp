#include "screenapi.h"
#include "appsapi.h"
#include "notificationapi.h"
#include "window.h"

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
    O_INFO("Screen API" << enabled);
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
        O_INFO("Can't find image" << path);
        return false;
    }
    QImage img(path);
    if(img.isNull()){
        O_INFO("Image data invalid" << path);
        return false;
    }
    Sentry::sentry_transaction("screen", "drawFullscrenImage", [img, path](Sentry::Transaction* t){
        Q_UNUSED(t);
        auto window = AppsAPI::_window();
        auto image = window->toImage();
        QRect rect = image.rect();
        QPainter painter(&image);
        painter.drawImage(rect, img);
        painter.end();
        if(window->_isVisible()){
            window->_repaint(rect, EPFrameBuffer::HighQualityGrayscale, 0, false);
        }else{
            window->_setVisible(true);
        }
    });
    return true;
}

QDBusObjectPath ScreenAPI::screenshot(){
    if(!hasPermission("screen")){
        return QDBusObjectPath("/");
    }
    auto filePath = _screenshot();
    QDBusObjectPath path("/");
    if(filePath.isEmpty()){
        O_INFO("Failed to take screenshot");
    }else{
        path = addScreenshot(filePath)->qPath();
    }
    return path;
}

QString ScreenAPI::_screenshot(){
    auto filePath = getNextPath();
#ifdef DEBUG
    O_INFO("Taking screenshot using path" << filePath);
#endif
    QImage screen = copy();
    notificationAPI->paintNotification("Taking Screenshot...", "");
    if(!screen.save(filePath)){
        O_INFO("Failed to take screenshot");
        return "";
    }
    NotificationAPI::_window()->_setVisible(false);
    return filePath;
}

QImage ScreenAPI::copy(){ return EPFrameBuffer::framebuffer()->copy(); }

QDBusObjectPath ScreenAPI::addScreenshot(QByteArray blob){
    if(!hasPermission("screen")){
        return QDBusObjectPath("/");
    }
    O_INFO("Adding external screenshot");
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
