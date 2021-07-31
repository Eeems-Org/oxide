#include "systemapi.h"
#include "appsapi.h"
#include "powerapi.h"
#include "wifiapi.h"
#include "notificationapi.h"
#include "devicesettings.h"

#ifdef DEBUG
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
#endif

void SystemAPI::PrepareForSleep(bool suspending){
    auto device = deviceSettings.getDeviceType();
    if(suspending){
        qDebug() << "Preparing for suspend...";
        wifiAPI->stopUpdating();
        emit deviceSuspending();
        appsAPI->recordPreviousApplication();
        auto path = appsAPI->currentApplicationNoSecurityCheck();
        if(path.path() != "/"){
            resumeApp = appsAPI->getApplication(path);
            resumeApp->pauseNoSecurityCheck(false);
        }else{
            resumeApp = nullptr;
        }
        if(QFile::exists("/usr/share/remarkable/sleeping.png")){
            screenAPI->drawFullscreenImage("/usr/share/remarkable/sleeping.png");
        }else{
            screenAPI->drawFullscreenImage("/usr/share/remarkable/suspended.png");
        }
        buttonHandler->setEnabled(false);
        if(device == DeviceSettings::DeviceType::RM2){
            if(wifiAPI->state() != WifiAPI::State::Off){
                wifiWasOn = true;
                wifiAPI->disable();
            }
            system("rmmod brcmfmac");
        }
        releaseSleepInhibitors();
        qDebug() << "Suspending...";
    }else{
        inhibitSleep();
        qDebug() << "Resuming...";
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        auto lockscreenApp = appsAPI->getApplication(appsAPI->lockscreenApplication());
        if(lockscreenApp != nullptr){
            resumeApp = lockscreenApp;
        }
        if(resumeApp == nullptr){
            resumeApp = appsAPI->getApplication(appsAPI->startupApplication());
        }
        if(resumeApp != nullptr){
            resumeApp->resumeNoSecurityCheck();
        }
        buttonHandler->setEnabled(true);
        emit deviceResuming();
        if(m_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected){
            qDebug() << "Suspend timer re-enabled due to resume";
            suspendTimer.start(m_autoSleep * 60 * 1000);
        }
        if(device == DeviceSettings::DeviceType::RM2){
            system("modprobe brcmfmac");
            if(wifiWasOn){
                wifiAPI->enable();
            }
        }
        wifiAPI->resumeUpdating();
    }
}
void SystemAPI::setAutoSleep(int autoSleep){
    if(autoSleep < 0 || autoSleep > 360){
        return;
    }
    qDebug() << "Auto Sleep" << autoSleep;
    m_autoSleep = autoSleep;
    if(m_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        suspendTimer.setInterval(m_autoSleep * 60 * 1000);
    }else if(!m_autoSleep){
        suspendTimer.stop();
    }
    settings.setValue("autoSleep", autoSleep);
    settings.sync();
    emit autoSleepChanged(autoSleep);
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
    if(!sleepInhibited() && m_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected && !suspendTimer.isActive()){
        qDebug() << "Suspend timer re-enabled due to uninhibit" << name;
        suspendTimer.start(m_autoSleep * 60 * 1000);
    }
}
void SystemAPI::startSuspendTimer(){
    if(m_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected && !suspendTimer.isActive()){
        qDebug() << "Suspend timer re-enabled due to start Suspend timer";
        suspendTimer.start(m_autoSleep * 60 * 1000);
    }
}
void SystemAPI::activity(){
    auto active = suspendTimer.isActive();
    suspendTimer.stop();
    if(m_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        if(!active){
            qDebug() << "Suspend timer re-enabled due to activity";
        }
        suspendTimer.start(m_autoSleep * 60 * 1000);
    }else if(active){
        qDebug() << "Suspend timer disabled";
    }
}

void SystemAPI::uninhibitSleep(QDBusMessage message){
    if(!sleepInhibited()){
        return;
    }
    sleepInhibitors.removeAll(message.service());
    if(!sleepInhibited() && m_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        if(!suspendTimer.isActive()){
            qDebug() << "Suspend timer re-enabled due to uninhibit sleep" << message.service();
            suspendTimer.start(m_autoSleep * 60 * 1000);
        }
        releaseSleepInhibitors(true);
    }
    if(!sleepInhibited()){
        emit sleepInhibitedChanged(false);
    }
}
void SystemAPI::timeout(){
    if(m_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        qDebug() << "Automatic suspend due to inactivity...";
        suspend();
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
