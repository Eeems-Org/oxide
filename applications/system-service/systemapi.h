#ifndef SYSTEMAPI_H
#define SYSTEMAPI_H

#include <QObject>
#include <QMetaType>
#include <QMutableListIterator>
#include <QTimer>
#include <QMutex>
#include <liboxide.h>

#include "apibase.h"
#include "buttonhandler.h"
#include "application.h"
#include "screenapi.h"
#include "digitizerhandler.h"
#include "eventlistener.h"
#include "login1_interface.h"

#define systemAPI SystemAPI::singleton()

typedef org::freedesktop::login1::Manager Manager;

struct Inhibitor {
    int fd;
    QString who;
    QString what;
    QString why;
    bool block;
    Inhibitor(Manager* systemd, QString what, QString who, QString why, bool block = false);
    void release();
    bool released();
};

#define NULL_TOUCH_COORD -1

struct Touch {
    int slot = 0;
    int id = -1;
    int x = NULL_TOUCH_COORD;
    int y = NULL_TOUCH_COORD;
    bool active = false;
    bool existing = false;
    bool modified = true;
    int pressure = 0;
    int major = 0;
    int minor = 0;
    int orientation = 0;
    inline string debugString() const{
        return "<Touch " + to_string(id) + " (" + to_string(x) + ", " + to_string(y) + ") " + (active ? "pressed" : "released") + ">";
    }
};
QDebug operator<<(QDebug debug, const Touch& touch);
QDebug operator<<(QDebug debug, Touch* touch);
Q_DECLARE_METATYPE(Touch)
Q_DECLARE_METATYPE(input_event)

class SystemAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_SYSTEM_INTERFACE)
    Q_PROPERTY(int autoSleep READ autoSleep WRITE setAutoSleep NOTIFY autoSleepChanged)
    Q_PROPERTY(int autoLock READ autoLock WRITE setAutoLock NOTIFY autoLockChanged)
    Q_PROPERTY(bool lockOnSuspend READ lockOnSuspend WRITE setLockOnSuspend NOTIFY lockOnSuspendChanged)
    Q_PROPERTY(bool sleepInhibited READ sleepInhibited NOTIFY sleepInhibitedChanged)
    Q_PROPERTY(bool powerOffInhibited READ powerOffInhibited NOTIFY powerOffInhibitedChanged)
    Q_PROPERTY(bool landscape READ landscape NOTIFY landscapeChanged)

public:
    enum SwipeDirection { None, Right, Left, Up, Down };
    Q_ENUM(SwipeDirection)
    static SystemAPI* singleton(SystemAPI* self = nullptr);
    SystemAPI(QObject* parent);
    ~SystemAPI();
    void setEnabled(bool enabled);
    int autoSleep();
    void setAutoSleep(int _autoSleep);
    int autoLock();
    void setAutoLock(int _autoLock);
    bool lockOnSuspend();
    void setLockOnSuspend(bool _lockOnSuspend);
    bool sleepInhibited();
    bool powerOffInhibited();
    bool landscape();
    void uninhibitAll(QString name);
    void stopSuspendTimer();
    void stopLockTimer();
    void startSuspendTimer();
    void startLockTimer();
    void lock();
    void unlock();
    Q_INVOKABLE void setSwipeEnabled(int direction, bool enabled);
    void setSwipeEnabled(SwipeDirection direction, bool enabled);
    Q_INVOKABLE bool getSwipeEnabled(int direction);
    bool getSwipeEnabled(SwipeDirection direction);
    Q_INVOKABLE void toggleSwipeEnabled(int direction);
    void toggleSwipeEnabled(SwipeDirection direction);
    Q_INVOKABLE void setSwipeLength(int direction, int length);
    void setSwipeLength(SwipeDirection direction, int length);
    Q_INVOKABLE int getSwipeLength(int direction);
    int getSwipeLength(SwipeDirection direction);

public slots:
    void suspend();
    void powerOff();
    void reboot();
    void activity();
    void inhibitSleep(QDBusMessage message);
    void uninhibitSleep(QDBusMessage message);
    void inhibitPowerOff(QDBusMessage message);
    void uninhibitPowerOff(QDBusMessage message);
    void toggleSwipes();

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
    void autoLockChanged(int);
    void lockOnSuspendChanged(bool);
    void swipeLengthChanged(int, int);
    void swipeEnabledChanged(int, bool);
    void landscapeChanged(bool);
    void deviceSuspending();
    void deviceResuming();

private slots:
    void PrepareForSleep(bool suspending);
    void suspendTimeout();
    void lockTimeout();

private:
    Manager* systemd;
    QList<Inhibitor> inhibitors;
    Application* resumeApp;
    qint64 lockTimestamp = 0;
    QTimer suspendTimer;
    QTimer lockTimer;
    QSettings settings;
    QStringList sleepInhibitors;
    QStringList powerOffInhibitors;
    QMutex mutex;
    bool wifiWasOn = false;
    QMap<SwipeDirection, bool> swipeStates;
    QMap<SwipeDirection, int> swipeLengths;
    Blight::shared_buf_t m_buffer = nullptr;

    void inhibitSleep();
    void inhibitPowerOff();
    void releaseSleepInhibitors(bool block = false);
    void releasePowerOffInhibitors(bool block = false);
    void rguard(bool install);

    void fn();
};
#endif // SYSTEMAPI_H
