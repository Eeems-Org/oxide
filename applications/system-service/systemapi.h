#ifndef SYSTEMAPI_H
#define SYSTEMAPI_H

#include <QObject>
#include <QMutableListIterator>
#include <QTimer>

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
    Q_PROPERTY(int autoSleep READ autoSleep WRITE setAutoSleep)
    Q_PROPERTY(bool sleepInhibited READ sleepInhibited)
    Q_PROPERTY(bool powerOffInhibited READ powerOffInhibited)
public:
    static SystemAPI* singleton(SystemAPI* self = nullptr){
        static SystemAPI* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    SystemAPI(QObject* parent)
     : APIBase(parent),
       suspendTimer(this),
       settings(this),
       sleepInhibitors(),
       powerOffInhibitors() {
        settings.sync();
        singleton(this);
        this->resumeApp = nullptr;
        systemd = new Manager("org.freedesktop.login1", "/org/freedesktop/login1", QDBusConnection::systemBus(), this);
        // Handle Systemd signals
        connect(systemd, &Manager::PrepareForSleep, this, &SystemAPI::PrepareForSleep);
        connect(&suspendTimer, &QTimer::timeout, this, &SystemAPI::suspend);
        suspendTimer.setInterval(settings.value("autoSleep", 1000 * 60).toInt());
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
    int autoSleep(){ return suspendTimer.interval(); }
    void setAutoSleep(int autoSleep){
        if(autoSleep < 1000 * 60){
            return;
        }
        suspendTimer.setInterval(autoSleep);
        settings.setValue("autoSleep", autoSleep);
        settings.sync();
    }
    bool sleepInhibited(){ return sleepInhibitors.length(); }
    bool powerOffInhibited(){ return powerOffInhibitors.length(); }
    void uninhibitAll(QString name){
        powerOffInhibitors.removeAll(name);
        sleepInhibitors.removeAll(name);
        if(!sleepInhibited()){
            suspendTimer.start();
        }
    }
public slots:
    void suspend(){
        qDebug() << "Automatic suspend due to inactivity...";
        if(!sleepInhibited()){
            systemd->Suspend(false);
        }
    }
    void powerOff() {
        if(!powerOffInhibited()){
            systemd->PowerOff(false);
        }
    }
    void activity(){
        suspendTimer.stop();
        suspendTimer.start();
    }
    void inhibitSleep(QDBusMessage message){
        suspendTimer.stop();
        sleepInhibitors.append(message.service());
    }
    void uninhibitSleep(QDBusMessage message) {
        sleepInhibitors.removeAll(message.service());
        if(!sleepInhibited()){
            suspendTimer.start();
        }
    }
    void inhibitPowerOff(QDBusMessage message){ powerOffInhibitors.append(message.service()); }
    void uninhibitPowerOff(QDBusMessage message) { powerOffInhibitors.removeAll(message.service()); }
    void stopSuspendTimer(){ suspendTimer.stop(); }
    void startSuspendTimer(){ suspendTimer.start(); }

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
    QTimer suspendTimer;
    QSettings settings;
    QStringList sleepInhibitors;
    QStringList powerOffInhibitors;

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
