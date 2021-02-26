#ifndef SYSTEMAPI_H
#define SYSTEMAPI_H

#include <QObject>
#include <QMetaType>
#include <QMutableListIterator>
#include <QTimer>
#include <QMutex>

#include "apibase.h"
#include "buttonhandler.h"
#include "application.h"
#include "screenapi.h"
#include "digitizerhandler.h"
#include "login1_interface.h"

#define DEBUG_TOUCH

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

struct Touch {
    int slot = 0;
    int id = -1;
    int x = 0;
    int y = 0;
    bool active = false;
    bool existing = false;
    bool modified = true;
    int pressure = 0;
    int major = 0;
    int minor = 0;
    int orientation = 0;
    string debugString() const{
        return "<Touch " + to_string(id) + " (" + to_string(x) + ", " + to_string(y) + ") " + (active ? "pressed" : "released") + ">";
    }
};
#ifdef DEBUG_TOUCH
QDebug operator<<(QDebug debug, const Touch& touch);
QDebug operator<<(QDebug debug, Touch* touch);
#endif
Q_DECLARE_METATYPE(Touch)
Q_DECLARE_METATYPE(input_event)

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
       powerOffInhibitors(),
       mutex(),
       touches(){
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
        qRegisterMetaType<input_event>();
        connect(touchHandler, &DigitizerHandler::inputEvent, this, &SystemAPI::touchEvent);
        connect(touchHandler, &DigitizerHandler::activity, this, &SystemAPI::activity);
        connect(wacomHandler, &DigitizerHandler::activity, this, &SystemAPI::activity);
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
    int autoSleep(){return m_autoSleep; }
    void setAutoSleep(int autoSleep);
    bool sleepInhibited(){ return sleepInhibitors.length(); }
    bool powerOffInhibited(){ return powerOffInhibitors.length(); }
    void uninhibitAll(QString name);
    void stopSuspendTimer(){
        qDebug() << "Suspend timer disabled";
        suspendTimer.stop();
    }
    void startSuspendTimer();
    void lock(){ mutex.lock(); }
    void unlock() { mutex.unlock(); }
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
    void bottomAction();
    void topAction();
    void sleepInhibitedChanged(bool);
    void powerOffInhibitedChanged(bool);
    void autoSleepChanged(int);
    void deviceSuspending();
    void deviceResuming();

private slots:
    void PrepareForSleep(bool suspending);
    void timeout();
    void touchEvent(const input_event& event){
        switch(event.type){
            case EV_SYN:
                switch(event.code){
                    case SYN_REPORT:
                        // Always mark the current slot as modified
                        auto touch = getEvent(currentSlot);
                        touch->modified = true;
                        // Remove any invalid events
                        for(auto touch : touches.values()){
                            if(touch->id == -1){
                                touches.remove(touch->slot);
                                delete touch;
                            }
                        }
                        QList<Touch*> released;
                        QList<Touch*> pressed;
                        QList<Touch*> moved;
                        for(auto touch : touches.values()){
                            if(!touch->active){
                                released.append(touch);
                            }else if(!touch->existing){
                                pressed.append(touch);
                            }else if(touch->modified){
                                moved.append(touch);
                            }
                        }
                        if(pressed.length()){
                            touchDown(pressed);
                        }
                        if(moved.length()){
                            touchMove(moved);
                        }
                        if(released.length()){
                            touchUp(released);
                        }
                        // Cleanup released touches
                        for(auto touch : released){
                            if(!touch->active){
                                touches.remove(touch->slot);
                                delete touch;
                            }
                        }
                        // Setup touches for next event set
                        for(auto touch : touches.values()){
                            touch->modified = false;
                            touch->existing = true;
                        }
                    break;
                }
            break;
            case EV_ABS:
                if(currentSlot == -1 && event.code != ABS_MT_SLOT){
                    return;
                }
                switch(event.code){
                    case ABS_MT_SLOT:{
                        currentSlot = event.value;
                        auto touch = getEvent(currentSlot);
                        touch->modified = true;
                    }break;
                    case ABS_MT_TRACKING_ID:{
                        auto touch = getEvent(currentSlot);
                        if(event.value == -1){
                            touch->active = false;
                            currentSlot = 0;
                        }else{
                            touch->active = true;
                            touch->id = event.value;
                        }
                    }break;
                    case ABS_MT_POSITION_X:{
                        auto touch = getEvent(currentSlot);
                        touch->x = event.value;
                    }break;
                    case ABS_MT_POSITION_Y:{
                        auto touch = getEvent(currentSlot);
                        touch->y = event.value;
                    }break;
                    case ABS_MT_PRESSURE:{
                        auto touch = getEvent(currentSlot);
                        touch->pressure = event.value;
                    }break;
                    case ABS_MT_TOUCH_MAJOR:{
                        auto touch = getEvent(currentSlot);
                        touch->major = event.value;
                    }break;
                    case ABS_MT_TOUCH_MINOR:{
                        auto touch = getEvent(currentSlot);
                        touch->minor = event.value;
                    }break;
                    case ABS_MT_ORIENTATION:{
                        auto touch = getEvent(currentSlot);
                        touch->orientation = event.value;
                    }break;
                }
            break;
        }

    }
    void touchDown(QList<Touch*> touches){
#ifdef DEBUG_TOUCH
        qDebug() << "DOWN" << touches;
#endif
        if(getCurrentFingers() != 1){
            touchHandler->ungrab();
            swipeDir = -1;
            return;
        }
        auto touch = touches.first();
        if(touch->y <= 20){
            swipeDir = 2;
        }else if(touch->y > 1003){
            swipeDir = 3;
        }else if(touch->x <= 20){
            swipeDir = 1;
        }else if(touch->x > 747){
            swipeDir = 0;
        }else{
            touchHandler->ungrab();
            swipeDir = -1;
            return;
        }
        touchHandler->grab();
#ifdef DEBUG_TOUCH
            qDebug() << "Swipe started" << swipeDir;
#endif
        startLocation = location = QPoint(touch->x, touch->y);
    }
    void touchUp(QList<Touch*> touches){
#ifdef DEBUG_TOUCH
        qDebug() << "UP" << touches;
#endif
        if(swipeDir == -1){
#ifdef DEBUG_TOUCH
            qDebug() << "Not swiping";
#endif
            return;
        }
        touchHandler->ungrab();
        auto touch = touches.first();
        if(swipeDir == 0){
            if(touch->x > location.x() || touch->x > startLocation.x() + 20){
#ifdef DEBUG_TOUCH
                qDebug() << "Swipe Cancelled";
#endif
                swipeDir = -1;
                return;
            }
            emit leftAction();
        }else if(swipeDir == 1){
            if(touch->x < location.x() || touch->x < startLocation.x() + 20){
#ifdef DEBUG_TOUCH
                qDebug() << "Swipe Cancelled";
#endif
                swipeDir = -1;
                return;
            }
            emit rightAction();
        }else  if(swipeDir == 2){
            if(touch->y < location.y() || touch->y < startLocation.y() + 20){
#ifdef DEBUG_TOUCH
                qDebug() << "Swipe Cancelled";
#endif
                swipeDir = -1;
                return;
            }
            emit bottomAction();
        }else{
            if(touch->y > location.y() || touch->y > startLocation.y() + 20){
#ifdef DEBUG_TOUCH
                qDebug() << "Swipe Cancelled";
#endif
                swipeDir = -1;
                return;
            }
            emit topAction();
        }
#ifdef DEBUG_TOUCH
            qDebug() << "Swipe action" << swipeDir;
#endif
    }
    void touchMove(QList<Touch*> touches){
#ifdef DEBUG_TOUCH
        qDebug() << "MOVE" << touches;
#endif
        if(swipeDir == -1){
            return;
        }
        auto touch = touches.first();
        if(touch->y > location.y()){
            location = QPoint(touch->x, touch->y);
        }
    }

private:
    Manager* systemd;
    QList<Inhibitor> inhibitors;
    Application* resumeApp;
    QTimer suspendTimer;
    QSettings settings;
    QStringList sleepInhibitors;
    QStringList powerOffInhibitors;
    QMutex mutex;
    QMap<int, Touch*> touches;
    int currentSlot = 0;
    int m_autoSleep;
    bool wifiWasOn = false;
    // -1 - not swiping
    // 0 - right
    // 1 - left
    // 2 - up
    // 3 - down
    int swipeDir = -1;
    QPoint location;
    QPoint startLocation;

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
    Touch* getEvent(int slot){
        if(slot == -1){
            return nullptr;
        }
        if(!touches.contains(slot)){
            touches.insert(slot, new Touch{
                .slot = slot
            });
        }
        return touches.value(slot);
    }
    int getCurrentFingers(){
        return std::count_if(touches.begin(), touches.end(), [](Touch* touch){
            return touch->active;
        });
    }
};

#endif // SYSTEMAPI_H
