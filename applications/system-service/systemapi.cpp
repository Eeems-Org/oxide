#include "systemapi.h"
#include "appsapi.h"
#include "powerapi.h"
#include "wifiapi.h"
#include "devicesettings.h"

void SystemAPI::PrepareForSleep(bool suspending){
    auto device = deviceSettings.getDeviceType();
    if(suspending){
        emit deviceSuspending();
        auto path = appsAPI->currentApplication();
        if(path.path() != "/"){
            resumeApp = appsAPI->getApplication(path);
            resumeApp->pause(false);
        }else{
            resumeApp = nullptr;
        }
        screenAPI->drawFullscreenImage("/usr/share/remarkable/suspended.png");
        qDebug() << "Suspending...";
        buttonHandler->setEnabled(false);
        if(device == DeviceSettings::DeviceType::RM2){
            if(wifiAPI->state() != WifiAPI::State::Off){
                wifiWasOn = true;
                wifiAPI->disable();
            }
            system("rmmod brcmfmac");
        }
        releaseSleepInhibitors();
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
            resumeApp->resume();
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
    }
}
void SystemAPI::setAutoSleep(int autoSleep){
    if(autoSleep < 0 || autoSleep > 10){
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
