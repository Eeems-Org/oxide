#ifndef APPSAPI_H
#define APPSAPI_H

#include <QDBusMetaType>
#include <QDebug>
#include <QDBusObjectPath>
#include <QSettings>
#include <QUuid>
#include <QFile>
#include <QTimer>

#include "apibase.h"
#include "application.h"
#include "fb2png.h"
#include "signalhandler.h"
#include "stb_image.h"
#include "stb_image_write.h"

#define PNG_PATH "/tmp/fb.png"
#define OXIDE_SETTINGS_VERSION 1
#define remarkable_color uint16_t

class AppsAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_APPS_INTERFACE)
    Q_PROPERTY(int state READ state) // This needs to be here for the XML to generate the other properties :(
    Q_PROPERTY(QDBusObjectPath startupApplication READ startupApplication WRITE setStartupApplication)
    Q_PROPERTY(QVariantMap applications READ getApplications)
    Q_PROPERTY(QDBusObjectPath currentApplication READ currentApplication)
    Q_PROPERTY(QVariantMap runningApplications READ runningApplications)
    Q_PROPERTY(QVariantMap pausedApplications READ pausedApplications)
public:
    AppsAPI(QObject* parent)
    : APIBase(parent),
      m_enabled(false),
      applications(),
      settings(this),
      m_startupApplication("/"),
      signalHandler() {
        setup_unix_signal_handlers();
        qDBusRegisterMetaType<QMap<QString,QDBusObjectPath>>();
        qDBusRegisterMetaType<QDBusObjectPath>();
        settings.sync();
        auto version = settings.value("version", OXIDE_SETTINGS_VERSION).toInt();
        if(version < OXIDE_SETTINGS_VERSION){
            migrate(&settings, version);
        }
        int size = settings.beginReadArray("applications");
        for(int i = 0; i < size; ++i){
            settings.setArrayIndex(i);
            auto name = settings.value("name").toString();
            auto displayName = settings.value("displayname", name).toString();
            auto app = new Application(getPath(name), this);
            app->setConfig(QVariantMap {
                {"name", name},
                {"displayName", displayName},
                {"description", settings.value("description", displayName).toString()},
                {"bin", settings.value("bin").toString()},
                {"type", settings.value("type", Foreground).toInt()},
                {"flags", settings.value("flags", QStringList()).toStringList()},
                {"icon", settings.value("icon", "").toString()},
                {"onPause", settings.value("onPause", "").toString()},
                {"onResume", settings.value("onResume", "").toString()},
                {"onStop", settings.value("onStop", "").toString()},
            });
            applications.insert(name, app);
        }
        settings.endArray();
        if(!applications.size()){
            // TODO load from draft config files
        }
        if(!applications.contains("xochitl")){
            auto app = new Application(getPath("Xochitl"), this);
            app->setConfig(QVariantMap {
                {"name", "xochitl"},
                {"displayName", "Xochitl"},
                {"description", "reMarkable default application"},
                {"bin", "/usr/bin/xochitl"},
                {"type", Foreground},
                {"flags", QStringList() << "system"},
                {"icon", "/opt/etc/draft/icons/xochitl.png"}
            });
            applications[app->name()] = app;
            emit applicationRegistered(app->qPath());
        }
        if(!applications.contains("codes.eeems.erode")){
            auto app = new Application(getPath("Process Manager"), this);
            app->setConfig(QVariantMap {
                {"name", "codes.eeems.erode"},
                {"displayName", "Process Manager"},
                {"description", "List and kill running processes"},
                {"bin", "/opt/bin/erode"},
                {"type", Foreground},
                {"flags", QStringList() << "system"},
            });
            applications[app->name()] = app;
            emit applicationRegistered(app->qPath());
        }
        auto path = QDBusObjectPath(settings.value("startupApplication").toString());
        auto app = getApplication(path);
        if(app == nullptr){
            app = getApplication("codes.eeems.oxide");
            if(app == nullptr){
                app = new Application(getPath("Launcher"), this);
                app->setConfig(QVariantMap {
                    {"name", "codes.eeems.oxide"},
                    {"displayName", "Launcher"},
                    {"description", "Application launcher"},
                    {"bin", "/opt/bin/oxide"},
                    {"type", Foreground},
                    {"flags", QStringList() << "system"},
                });
                applications[app->name()] = app;
                emit applicationRegistered(app->qPath());
            }
            path = app->qPath();
        }
        m_startupApplication = path;
        for(auto app : applications){
            if(app->autoStart()){
                app->launch();
                if(app->type() == Backgroundable){
                    app->pause();
                }
            }
        }
        app->launch();
    }
    ~AppsAPI() {
        writeApplications();
        settings.sync();
        for(auto app : applications){
            switch(app->state()){
                case Application::Paused:
                    app->signal(SIGCONT);
                case Application::InForeground:
                case Application::InBackground:
                    app->signal(SIGTERM);
                break;
            }
        }
        for(auto app : applications){
            app->waitForFinished();
            delete app;
        }
        applications.clear();
    }
    int state() { return 0; } // Ignore this, it's a kludge to get the xml to generate

    enum ApplicationType { Foreground, Background, Backgroundable};
    Q_ENUM(ApplicationType)

    void setEnabled(bool enabled){
        qDebug() << "Apps API" << enabled;
        for(auto app : applications){
            if(enabled){
                app->registerPath();
            }else{
                app->unregisterPath();
            }
        }
    }

    Q_INVOKABLE QDBusObjectPath registerApplication(QVariantMap properties){
        QString name = properties.value("name", "").toString();
        QString bin = properties.value("bin", "").toString();
        int type = properties.value("type", Foreground).toInt();
        if(type < Foreground || type > Background || name.isEmpty() || bin.isEmpty()){
            return QDBusObjectPath("/");
        }
        if(applications.contains(name)){
            return applications[name]->qPath();
        }
        auto path = QDBusObjectPath(getPath(name));
        auto app = new Application(path, this);
        auto displayName = properties.value("displayName", name).toString();
        app->setConfig(properties);
        applications.insert(name, app);
        writeApplications();
        app->registerPath();
        emit applicationRegistered(path);
        return path;
    }
    Q_INVOKABLE bool unregisterApplication(QDBusObjectPath path){
        auto app = getApplication(path);
        if(app == nullptr){
            return true;
        }
        if(app->systemApp()){
            return false;
        }
        unregisterApplication(app);
        return true;
    }

    QDBusObjectPath startupApplication(){
        return m_startupApplication;
    }
    void setStartupApplication(QDBusObjectPath path){
        if(getApplication(path) != nullptr){
            m_startupApplication = path;
            settings.setValue("startupApplication", path.path());
        }
    }

    QVariantMap getApplications(){
        QVariantMap result;
        for(auto app : applications){
            result.insert(app->name(), QVariant::fromValue(app->qPath()));
        }
        return result;
    }

    QDBusObjectPath currentApplication(){
        for(auto app : applications){
            if(app->state() == Application::InForeground){
                return app->qPath();
            }
        }
        return QDBusObjectPath("/");
    }
    QVariantMap runningApplications(){
        QVariantMap result;
        for(auto app : applications){
            auto state = app->state();
            if(state == Application::InForeground || state == Application::InBackground){
                result.insert(app->name(), QVariant::fromValue(app->qPath()));
            }
        }
        return result;
    }
    QVariantMap pausedApplications(){
        QVariantMap result;
        for(auto app : applications){
            auto state = app->state();
            if(state == Application::Paused){
                result.insert(app->name(), QVariant::fromValue(app->qPath()));
            }
        }
        return result;
    }

    void unregisterApplication(Application* app){
        auto name = app->name();
        if(applications.contains(name)){
            applications.remove(name);
            emit applicationUnregistered(app->qPath());
            delete app;
            writeApplications();
        }
    }
    void pauseAll(){
        for(auto app : applications){
            app->pause(false);
        }
    }
    void resumeIfNone(){
        for(auto app : applications){
            if(app->state() == Application::InForeground){
                return;
            }
        }
        getApplication(m_startupApplication)->launch();
    }
    Application* getApplication(QDBusObjectPath path){
        for(auto app : applications){
            if(app->path() == path.path()){
                return app;
            }
        }
        return nullptr;
    }
    Q_INVOKABLE QDBusObjectPath getApplicationPath(QString name){
        auto app = getApplication(name);
        if(app == nullptr){
            return QDBusObjectPath("/");
        }
        return app->qPath();
    }
    Application* getApplication(QString name){
        if(applications.contains(name)){
            return applications[name];
        }
        return nullptr;
    }
    void connectSignals(Application* app, int signal){
        switch(signal){
            case 1:
                connect(&signalHandler, &SignalHandler::sigUsr1, app, &Application::sigUsr1);
            break;
            case 2:
                connect(&signalHandler, &SignalHandler::sigUsr2, app, &Application::sigUsr2);
            break;
        }
    }
    void disconnectSignals(Application* app, int signal){
        switch(signal){
            case 1:
                disconnect(&signalHandler, &SignalHandler::sigUsr1, app, &Application::sigUsr1);
            break;
            case 2:
                disconnect(&signalHandler, &SignalHandler::sigUsr2, app, &Application::sigUsr2);
            break;
        }
    }
signals:
    void applicationRegistered(QDBusObjectPath);
    void applicationLaunched(QDBusObjectPath);
    void applicationUnregistered(QDBusObjectPath);
    void applicationPaused(QDBusObjectPath);
    void applicationResumed(QDBusObjectPath);
    void applicationSignaled(QDBusObjectPath);
    void applicationExited(QDBusObjectPath, int);

public slots:
    void leftHeld(){
        auto currentApplication = getApplication(this->currentApplication());
        if(currentApplication->state() != Application::Inactive && currentApplication->path() == m_startupApplication.path()){
            qDebug() << "Already at startup application";
            return;
        }
        getApplication(m_startupApplication)->launch();
    }
    void homeHeld(){
        auto app = getApplication("codes.eeems.erode");
        if(app == nullptr){
            qDebug() << "Unable to find process manager";
            return;
        }
        if(app->state() == Application::InForeground){
            qDebug() << "Process manager already running";
            return;
        }
        app->launch();
    }
    void rightHeld(){
        takeScreenshot();
        if(QFile("/tmp/.screenshot").exists()){
            // Then execute the contents of /tmp/.terminate
            qDebug() << "Screenshot file exists.";
            system("/bin/bash /tmp/.screenshot");
        }
        qDebug() << "Screenshot done.";
    }
    void powerHeld(){

    }
    void powerPress(){
        qDebug() << "Power button pressed...";
        auto app = getApplication(currentApplication());
        app->pause();
//        int neww, newh, channels;
//        auto decoded = (stbi_uc*)stbi_load("/usr/share/remarkable/suspended.png", &neww, &newh, &channels, 4);
//        int fd = open("/dev/fb0", O_RDWR);
//        int byte_size = DISPLAYWIDTH * DISPLAYHEIGHT * sizeof(remarkable_color);
//        auto ptr = (remarkable_color*)mmap(NULL, byte_size, PROT_WRITE, MAP_SHARED, fd, 0);
//        memcpy(ptr, decoded, DISPLAYSIZE);
//        mxcfb_update_data update_data;
//        mxcfb_rect update_rect;
//        update_rect.top = 0;
//        update_rect.left = 0;
//        update_rect.width = DISPLAYWIDTH;
//        update_rect.height = DISPLAYHEIGHT;
//        update_data.update_marker = 0;
//        update_data.update_region = update_rect;
//        update_data.waveform_mode = WAVEFORM_MODE_AUTO;
//        update_data.update_mode = UPDATE_MODE_FULL;
//        update_data.dither_mode = EPDC_FLAG_USE_DITHERING_MAX;
//        update_data.temp = TEMP_USE_REMARKABLE_DRAW;
//        update_data.flags = 0;
//        ioctl(fd, MXCFB_SEND_UPDATE, &update_data);
//        free(decoded);
//        close(fd);
        QTimer::singleShot(1, [=]{
            qDebug() << "Suspending...";
            system("echo mem > /sys/power/state");
            QTimer::singleShot(1, [=]{
                qDebug() << "Resuming...";
                app->resume();
            });
        });
    }

private:
    bool m_enabled;
    QMap<QString, Application*> applications;
    QSettings settings;
    QDBusObjectPath m_startupApplication;
    SignalHandler signalHandler;
    QString getPath(QString name){
        static const QUuid NS = QUuid::fromString(QLatin1String("{d736a9e1-10a9-4258-9634-4b0fa91189d5}"));
        return QString(OXIDE_SERVICE_PATH) + "/apps/" + QUuid::createUuidV5(NS, name).toString(QUuid::Id128);
    }
    void writeApplications(){
        auto apps = applications.values();
        int size = apps.size();
        settings.beginWriteArray("applications", size);
        for(int i = 0; i < size; ++i){
            settings.setArrayIndex(i);
            auto app = apps[i];
            auto config = app->getConfig();
            for(auto key : config.keys()){
                settings.setValue(key, config[key]);
            }
        }
        settings.endArray();
    }
    static void removeScreenshot(){
        QFile file(PNG_PATH);
        if(file.exists()){
            qDebug() << "Removing framebuffer image";
            file.remove();
        }
    }
    static void takeScreenshot(){
        qDebug() << "Taking screenshot";
        int res = fb2png_defaults();
        if(res){
            qDebug() << "Failed to take screenshot: " << res;
        }
    }
    static void migrate(QSettings* settings, int fromVersion){
        Q_UNUSED(settings)
        Q_UNUSED(fromVersion)
        // In the future migrate changes to settings between versions
    }
};
#endif // APPSAPI_H
