#ifndef SCREENSHOTAPI_H
#define SCREENSHOTAPI_H

#include <QObject>
#include <QDebug>
#include <QFile>
#include <QPainter>
#include <QDBusObjectPath>
#include <QMutex>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <epframebuffer.h>
#include <liboxide.h>

#include "apibase.h"
#include "mxcfb.h"
#include "screenshot.h"

#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872
#define TEMP_USE_REMARKABLE_DRAW 0x0018
#define remarkable_color uint16_t
#define DISPLAYSIZE DISPLAYWIDTH * DISPLAYHEIGHT * sizeof(remarkable_color)

#define screenAPI ScreenAPI::singleton()

class ScreenAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_SCREEN_INTERFACE)
    Q_PROPERTY(bool enabled READ enabled)
    Q_PROPERTY(QList<QDBusObjectPath> screenshots READ screenshots)

public:
    static ScreenAPI* singleton(ScreenAPI* self = nullptr){
        static ScreenAPI* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    ScreenAPI(QObject* parent) : APIBase(parent), m_screenshots(), m_enabled(false) {
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
    ~ScreenAPI(){}
    void setEnabled(bool enabled){
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
    bool enabled() {
        if(!hasPermission("screen")){
            return false;
        }
        return m_enabled;
    }
    QList<QDBusObjectPath> screenshots(){
        QList<QDBusObjectPath> list;
        if(!hasPermission("screen")){
            return list;
        }
        for(auto screenshot : m_screenshots){
            list.append(screenshot->qPath());
        }
        return list;
    }

    Q_INVOKABLE bool drawFullscreenImage(QString path, float rotate = 0);

    Q_INVOKABLE QDBusObjectPath screenshot();
    QImage copy(){
        return Oxide::dispatchToMainThread<QImage>([]{
            auto frameBuffer = EPFrameBuffer::framebuffer();
            qDebug() << "Waiting for other painting to finish...";
            while(frameBuffer->paintingActive()){
                EPFrameBuffer::waitForLastUpdate();
            }
            return frameBuffer->copy();
        });
    }

public slots:
    QDBusObjectPath addScreenshot(QByteArray blob){
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

signals:
    void screenshotAdded(QDBusObjectPath);
    void screenshotRemoved(QDBusObjectPath);
    void screenshotModified(QDBusObjectPath);

private:
    QList<Screenshot*> m_screenshots;
    bool m_enabled;
    QMutex mutex;

    Screenshot* addScreenshot(QString filePath){
        Screenshot* instance;
        Oxide::Sentry::sentry_transaction("screen", "addScreenshot", [this, filePath, &instance](Oxide::Sentry::Transaction* t){
            Oxide::Sentry::sentry_span(t, "screenshot", "Create screenshot", [this, filePath, &instance]{
                auto path = QString(OXIDE_SERVICE_PATH "/screenshots/") + QFileInfo(filePath).completeBaseName().remove('-').remove('.');
                instance = new Screenshot(path, filePath, this);
                m_screenshots.append(instance);
            });
            Oxide::Sentry::sentry_span(t, "events", "Connect and emit events", [this, &instance]{
                connect(instance, &Screenshot::removed, [=]{
                    if(m_enabled){
                        emit screenshotRemoved(instance->qPath());
                    }
                    m_screenshots.removeAll(instance);
                    delete instance;
                });
                connect(instance, &Screenshot::modified, [=]{
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
    void mkdirs(const QString& path, mode_t mode = 0700){
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
    QString getTimestamp(){ return QDateTime::currentDateTime().toString("yyyy-MM-ddThhmmss.zzz"); }
    QString getNextPath() {
        QString filePath;
        do {
            filePath = "/home/root/screenshots/" + getTimestamp() + ".png";
        } while(QFile::exists(filePath));
        return filePath;
    }
};

#endif // SCREENSHOTAPI_H
