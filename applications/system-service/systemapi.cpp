#include <liboxide.h>
#include <QMutableListIterator>
#include <QMetaType>
#include <QKeyEvent>

#include "systemapi.h"
#include "appsapi.h"
#include "powerapi.h"
#include "wifiapi.h"
#include "notificationapi.h"
#include "screenapi.h"
#include "guiapi.h"
#include "digitizerhandler.h"
#include "eventlistener.h"

QDebug operator<<(QDebug debug, const TouchData& touch){
    QDebugStateSaver saver(debug);
    debug.nospace() << touch.debugString().c_str();
    return debug.maybeSpace();
}
QDebug operator<<(QDebug debug, TouchData* touch){
    QDebugStateSaver saver(debug);
    debug.nospace() << touch->debugString().c_str();
    return debug.maybeSpace();
}

void SystemAPI::PrepareForSleep(bool suspending){
    auto device = deviceSettings.getDeviceType();
    if(suspending){
        Oxide::Sentry::sentry_transaction("system", "suspend", [this, device](Oxide::Sentry::Transaction* t){
            if(autoLock()){
                lockTimestamp = QDateTime::currentMSecsSinceEpoch() + lockTimer.remainingTime();
                O_INFO("Auto Lock timestamp:" << lockTimestamp);
            }
            O_INFO("Preparing for suspend...");
            Oxide::Sentry::sentry_span(t, "prepare", "Prepare for suspend", [this]{
                wifiAPI->stopUpdating();
                emit deviceSuspending();
                appsAPI->recordPreviousApplication();
                auto path = appsAPI->currentApplicationNoSecurityCheck();
                if(path.path() != "/"){
                    resumeApp = appsAPI->getApplication(path);
                    resumeApp->pauseNoSecurityCheck(false);
                    O_INFO("Resume app set to " << resumeApp->name());
                }else{
                    O_INFO("Unable to set resume app");
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
                if(device == Oxide::DeviceSettings::DeviceType::RM2){
                    if(wifiAPI->state() != WifiAPI::State::Off){
                        wifiWasOn = true;
                        wifiAPI->disable();
                    }
                    system("rmmod brcmfmac");
                }
                releaseSleepInhibitors();
            });
            O_INFO("Suspending...");
        });
    }else{
        Oxide::Sentry::sentry_transaction("system", "resume", [this, device](Oxide::Sentry::Transaction* t){
            Oxide::Sentry::sentry_span(t, "inhibit", "Inhibit sleep", [this]{
                inhibitSleep();
            });
            O_INFO("Resuming...");
            Oxide::Sentry::sentry_span(t, "process", "Process events", []{
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            });
            Oxide::Sentry::sentry_span(t, "resume", "Resume running application or go to lockscreen", [this]{
                auto now = QDateTime::currentMSecsSinceEpoch();
                bool lockTimeout = autoLock();
                if(lockTimeout){
                    O_INFO("Current timestamp:" << now);
                    lockTimeout = now >= lockTimestamp;
                }
                if(lockOnSuspend() || lockTimeout){
                    if(lockTimeout){
                        O_INFO("Lock timer expired while suspended");
                    }else{
                        O_INFO("Always locking after suspend");
                    }
                    auto lockscreenApp = appsAPI->getApplication(appsAPI->lockscreenApplication());
                    if(lockscreenApp != nullptr){
                        O_INFO("Resume app set to lockscreen application");
                        resumeApp = lockscreenApp;
                    }
                }
                if(resumeApp == nullptr){
                    O_INFO("Resume app set to startup application");
                    resumeApp = appsAPI->getApplication(appsAPI->startupApplication());
                }
                if(resumeApp != nullptr){
                    resumeApp->resumeNoSecurityCheck();
                }else{
                    O_INFO("Unable to find an app to resume");
                }
            });
            Oxide::Sentry::sentry_span(t, "enable", "Enable various services", [this, device]{
                emit deviceResuming();
                if(autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected){
                    O_INFO("Suspend timer re-enabled due to resume");
                    suspendTimer.start(autoSleep() * 60 * 1000);
                }
                if(autoLock()){
                    O_INFO("Lock timer re-enabled due to resume");
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
SystemAPI* SystemAPI::singleton(SystemAPI* self){
    static SystemAPI* instance;
    if(self != nullptr){
        instance = self;
    }
    return instance;
}

SystemAPI::SystemAPI(QObject* parent)
    : APIBase(parent),
      suspendTimer(this),
      lockTimer(this),
      settings(this),
      sleepInhibitors(),
      powerOffInhibitors(),
      mutex(),
      touches(),
      swipeStates(),
      swipeLengths() {
    Oxide::Sentry::sentry_transaction("system", "init", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::sentry_span(t, "settings", "Sync settings", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "swipes", "Default swipe values", [this]{
                for(short i = Right; i <= Down; i++){
                    swipeStates[(SwipeDirection)i] = true;
                }
            });
            Oxide::Sentry::sentry_span(s, "sync", "Sync settings", [this]{
                settings.sync();
            });
            Oxide::Sentry::sentry_span(s, "singleton", "Instantiate singleton", [this]{
                singleton(this);
            });
            resumeApp = nullptr;
        });
        Oxide::Sentry::sentry_span(t, "systemd", "Connect to SystemD DBus", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "manager", "Create manager object", [this]{
                systemd = new Manager("org.freedesktop.login1", "/org/freedesktop/login1", QDBusConnection::systemBus());
                systemd->moveToThread(thread());
                systemd->setParent(this);
            });
            Oxide::Sentry::sentry_span(s, "connect", "Connect to signals", [this]{
                // Handle Systemd signals
                connect(systemd, &Manager::PrepareForSleep, this, &SystemAPI::PrepareForSleep);
                connect(&suspendTimer, &QTimer::timeout, this, &SystemAPI::suspendTimeout);
                connect(&lockTimer, &QTimer::timeout, this, &SystemAPI::lockTimeout);
            });
        });
        Oxide::Sentry::sentry_span(t, "autoSleep", "Setup automatic sleep", [this](Oxide::Sentry::Span* s){
            QSettings settings;
            if(QFile::exists(settings.fileName())){
                O_INFO("Importing old settings");
                settings.sync();
                if(settings.contains("autoSleep")){
                    O_INFO("Importing old autoSleep");
                    sharedSettings.set_autoSleep(settings.value("autoSleep").toInt());
                }
                int size = settings.beginReadArray("swipes");
                if(size){
                    sharedSettings.beginWriteArray("swipes");
                    for(short i = Right; i <= Down && i < size; i++){
                        settings.setArrayIndex(i);
                        sharedSettings.setArrayIndex(i);
                        O_INFO(QString("Importing old swipe[%1]").arg(i));
                        sharedSettings.setValue("enabled", settings.value("enabled", true));
                        sharedSettings.setValue("length", settings.value("length", 30));
                    }
                    sharedSettings.endArray();
                }
                settings.endArray();
                settings.remove("swipes");
                settings.sync();
                sharedSettings.sync();
            }
            if(autoSleep() < 0) {
                sharedSettings.set_autoSleep(0);
            }else if(autoSleep() > 10){
                sharedSettings.set_autoSleep(10);
            }
            O_INFO("Auto Sleep" << autoSleep());
            Oxide::Sentry::sentry_span(s, "timer", "Setup timers", [this]{
                if(autoSleep()){
                    suspendTimer.start(autoSleep() * 60 * 1000);
                }else{
                    suspendTimer.stop();
                }
                if(autoLock()){
                    lockTimer.start(autoLock() * 60 * 1000);
                }else{
                    lockTimer.stop();
                }
            });
            connect(&sharedSettings, &Oxide::SharedSettings::autoSleepChanged, [this](int _autoSleep){ emit autoSleepChanged(_autoSleep); });
            connect(&sharedSettings, &Oxide::SharedSettings::changed, [this](){
                sharedSettings.beginReadArray("swipes");
                for(short i = Right; i <= Down; i++){
                    sharedSettings.setArrayIndex(i);
                    swipeStates[(SwipeDirection)i] = sharedSettings.value("enabled", true).toBool();
                    int length = sharedSettings.value("length", 30).toInt();
                    swipeLengths[(SwipeDirection)i] = length;
                    emit swipeLengthChanged(i, length);
                }
                sharedSettings.endArray();
            });
        });
        Oxide::Sentry::sentry_span(t, "swipes", "Load swipe settings", [this](){
            sharedSettings.beginReadArray("swipes");
            for(short i = Right; i <= Down; i++){
                sharedSettings.setArrayIndex(i);
                swipeStates[(SwipeDirection)i] = sharedSettings.value("enabled", true).toBool();
                swipeLengths[(SwipeDirection)i] = sharedSettings.value("length", 30).toInt();
            }
            sharedSettings.endArray();
        });
        // Ask Systemd to tell us nicely when we are about to suspend or resume
        Oxide::Sentry::sentry_span(t, "inhibit", "Inhibit sleep and power off", [this](Oxide::Sentry::Span* s){
            Oxide::Sentry::sentry_span(s, "inhibitSleep", "Inhibit sleep", [this]{
                inhibitSleep();
            });
            Oxide::Sentry::sentry_span(s, "inhibitPowerOff", "Inhibit power off", [this]{
                inhibitPowerOff();
            });
        });
        Oxide::Sentry::sentry_span(t, "input", "Connect input events", [this]{
            connect(touchHandler, &DigitizerHandler::inputEvent, this, &SystemAPI::touchEvent);
            connect(wacomHandler, &DigitizerHandler::inputEvent, this, &SystemAPI::penEvent);
            eventListener->append([this](QObject* object, QEvent* event){
                Q_UNUSED(object)
                switch(event->type()){
                    case QEvent::KeyPress:{
                        activity();
                        auto keyEvent = static_cast<QKeyEvent*>(event);
                        auto key = keyEvent->key();
                        switch(key){
                            case Qt::Key_PowerOff:
                            case Qt::Key_Left:
                            case Qt::Key_Right:
                            case Qt::Key_Home:
                                break;
                            default:
                                return false;
                        }
                        if(!m_pressStates.contains(key)){
                            m_pressStates.insert(key, new QTimer());
                            connect(m_pressStates[key], &QTimer::timeout, this, ([this, key]{
                                switch(key){
                                    case Qt::Key_PowerOff: return &SystemAPI::suspend;
                                    case Qt::Key_Left: return &SystemAPI::leftAction;
                                    case Qt::Key_Right: return &SystemAPI::rightAction;
                                    case Qt::Key_Home: return &SystemAPI::homeAction;
                                }
                                Q_ASSERT(false);
                            })());
                        }
                        m_pressStates[key]->start(700);
                        return true;
                    }
                    case QEvent::KeyRelease:{
                        activity();
                        auto keyEvent = static_cast<QKeyEvent*>(event);
                        auto key = keyEvent->key();
                        switch(key){
                            case Qt::Key_PowerOff:
                            case Qt::Key_Left:
                            case Qt::Key_Right:
                            case Qt::Key_Home:
                                break;
                            default:
                                dispatchToThread(guiAPI->thread(), [keyEvent]{ guiAPI->writeKeyEvent(keyEvent); });
                                return false;
                        }
                        if(!m_pressStates.contains(key) || !m_pressStates[key]->isActive()){
                            return true;
                        }
                        m_pressStates[key]->stop();
                        dispatchToThread(guiAPI->thread(), [keyEvent, key]{
                            timeval time;
                            gettimeofday (&time, NULL);
                            input_event event{
                                .time = time,
                                .type = EV_KEY,
                                .code = (unsigned short)keyEvent->nativeScanCode(),
                                .value = 1,
                            };
                            input_event syn{
                                .time = time,
                                .type = EV_SYN,
                                .code = SYN_REPORT,
                                .value = 0,
                            };
                            guiAPI->keyEvent(event);
                            guiAPI->keyEvent(syn);
                            gettimeofday (&time, NULL);
                            event.time = time;
                            event.value = 0;
                            guiAPI->keyEvent(event);
                            guiAPI->keyEvent(syn);
                            guiAPI->writeKeyEvent(keyEvent);
                            guiAPI->writeKeyEvent(new QKeyEvent(QEvent::KeyPress, key, keyEvent->modifiers(), keyEvent->text()));
                        });
                        return true;
                    }
                    case QEvent::TabletEnterProximity:
                        activity();
                        penActive = true;
                        dispatchToThread(guiAPI->thread(), [event]{ guiAPI->writeTabletEvent(event); });
                        return true;
                    case QEvent::TabletLeaveProximity:
                        activity();
                        penActive = false;
                        dispatchToThread(guiAPI->thread(), [event]{ guiAPI->writeTabletEvent(event); });
                        return true;
                    case QEvent::TabletPress:
                    case QEvent::TabletMove:
                    case QEvent::TabletRelease:
                    case QEvent::TabletTrackingChange:
                        activity();
                        dispatchToThread(guiAPI->thread(), [event]{ guiAPI->writeTabletEvent(event); });
                        return true;
                    case QEvent::TouchBegin:
                    case QEvent::TouchCancel:
                    case QEvent::TouchEnd:
                    case QEvent::TouchUpdate:
                        activity();
                        dispatchToThread(guiAPI->thread(), [event]{ guiAPI->writeTouchEvent(event); });
                        return true;
                    default:
                        return false;
                }
            });
        });
        O_INFO("System API ready to use");
    });
}

SystemAPI::~SystemAPI(){
    O_INFO("Removing all inhibitors");
    rguard(false);
    // TODO - Use STL style iterators https://doc.qt.io/qt-5/containers.html#stl-style-iterators
    QMutableListIterator<Inhibitor> i(inhibitors);
    while(i.hasNext()){
        auto inhibitor = i.next();
        inhibitor.release();
        i.remove();
    }
    delete systemd;
}

void SystemAPI::setEnabled(bool enabled){
    O_INFO("System API" << enabled);
    m_enabled = enabled;
}

bool SystemAPI::isEnabled(){ return m_enabled; }

int SystemAPI::autoSleep(){return sharedSettings.autoSleep(); }

void SystemAPI::setAutoSleep(int _autoSleep){
    if(_autoSleep < 0 || _autoSleep > 360){
        return;
    }
    O_INFO("Auto Sleep" << _autoSleep);
    sharedSettings.set_autoSleep(_autoSleep);
    if(_autoSleep && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        suspendTimer.setInterval(_autoSleep * 60 * 1000);
    }else if(!_autoSleep){
        suspendTimer.stop();
    }
    sharedSettings.sync();
    emit autoSleepChanged(_autoSleep);
}

int SystemAPI::autoLock(){return sharedSettings.autoLock(); }
void SystemAPI::setAutoLock(int _autoLock){
    if(_autoLock < 0 || _autoLock > 360){
        return;
    }
    O_INFO("Auto Lock" << _autoLock);
    sharedSettings.set_autoLock(_autoLock);
    lockTimer.setInterval(_autoLock * 60 * 1000);
    sharedSettings.sync();
    emit autoLockChanged(_autoLock);
}

bool SystemAPI::lockOnSuspend(){return sharedSettings.lockOnSuspend(); }
void SystemAPI::setLockOnSuspend(bool _lockOnSuspend){
    sharedSettings.set_lockOnSuspend(_lockOnSuspend);
    O_INFO("Lock on Suspend" << _lockOnSuspend);
    sharedSettings.sync();
    emit lockOnSuspendChanged(_lockOnSuspend);
}

bool SystemAPI::sleepInhibited(){ return sleepInhibitors.length(); }

bool SystemAPI::powerOffInhibited(){ return powerOffInhibitors.length(); }
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
        O_INFO("Suspend timer re-enabled due to uninhibit" << name);
        suspendTimer.start(autoSleep() * 60 * 1000);
    }
}

void SystemAPI::stopSuspendTimer(){
    O_INFO("Suspend timer disabled");
    suspendTimer.stop();
}

void SystemAPI::stopLockTimer(){
    O_INFO("Lock timer disabled");
    lockTimer.stop();
}
void SystemAPI::startSuspendTimer(){
    if(autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected && !suspendTimer.isActive()){
        O_INFO("Suspend timer re-enabled due to start Suspend timer");
        suspendTimer.start(autoSleep() * 60 * 1000);
    }
}
void SystemAPI::startLockTimer(){
    if(autoLock() && !lockTimer.isActive()){
        O_INFO("Lock timer re-enabled due to start lock timer");
        lockTimer.start(autoSleep() * 60 * 1000);
    }
}

void SystemAPI::lock(){ mutex.lock(); }

void SystemAPI::unlock() { mutex.unlock(); }

void SystemAPI::setSwipeEnabled(int direction, bool enabled){
    if(!hasPermission("system")){
        return;
    }
    if(direction <= SwipeDirection::None || direction > SwipeDirection::Down){
        O_INFO("Invalid swipe direction: " << direction);
        return;
    }
    setSwipeEnabled((SwipeDirection)direction, enabled);
}

void SystemAPI::setSwipeEnabled(SwipeDirection direction, bool enabled){
    if(direction == None){
        return;
    }
    switch(direction){
        case Left:
            O_INFO("Swipe Left: " << enabled);
            break;
        case Right:
            O_INFO("Swipe Right: " << enabled);
            break;
        case Up:
            O_INFO("Swipe Up: " << enabled);
            break;
        case Down:
            O_INFO("Swipe Down: " << enabled);
            break;
        default:
            return;
    }
    swipeStates[direction] = enabled;
    sharedSettings.beginWriteArray("swipes");
    for(short i = Right; i <= Down; i++){
        sharedSettings.setArrayIndex(i);
        sharedSettings.setValue("enabled", swipeStates[(SwipeDirection)i]);
        sharedSettings.setValue("length", swipeLengths[(SwipeDirection)i]);
    }
    sharedSettings.endArray();
    sharedSettings.sync();
}

bool SystemAPI::getSwipeEnabled(int direction){
    if(!hasPermission("system")){
        return false;
    }
    if(direction <= SwipeDirection::None || direction > SwipeDirection::Down){
        O_INFO("Invalid swipe direction: " << direction);
        return false;
    }
    return getSwipeEnabled(direction);
}

bool SystemAPI::getSwipeEnabled(SwipeDirection direction){ return swipeStates[direction]; }

void SystemAPI::toggleSwipeEnabled(int direction){
    if(!hasPermission("system")){
        return;
    }
    if(direction <= SwipeDirection::None || direction > SwipeDirection::Down){
        O_INFO("Invalid swipe direction: " << direction);
        return;
    }
    toggleSwipeEnabled((SwipeDirection)direction);
}

void SystemAPI::toggleSwipeEnabled(SwipeDirection direction){ setSwipeEnabled(direction, !getSwipeEnabled(direction)); }

void SystemAPI::setSwipeLength(int direction, int length){
    int maxLength;
    if(!hasPermission("system")){
        return;
    }
    if(direction <= SwipeDirection::None || direction > SwipeDirection::Down){
        O_INFO("Invalid swipe direction: " << direction);
        return;
    }
    if(direction == SwipeDirection::Up || direction == SwipeDirection::Down){
        maxLength = deviceSettings.getTouchHeight();
    }else{
        maxLength = deviceSettings.getTouchWidth();
    }
    if(length < 0 || length > maxLength){
        O_INFO("Invalid swipe length: " << direction);
        return;
    }
    setSwipeLength((SwipeDirection)direction, length);
}

void SystemAPI::setSwipeLength(SwipeDirection direction, int length){
    if(direction == None){
        return;
    }
    switch(direction){
        case Left:
            O_INFO("Swipe Left Length: " << length);
            break;
        case Right:
            O_INFO("Swipe Right Length: " << length);
            break;
        case Up:
            O_INFO("Swipe Up Length: " << length);
            break;
        case Down:
            O_INFO("Swipe Down Length: " << length);
            break;
        default:
            return;
    }
    swipeLengths[direction] = length;
    sharedSettings.beginWriteArray("swipes");
    for(short i = Right; i <= Down; i++){
        sharedSettings.setArrayIndex(i);
        sharedSettings.setValue("enabled", swipeStates[(SwipeDirection)i]);
        sharedSettings.setValue("length", swipeLengths[(SwipeDirection)i]);
    }
    sharedSettings.endArray();
    sharedSettings.sync();
    emit swipeLengthChanged(direction, length);
}

int SystemAPI::getSwipeLength(int direction){
    if(!hasPermission("system")){
        return -1;
    }
    if(direction <= SwipeDirection::None || direction > SwipeDirection::Down){
        O_INFO("Invalid swipe direction: " << direction);
        return -1;
    }
    return getSwipeLength((SwipeDirection)direction);
}

int SystemAPI::getSwipeLength(SwipeDirection direction){ return swipeLengths[direction]; }

void SystemAPI::suspend(){
    if(sleepInhibited()){
        O_INFO("Unable to suspend. Action is currently inhibited.");
        return;
    }
    O_INFO("Requesting Suspend...");
    systemd->Suspend(false).waitForFinished();
    O_INFO("Suspend requested.");
}

void SystemAPI::powerOff() {
    if(powerOffInhibited()){
        O_INFO("Unable to power off. Action is currently inhibited.");
        return;
    }
    O_INFO("Requesting Power off...");
    releasePowerOffInhibitors(true);
    rguard(false);
    systemd->PowerOff(false).waitForFinished();
    O_INFO("Power off requested");
}

void SystemAPI::reboot() {
    if(powerOffInhibited()){
        O_INFO("Unable to reboot. Action is currently inhibited.");
        return;
    }
    O_INFO("Requesting Reboot...");
    releasePowerOffInhibitors(true);
    rguard(false);
    systemd->Reboot(false).waitForFinished();
    O_INFO("Reboot requested");
}
void SystemAPI::activity(){
    auto active = suspendTimer.isActive();
    suspendTimer.stop();
    if(autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        if(!active){
            O_INFO("Suspend timer re-enabled due to activity");
        }
        suspendTimer.start(autoSleep() * 60 * 1000);
    }else if(active){
        O_INFO("Suspend timer disabled");
    }
    active = lockTimer.isActive();
    if(autoLock()){
        if(!active){
            O_INFO("Lock timer re-enabled due to activity");
        }
        lockTimer.start(autoLock() * 60 * 1000);
    }else if(active){
        O_INFO("Lock timer disabled");
    }
}

void SystemAPI::inhibitSleep(QDBusMessage message){
    if(!sleepInhibited()){
        emit sleepInhibitedChanged(true);
    }
    suspendTimer.stop();
    sleepInhibitors.append(message.service());
    inhibitors.append(Inhibitor(systemd, "sleep:handle-suspend-key:idle", message.service(), "Application requested block", true));
}

void SystemAPI::uninhibitSleep(QDBusMessage message){
    if(!sleepInhibited()){
        return;
    }
    sleepInhibitors.removeAll(message.service());
    if(!sleepInhibited() && autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        if(!suspendTimer.isActive()){
            O_INFO("Suspend timer re-enabled due to uninhibit sleep" << message.service());
            suspendTimer.start(autoSleep() * 60 * 1000);
        }
        releaseSleepInhibitors(true);
    }
    if(!sleepInhibited()){
        emit sleepInhibitedChanged(false);
    }
}

void SystemAPI::inhibitPowerOff(QDBusMessage message){
    if(!powerOffInhibited()){
        emit powerOffInhibitedChanged(true);
    }
    powerOffInhibitors.append(message.service());
    inhibitors.append(Inhibitor(systemd, "shutdown:handle-power-key", message.service(), "Application requested block", true));
}

void SystemAPI::uninhibitPowerOff(QDBusMessage message){
    if(!powerOffInhibited()){
        return;
    }
    powerOffInhibitors.removeAll(message.service());
    if(!powerOffInhibited()){
        emit powerOffInhibitedChanged(false);
    }
}
void SystemAPI::suspendTimeout(){
    if(autoSleep() && powerAPI->chargerState() != PowerAPI::ChargerConnected){
        O_INFO("Automatic suspend due to inactivity...");
        suspend();
    }
}
void SystemAPI::lockTimeout(){
    if(autoLock()){
        auto lockscreenApp = appsAPI->getApplication(appsAPI->lockscreenApplication());
        if(lockscreenApp != nullptr){
            O_INFO("Automatic lock due to inactivity...");
            lockscreenApp->resumeNoSecurityCheck();
        }
    }
}

void SystemAPI::touchEvent(const input_event& event){
    switch(event.type){
        case EV_SYN:
            switch(event.code){
                case SYN_REPORT:
                    // Always mark the current slot as modified
                    auto touch = getEvent(currentSlot);
                    touch->modified = true;
                    QList<TouchData*> released;
                    QList<TouchData*> pressed;
                    QList<TouchData*> moved;
                    for(auto touch : touches.values()){
                        if(touch->id == -1){
                            touch->active = false;
                            released.append(touch);
                        }else if(!touch->active){
                            released.append(touch);
                        }else if(!touch->existing){
                            pressed.append(touch);
                        }else if(touch->modified){
                            moved.append(touch);
                        }
                    }
                    if(!penActive){
                        if(pressed.length()){
                            touchDown(pressed);
                        }
                        if(moved.length()){
                            touchMove(moved);
                        }
                        if(released.length()){
                            touchUp(released);
                        }
                    }else if(swipeDirection != None){
                        if(Oxide::debugEnabled()){
                            O_INFO("Swiping cancelled due to pen activity");
                        }
                        swipeDirection = None;
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
                        touch->existing = touch->existing || (touch->x != NULL_TOUCH_COORD && touch->y != NULL_TOUCH_COORD);
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
                    touch->active = event.value != -1;
                    if(touch->active){
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

void SystemAPI::penEvent(const input_event& event){
    if(event.type != EV_KEY || event.code != BTN_TOOL_PEN){
        return;
    }
    penActive = event.value;
    if(Oxide::debugEnabled()){
        O_INFO("Pen state: " << (penActive ? "Active" : "Inactive"));
    }
}

void SystemAPI::inhibitSleep(){
    inhibitors.append(Inhibitor(systemd, "sleep", qApp->applicationName(), "Handle sleep screen"));
}

void SystemAPI::inhibitPowerOff(){
    inhibitors.append(Inhibitor(systemd, "shutdown", qApp->applicationName(), "Block power off from any other method", true));
    rguard(true);
}

void SystemAPI::releaseSleepInhibitors(bool block){
    // TODO - Use STL style iterators https://doc.qt.io/qt-5/containers.html#stl-style-iterators
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

void SystemAPI::releasePowerOffInhibitors(bool block){
    // TODO - Use STL style iterators https://doc.qt.io/qt-5/containers.html#stl-style-iterators
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

void SystemAPI::rguard(bool install){
    QProcess::execute("/opt/bin/rguard", QStringList() << (install ? "-1" : "-0"));
}

TouchData* SystemAPI::getEvent(int slot){
    if(slot == -1){
        return nullptr;
    }
    if(!touches.contains(slot)){
        touches.insert(slot, new TouchData{
                           .slot = slot
                       });
    }
    return touches.value(slot);
}

int SystemAPI::getCurrentFingers(){
    return std::count_if(touches.begin(), touches.end(), [](TouchData* touch){
        return touch->active;
    });
}

void SystemAPI::touchDown(QList<TouchData*> touches){
    if(penActive){
        return;
    }
    if(Oxide::debugEnabled()){
        O_INFO("DOWN" << touches);
    }
    if(getCurrentFingers() != 1){
        return;
    }
    auto touch = touches.first();
    if(swipeDirection != None || touch->x == NULL_TOUCH_COORD || touch->y == NULL_TOUCH_COORD){
        return;
    }
    int offset = 20;
    if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM2){
        offset = 40;
    }
    if(touch->y <= offset){
        swipeDirection = Up;
    }else if(touch->y > (deviceSettings.getTouchHeight() - offset)){
        swipeDirection = Down;
    }else if(touch->x <= offset){
        if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM2){
            swipeDirection = Right;
        }else{
            swipeDirection = Left;
        }
    }else if(touch->x > (deviceSettings.getTouchWidth() - offset)){
        if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM2){
            swipeDirection = Left;
        }else{
            swipeDirection = Right;
        }
    }else{
        return;
    }
    if(Oxide::debugEnabled()){
        O_INFO("Swipe started" << swipeDirection);
    }
    startLocation = location = QPoint(touch->x, touch->y);
}

void SystemAPI::touchUp(QList<TouchData*> touches){
    if(Oxide::debugEnabled()){
        O_INFO("UP" << touches);
    }
    if(swipeDirection == None){
        if(Oxide::debugEnabled()){
            O_INFO("Not swiping");
        }
        for(auto touch : touches){
            writeTouchUp(touch);
        }
        return;
    }
    if(getCurrentFingers()){
        if(Oxide::debugEnabled()){
            O_INFO("Still swiping");
        }
        for(auto touch : touches){
            writeTouchUp(touch);
        }
        return;
    }
    if(touches.length() > 1){
        if(Oxide::debugEnabled()){
            O_INFO("Too many fingers");
        }
        for(auto touch : touches){
            writeTouchUp(touch);
        }
        swipeDirection = None;
        return;
    }
    auto touch = touches.first();
    if(touch->x == NULL_TOUCH_COORD || touch->y == NULL_TOUCH_COORD){
        if(Oxide::debugEnabled()){
            O_INFO("Invalid touch event");
        }
        swipeDirection = None;
        return;
    }
    if(swipeDirection == Up){
        if(!swipeStates[Up] || touch->y < location.y() || touch->y - startLocation.y() < swipeLengths[Up]){
            // Must end swiping up and having gone far enough
            cancelSwipe(touch);
            return;
        }
        emit bottomAction();
    }else if(swipeDirection == Down){
        if(!swipeStates[Down] || touch->y > location.y() || startLocation.y() - touch->y < swipeLengths[Down]){
            // Must end swiping down and having gone far enough
            cancelSwipe(touch);
            return;
        }
        emit topAction();
    }else if(swipeDirection == Right || swipeDirection == Left){
        auto isRM2 = deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM2;
        auto invalidLeft = !swipeStates[Left] || touch->x < location.x() || touch->x - startLocation.x() < swipeLengths[Left];
        auto invalidRight = !swipeStates[Right] || touch->x > location.x() || startLocation.x() - touch->x < swipeLengths[Right];
        if(swipeDirection == Right && (isRM2 ? invalidLeft : invalidRight)){
            // Must end swiping right and having gone far enough
            cancelSwipe(touch);
            return;
        }else if(swipeDirection == Left && (isRM2 ? invalidRight : invalidLeft)){
            // Must end swiping left and having gone far enough
            cancelSwipe(touch);
            return;
        }
        if(swipeDirection == Left){
            emit rightAction();
        }else{
            emit leftAction();
        }
    }
    swipeDirection = None;
    touch->x = -1;
    touch->y = -1;
    writeTouchUp(touch);
    if(Oxide::debugEnabled()){
        O_INFO("Swipe direction" << swipeDirection);
    }
}

void SystemAPI::touchMove(QList<TouchData*> touches){
    if(Oxide::debugEnabled()){
        O_INFO("MOVE" << touches);
    }
    if(swipeDirection == None){
        for(auto touch : touches){
            writeTouchMove(touch);
        }
        return;
    }
    if(touches.length() > 1){
        if(Oxide::debugEnabled()){
            O_INFO("Too many fingers");
        }
        for(auto touch : touches){
            writeTouchMove(touch);
        }
        swipeDirection = None;
        return;
    }
    auto touch = touches.first();
    if(touch->y > location.y()){
        location = QPoint(touch->x, touch->y);
    }
}

void SystemAPI::cancelSwipe(TouchData* touch){
    if(Oxide::debugEnabled()){
        O_INFO("Swipe Cancelled");
    }
    swipeDirection = None;
    writeTouchUp(touch);
}

void SystemAPI::writeTouchUp(TouchData* touch){
    writeTouchMove(touch);
    if(Oxide::debugEnabled()){
        O_INFO("Write touch up" << touch);
    }
    int size = sizeof(input_event) * 3;
    input_event* events = (input_event*)malloc(size);
    events[0] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_SLOT, touch->slot);
    events[1] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);
    events[2] = DigitizerHandler::createEvent(EV_SYN, 0, 0);
    touchHandler->write(events, size);
    free(events);
}

void SystemAPI::writeTouchMove(TouchData* touch){
    if(Oxide::debugEnabled()){
        O_INFO("Write touch move" << touch);
    }
    int count = 8;
    if(touch->x == NULL_TOUCH_COORD){
        count--;
    }
    if(touch->y == NULL_TOUCH_COORD){
        count--;
    }
    int size = sizeof(input_event) * count;
    input_event* events = (input_event*)malloc(size);
    events[2] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_SLOT, touch->slot);
    if(touch->x != NULL_TOUCH_COORD){
        events[2] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_POSITION_X, touch->x);
    }
    if(touch->y != NULL_TOUCH_COORD){
        events[2] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_POSITION_Y, touch->y);
    }
    events[2] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_PRESSURE, touch->pressure);
    events[2] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_TOUCH_MAJOR, touch->major);
    events[2] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_TOUCH_MINOR, touch->minor);
    events[2] = DigitizerHandler::createEvent(EV_ABS, ABS_MT_ORIENTATION, touch->orientation);
    events[2] = DigitizerHandler::createEvent(EV_SYN, 0, 0);
    touchHandler->write(events, size);
    free(events);
}

void SystemAPI::fn(){
    auto n = 512 * 8;
    auto num_inst = 4;
    input_event* ev = (input_event *)malloc(sizeof(struct input_event) * n * num_inst);
    memset(ev, 0, sizeof(input_event) * n * num_inst);
    auto i = 0;
    while (i < n) {
        ev[i++] = DigitizerHandler::createEvent(EV_ABS, ABS_DISTANCE, 1);
        ev[i++] = DigitizerHandler::createEvent(EV_SYN, 0, 0);
        ev[i++] = DigitizerHandler::createEvent(EV_ABS, ABS_DISTANCE, 2);
        ev[i++] = DigitizerHandler::createEvent(EV_SYN, 0, 0);
    }
}
void SystemAPI::toggleSwipes(){
    bool state = !swipeStates[Up];
    setSwipeEnabled(Left, state);
    setSwipeEnabled(Right, state);
    setSwipeEnabled(Up, state);
    QString message = state ? "Swipes Enabled" : "Swipes Disabled";
    O_INFO(message);
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

Inhibitor::Inhibitor(Manager* systemd, QString what, QString who, QString why, bool block)
    : who(who), what(what), why(why), block(block) {
    QDBusUnixFileDescriptor reply = systemd->Inhibit(what, who, why, block ? "block" : "delay");
    fd = reply.takeFileDescriptor();
}

void Inhibitor::release(){
    if(released()){
        return;
    }
    close(fd);
    fd = -1;
}

bool Inhibitor::released() { return fd == -1; }

string TouchData::debugString() const{
    return "<Touch " + to_string(id) + " (" + to_string(x) + ", " + to_string(y) + ") " + (active ? "pressed" : "released") + ">";
}
