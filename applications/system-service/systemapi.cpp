#include <liboxide.h>

#include "systemapi.h"
#include "appsapi.h"
#include "powerapi.h"
#include "wifiapi.h"
#include "notificationapi.h"

QDebug operator<<(QDebug debug, const Touch& touch){
    QDebugStateSaver saver(debug);
    debug.nospace() << touch.debugString().c_str();
    return debug.maybeSpace();
}
QDebug operator<<(QDebug debug, Touch* touch){
    QDebugStateSaver saver(debug);
    debug.nospace() << touch->debugString().c_str();
    return debug.maybeSpace();
}

void SystemAPI::PrepareForSleep(bool suspending){
    auto device = deviceSettings.getDeviceType();
    if(suspending){
        lockTimestamp = QDateTime::currentMSecsSinceEpoch() + lockTimer.remainingTime();
        Oxide::Sentry::sentry_transaction("system", "suspend", [this, device](Oxide::Sentry::Transaction* t){
            qDebug() << "Preparing for suspend...";
            Oxide::Sentry::sentry_span(t, "prepare", "Prepare for suspend", [this]{
                wifiAPI->stopUpdating();
                emit deviceSuspending();
                appsAPI->recordPreviousApplication();
                auto path = appsAPI->currentApplicationNoSecurityCheck();
                if(path.path() != "/"){
                    resumeApp = appsAPI->getApplication(path);
                    resumeApp->pauseNoSecurityCheck(false);
                    qDebug() << "Resume app set to " << resumeApp->name();
                }else{
                    qDebug() << "Unable to set resume app";
                    resumeApp = nullptr;
                }
            });
            Oxide::Sentry::sentry_span(t, "screen", "Update screen with suspend image", []{
                if(QFile::exists("/usr/share/remarkable/sleeping.png")){
                    screenAPI->drawFullscreenImage("/usr/share/remarkable/sleeping.png");
                }else{
                    screenAPI->drawFullscreenImage("/usr/share/remarkable/suspended.png");
                }
            });
            Oxide::Sentry::sentry_span(t, "disable", "Disable various services", [this, device]{
                buttonHandler->setEnabled(false);
                if(device == Oxide::DeviceSettings::DeviceType::RM2){
                    if(wifiAPI->state() != WifiAPI::State::Off){
                        wifiWasOn = true;
                        wifiAPI->disable();
                    }
                    system("rmmod brcmfmac");
                }
                releaseSleepInhibitors();
            });
            Oxide::Sentry::sentry_span(t, "clear-input", "Clear input buffers", [this]{
                touchHandler->clear_buffer();
                wacomHandler->clear_buffer();
                buttonHandler->clear_buffer();
            });
            qDebug() << "Suspending...";
        });
    }else{
        Oxide::Sentry::sentry_transaction("system", "resume", [this, device](Oxide::Sentry::Transaction* t){
            Oxide::Sentry::sentry_span(t, "inhibit", "Inhibit sleep", [this]{
                inhibitSleep();
            });
            qDebug() << "Resuming...";
            Oxide::Sentry::sentry_span(t, "process", "Process events", []{
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            });
            Oxide::Sentry::sentry_span(t, "resume", "Resume running application or go to lockscreen", [this]{
                if(lockOnSuspend() || (autoLock() && QDateTime::currentMSecsSinceEpoch() >= lockTimestamp)){
                    qDebug() << "Lock timer expired while suspended";
                    auto lockscreenApp = appsAPI->getApplication(appsAPI->lockscreenApplication());
                    if(lockscreenApp != nullptr){
                        qDebug() << "Resume app set to lockscreen application";
                        resumeApp = lockscreenApp;
                    }
                }
                if(resumeApp == nullptr){
                    qDebug() << "Resume app set to startup application";
                    resumeApp = appsAPI->getApplication(appsAPI->startupApplication());
                }
                if(resumeApp != nullptr){
                    resumeApp->resumeNoSecurityCheck();
                }else{
                    qDebug() << "Unable to find an app to resume";
                }
            });
            Oxide::Sentry::sentry_span(t, "enable", "Enable various services", [this, device]{
                buttonHandler->setEnabled(true);
                emit deviceResuming();
                if(autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected){
                    qDebug() << "Suspend timer re-enabled due to resume";
                    suspendTimer.start(autoSleep() * 60 * 1000);
                }
                if(autoLock()){
                    qDebug() << "Lock timer re-enabled due to resume";
                    lockTimer.start(autoLock() * 60 * 1000);
                }
                if(device == Oxide::DeviceSettings::DeviceType::RM2){
                    system("modprobe brcmfmac");
                    if(wifiWasOn){
                        wifiAPI->enable();
                    }
                }
                wifiAPI->resumeUpdating();
            });
        });
    }
}
void SystemAPI::setAutoSleep(int _autoSleep){
    if(_autoSleep < 0 || _autoSleep > 360){
        return;
    }
    qDebug() << "Auto Sleep" << _autoSleep;
    sharedSettings.set_autoSleep(_autoSleep);
    if(_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        suspendTimer.setInterval(_autoSleep * 60 * 1000);
    }else if(!_autoSleep){
        suspendTimer.stop();
    }
    sharedSettings.sync();
    emit autoSleepChanged(_autoSleep);
}
void SystemAPI::setAutoLock(int _autoLock){
    if(_autoLock < 0 || _autoLock > 360){
        return;
    }
    qDebug() << "Auto Lock" << _autoLock;
    sharedSettings.set_autoLock(_autoLock);
    lockTimer.setInterval(_autoLock * 60 * 1000);
    sharedSettings.sync();
    emit autoLockChanged(_autoLock);
}
void SystemAPI::setLockOnSuspend(bool _lockOnSuspend){
    sharedSettings.set_lockOnSuspend(_lockOnSuspend);
    qDebug() << "Lock on Suspend" << _lockOnSuspend;
    sharedSettings.sync();
    emit lockOnSuspendChanged(_lockOnSuspend);
}
void SystemAPI::uninhibitAll(QString name){
    if(powerOffInhibited()){
        powerOffInhibitors.removeAll(name);
        if(!powerOffInhibited()){
            emit powerOffInhibitedChanged(false);
        }
    }
    if(sleepInhibited()){
        sleepInhibitors.removeAll(name);
        if(!sleepInhibited()){
            emit sleepInhibitedChanged(false);
        }
    }
    if(!sleepInhibited() && autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected && !suspendTimer.isActive()){
        qDebug() << "Suspend timer re-enabled due to uninhibit" << name;
        suspendTimer.start(autoSleep() * 60 * 1000);
    }
}
void SystemAPI::startSuspendTimer(){
    if(autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected && !suspendTimer.isActive()){
        qDebug() << "Suspend timer re-enabled due to start Suspend timer";
        suspendTimer.start(autoSleep() * 60 * 1000);
    }
}
void SystemAPI::startLockTimer(){
    if(autoLock() && !lockTimer.isActive()){
        qDebug() << "Lock timer re-enabled due to start lock timer";
        lockTimer.start(autoSleep() * 60 * 1000);
    }
}
void SystemAPI::activity(){
    auto active = suspendTimer.isActive();
    suspendTimer.stop();
    if(autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        if(!active){
            qDebug() << "Suspend timer re-enabled due to activity";
        }
        suspendTimer.start(autoSleep() * 60 * 1000);
    }else if(active){
        qDebug() << "Suspend timer disabled";
    }
    active = lockTimer.isActive();
    if(autoLock()){
        if(!active){
            qDebug() << "Lock timer re-enabled due to activity";
        }
        lockTimer.start(autoLock() * 60 * 1000);
    }else if(active){
        qDebug() << "Lock timer disabled";
    }
}

void SystemAPI::uninhibitSleep(QDBusMessage message){
    if(!sleepInhibited()){
        return;
    }
    sleepInhibitors.removeAll(message.service());
    if(!sleepInhibited() && autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        if(!suspendTimer.isActive()){
            qDebug() << "Suspend timer re-enabled due to uninhibit sleep" << message.service();
            suspendTimer.start(autoSleep() * 60 * 1000);
        }
        releaseSleepInhibitors(true);
    }
    if(!sleepInhibited()){
        emit sleepInhibitedChanged(false);
    }
}
void SystemAPI::suspendTimeout(){
    if(autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        qDebug() << "Automatic suspend due to inactivity...";
        suspend();
    }
}
void SystemAPI::lockTimeout(){
    if(autoLock()){
        auto lockscreenApp = appsAPI->getApplication(appsAPI->lockscreenApplication());
        if(lockscreenApp != nullptr){
            qDebug() << "Automatic lock due to inactivity...";
            lockscreenApp->resumeNoSecurityCheck();
        }
    }
}
void SystemAPI::toggleSwipes(){
    bool state = !swipeStates[Up];
    setSwipeEnabled(Left, state);
    setSwipeEnabled(Right, state);
    setSwipeEnabled(Up, state);
    QString message = state ? "Swipes Enabled" : "Swipes Disabled";
    qDebug() << message;
    const QString& id = "system-swipe-toggle";
    auto notification = notificationAPI->add(id, OXIDE_SERVICE, "tarnish", message, "");
    if(notification == nullptr){
        notification = notificationAPI->getByIdentifier(id);
        if(notification == nullptr){
            return;
        }
    }else{
        connect(notification, &Notification::clicked, [notification]{
            notification->remove();
        });
    }
    notification->setText(message);
    notification->display();
}
