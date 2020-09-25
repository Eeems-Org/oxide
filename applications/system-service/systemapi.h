#ifndef SYSTEMAPI_H
#define SYSTEMAPI_H

#include <QObject>
#include <QMutableListIterator>

#include "apibase.h"
#include "buttonhandler.h"
#include "application.h"
#include "stb_image.h"
#include "screenapi.h"
#include "stb_image_write.h"
#include "login1_interface.h"


#define RDISPLAYWIDTH 1408
#define RDISPLAYHEIGHT 1920
#define RDISPLAYSIZE RDISPLAYWIDTH * RDISPLAYHEIGHT * sizeof(remarkable_color)

#define systemAPI SystemAPI::singleton()

typedef org::freedesktop::login1::Manager Manager;

struct Inhibitor {
    int fd;
    QString who;
    QString what;
    QString why;
    bool block;
    Inhibitor(Manager* systemd, QString what, QString who, QString why, bool block = false)
     : who(who), what(what), why(why), block(block) {
        QDBusUnixFileDescriptor reply = systemd->Inhibit("sleep", "tarnish", "handle screen", "delay");
        fd = reply.takeFileDescriptor();
    }
    void release(){
        if(released()){
            return;
        }
        close(fd);
        fd = -1;
    }
    bool released() { return fd == -1; }
};

class SystemAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_SYSTEM_INTERFACE)
public:
    static SystemAPI* singleton(SystemAPI* self = nullptr){
        static SystemAPI* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    SystemAPI(QObject* parent) : APIBase(parent) {
        singleton(this);
        this->resumeApp = nullptr;
        systemd = new Manager("org.freedesktop.login1", "/org/freedesktop/login1", QDBusConnection::systemBus(), this);
        // Handle Systemd signals
        connect(systemd, &Manager::PrepareForSleep, this, &SystemAPI::PrepareForSleep);
        inhibitSleep();
    }
    ~SystemAPI(){
        QMutableListIterator<Inhibitor> i(inhibitors);
        while(i.hasNext()){
            auto inhibitor = i.next();
            inhibitor.release();
            i.remove();
        }
        delete systemd;
    }
    void setEnabled(bool enabled){
        qDebug() << "System API" << enabled;
    }
public slots:
    void suspend(){ systemd->Suspend(false); }
    void powerOff() { systemd->PowerOff(false); }

signals:
    void leftAction();
    void homeAction();
    void rightAction();
    void powerAction();

private slots:
    void PrepareForSleep(bool suspending);

private:
    Manager* systemd;
    QList<Inhibitor> inhibitors;
    Application* resumeApp;

    void inhibitSleep(){
        inhibitors.append(Inhibitor(systemd, "sleep", qApp->applicationName(), "Handle sleep screen"));
    }
    void releaseSleepInhibitors(){
        QMutableListIterator<Inhibitor> i(inhibitors);
        while(i.hasNext()){
            auto inhibitor = i.next();
            if(inhibitor.what == "sleep"){
                inhibitor.release();
            }
            if(inhibitor.released()){
                i.remove();
            }
        }
    }
    void drawSleepImage(){
        screenAPI->drawFullscreenImage("/usr/share/remarkable/suspended.png");
    }
};

#endif // SYSTEMAPI_H
