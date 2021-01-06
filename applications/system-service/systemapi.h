#ifndef SYSTEMAPI_H
#define SYSTEMAPI_H

#include <QObject>
#include <QMutableListIterator>
#include <QTimer>

#include "apibase.h"
#include "buttonhandler.h"
#include "application.h"
#include "screenapi.h"
#include "login1_interface.h"

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
        QDBusUnixFileDescriptor reply = systemd->Inhibit(what, who, why, block ? "block" : "delay");
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
    Q_PROPERTY(int autoSleep READ autoSleep WRITE setAutoSleep NOTIFY autoSleepChanged)
    Q_PROPERTY(bool sleepInhibited READ sleepInhibited NOTIFY sleepInhibitedChanged)
    Q_PROPERTY(bool powerOffInhibited READ powerOffInhibited NOTIFY powerOffInhibitedChanged)
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
        connect(&suspendTimer, &QTimer::timeout, this, &SystemAPI::timeout);

        auto autoSleep = settings.value("autoSleep", 1).toInt();
        m_autoSleep = autoSleep;
        if(autoSleep < 0) {
            m_autoSleep = 0;

        }else if(autoSleep > 10){
            m_autoSleep = 10;
        }
        if(autoSleep != m_autoSleep){
            m_autoSleep = autoSleep;
            settings.setValue("autoSleep", autoSleep);
            settings.sync();
            emit autoSleepChanged(autoSleep);
        }
        qDebug() << "Auto Sleep" << autoSleep;
        if(m_autoSleep){
            suspendTimer.start(m_autoSleep * 60 * 1000);
        }else if(!m_autoSleep){
            suspendTimer.stop();
        }
        // Ask Systemd to tell us nicely when we are about to suspend or resume
        inhibitSleep();
        inhibitPowerOff();
        qDebug() << "System API ready to use";
    }
    ~SystemAPI(){
        qDebug() << "Removing all inhibitors";
        rguard(false);
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
    int autoSleep(){ return m_autoSleep; }
    void setAutoSleep(int autoSleep);
    bool sleepInhibited(){ return sleepInhibitors.length(); }
    bool powerOffInhibited(){ return powerOffInhibitors.length(); }
    void uninhibitAll(QString name);
    void stopSuspendTimer(){
        qDebug() << "Suspend timer disabled";
        suspendTimer.stop();
    }
    void startSuspendTimer();
public slots:
    void suspend(){
        if(!sleepInhibited()){
            qDebug() << "Suspending...";
            systemd->Suspend(false);
        }
    }
    void powerOff() {
        if(!powerOffInhibited()){
            qDebug() << "Powering off...";
            releasePowerOffInhibitors(true);
            rguard(false);
            systemd->PowerOff(false);
        }
    }
    void reboot() {
        if(!powerOffInhibited()){
            qDebug() << "Rebooting...";
            releasePowerOffInhibitors(true);
            rguard(false);
            systemd->Reboot(false);
        }
    }
    void activity();
    void inhibitSleep(QDBusMessage message){
        if(!sleepInhibited()){
            emit sleepInhibitedChanged(true);
        }
        suspendTimer.stop();
        sleepInhibitors.append(message.service());
        inhibitors.append(Inhibitor(systemd, "sleep:handle-suspend-key:idle", message.service(), "Application requested block", true));
    }
    void uninhibitSleep(QDBusMessage message);
    void inhibitPowerOff(QDBusMessage message){
        if(!powerOffInhibited()){
            emit powerOffInhibitedChanged(true);
        }
        powerOffInhibitors.append(message.service());
        inhibitors.append(Inhibitor(systemd, "shutdown:handle-power-key", message.service(), "Application requested block", true));
    }
    void uninhibitPowerOff(QDBusMessage message){
        if(!powerOffInhibited()){
            return;
        }
        powerOffInhibitors.removeAll(message.service());
        if(!powerOffInhibited()){
            emit powerOffInhibitedChanged(false);
        }
    }

signals:
    void leftAction();
    void homeAction();
    void rightAction();
    void powerAction();
    void sleepInhibitedChanged(bool);
    void powerOffInhibitedChanged(bool);
    void autoSleepChanged(int);
    void deviceSuspending();
    void deviceResuming();

private slots:
    void PrepareForSleep(bool suspending);
    void timeout();

private:
    Manager* systemd;
    QList<Inhibitor> inhibitors;
    Application* resumeApp;
    QTimer suspendTimer;
    QSettings settings;
    QStringList sleepInhibitors;
    QStringList powerOffInhibitors;
    int m_autoSleep;
    bool wifiWasOn = false;

    void inhibitSleep(){
        inhibitors.append(Inhibitor(systemd, "sleep", qApp->applicationName(), "Handle sleep screen"));
    }
    void inhibitPowerOff(){
        inhibitors.append(Inhibitor(systemd, "shutdown", qApp->applicationName(), "Block power off from any other method", true));
        rguard(true);
    }
    void releaseSleepInhibitors(bool block = false){
        QMutableListIterator<Inhibitor> i(inhibitors);
        while(i.hasNext()){
            auto inhibitor = i.next();
            if(inhibitor.what.contains("sleep") && inhibitor.block == block){
                inhibitor.release();
            }
            if(inhibitor.released()){
                i.remove();
            }
        }
    }
    void releasePowerOffInhibitors(bool block = false){
        QMutableListIterator<Inhibitor> i(inhibitors);
        while(i.hasNext()){
            auto inhibitor = i.next();
            if(inhibitor.what.contains("shutdown") && inhibitor.block == block){
                inhibitor.release();
            }
            if(inhibitor.released()){
                i.remove();
            }
        }
    }
    void rguard(bool install){
        QProcess::execute("/opt/bin/rguard", QStringList() << (install ? "-1" : "-0"));
    }
};

#endif // SYSTEMAPI_H
