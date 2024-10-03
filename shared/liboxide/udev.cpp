#include "udev.h"
#include "debug.h"
#include "liboxide.h"

#include <fcntl.h>
#include <cerrno>

#include <QtConcurrent/QtConcurrentRun>
#include <QThread>

namespace Oxide {

    UDev* UDev::singleton(){
        static UDev* instance = nullptr;
        static std::once_flag initFlag;
        std::call_once(initFlag, [](){
           instance = new UDev();
           instance->start();
        });
        return instance;
    }

    UDev::UDev() : QObject(), _thread(this){
        qRegisterMetaType<Device>("UDev::Device");
        udevLib = udev_new();
        connect(&_thread, &QThread::started, [this]{
            O_DEBUG("UDev::Thread started");
        });
        connect(&_thread, &QThread::finished, [this]{
            O_DEBUG("UDev::Thread finished");
        });
        _thread.start(QThread::LowPriority);
        moveToThread(&_thread);
    }

    UDev::~UDev(){
        if(udevLib != nullptr){
            udev_unref(udevLib);
            udevLib = nullptr;
        }
    }

    void UDev::subsystem(const QString& subsystem, std::function<void(const Device&)> callback){
        deviceType(subsystem, "", callback);
    }

    void UDev::subsystem(const QString& subsystem, std::function<void()> callback){
        deviceType(subsystem, "", callback);
    }

    void UDev::deviceType(const QString& subsystem, const QString& deviceType, std::function<void(const Device&)> callback){
        connect(singleton(), &UDev::event, [callback, subsystem, deviceType](const Device& device){
            if(
                device.subsystem == subsystem
                && (
                    deviceType.isNull()
                    || deviceType.isEmpty()
                    || device.deviceType == deviceType
                )
            ){
                callback(device);
            }
        });
        singleton()->addMonitor(subsystem, deviceType);
    }

    void UDev::deviceType(const QString& subsystem, const QString& deviceType, std::function<void()> callback){
        UDev::deviceType(subsystem, deviceType, [callback](const Device& device){
            Q_UNUSED(device);
            callback();
        });
    }

    void UDev::start(){
        statelock.lock();
        O_DEBUG("UDev::Starting...");
        exitRequested = false;
        if(running){
            statelock.unlock();
            O_DEBUG("UDev::Already running");
            return;
        }
        QTimer::singleShot(0, [this](){
            monitor();
            statelock.unlock();
            O_DEBUG("UDev::Started");
        });
    }

    void UDev::stop(){
        statelock.lock();
        O_DEBUG("UDev::Stopping...");
        if(running){
            exitRequested = true;
        }
        statelock.unlock();
    }

    bool UDev::isRunning(){ return running; }

    void UDev::wait(){
        if(isRunning()){
            O_DEBUG("UDev::Waiting to stop...");
            QEventLoop loop;
            connect(this, &UDev::stopped, &loop, &QEventLoop::quit);
            loop.exec();
        }
    }

    void UDev::addMonitor(QString subsystem, QString deviceType){
        O_DEBUG("UDev::Adding" << subsystem << deviceType);
        auto& list = monitors[subsystem];
        if (!list) {
            list = QSharedPointer<QStringList>::create();
        }
        if(!list->contains(deviceType)){
            list->append(deviceType);
            update = true;
        }
    }
    void UDev::removeMonitor(QString subsystem, QString deviceType){
        O_DEBUG("UDev::Removing" << subsystem << deviceType);
        if(monitors.contains(subsystem)){
            monitors[subsystem]->removeAll(deviceType);
            update = true;
        }

        auto it = monitors.find(subsystem);
        if(it != monitors.end() && *it){
            (*it)->removeAll(deviceType);
            update = true;
        }
    }

    QList<UDev::Device> UDev::getDeviceList(const QString& subsystem){
        QList<Device> deviceList;
        struct udev_enumerate* udevEnumeration = udev_enumerate_new(udevLib);
        if(udevEnumeration == nullptr){
            static std::once_flag onceFlag;
            std::call_once(onceFlag, [](){
                O_WARNING("Failed to enumerate udev");
            });
            return deviceList;
        }
        if(udev_enumerate_add_match_subsystem(udevEnumeration, subsystem.toUtf8().constData()) < 0){
            O_WARNING("Failed to add subsystem");
            udev_enumerate_unref(udevEnumeration);
            return deviceList;
        }
        if(udev_enumerate_scan_devices(udevEnumeration) < 0){
            O_WARNING("Failed to scan devices");
            udev_enumerate_unref(udevEnumeration);
            return deviceList;
        }
        struct udev_list_entry* udevDeviceList  = udev_enumerate_get_list_entry(udevEnumeration);
        if(udevDeviceList != nullptr){
            struct udev_list_entry* entry = nullptr;
            udev_list_entry_foreach(entry, udevDeviceList){
                if(entry == nullptr){
                    continue;
                }
                const char* path = udev_list_entry_get_name(entry);
                if(path == nullptr){
                    continue;
                }
                Device device;
                struct udev_device* udevDevice = udev_device_new_from_syspath(udevLib, path);
                if(udevDevice == nullptr) {
                    O_WARNING("Failed to create udev device from syspath");
                    continue;
                }
                device.action = getActionType(udevDevice);
                device.path = path;
                device.subsystem = subsystem;
                auto devType = udev_device_get_devtype(udevDevice);
                device.deviceType = QString(devType ? devType : "");
                udev_device_unref(udevDevice);
                deviceList.append(device);
            }
        }
        udev_enumerate_unref(udevEnumeration);
        return deviceList;
    }

    UDev::ActionType UDev::getActionType(udev_device* udevDevice){
        if(udevDevice == nullptr){
            return Unknown;
        }
        return getActionType(QString(udev_device_get_action(udevDevice)).trimmed().toUpper());
    }

    UDev::ActionType UDev::getActionType(const QString& actionType){
        if(actionType == "ADD"){
            return Add;
        }
        if(actionType == "REMOVE"){
            return Remove;
        }
        if(actionType == "CHANGE"){
            return Change;
        }
        if(actionType == "OFFLINE"){
            return Offline;
        }
        if(actionType == "ONLINE"){
            return Online;
        }
        return Unknown;
    }

    void UDev::monitor(){
        running = true;
        O_DEBUG("UDev::Monitor starting...");
        udev_monitor* mon = udev_monitor_new_from_netlink(udevLib, "udev");
        if(!mon){
            O_WARNING("UDev::Monitor Unable to listen to UDev: Failed to create netlink monitor");
            O_DEBUG(strerror(errno))
            return;
        }
        O_DEBUG("UDev::Monitor applying filters...");
        for(QString subsystem : monitors.keys()){
            for(QString deviceType : *monitors[subsystem]){
                O_DEBUG("UDev::Monitor filter" << subsystem << deviceType);
                int err = udev_monitor_filter_add_match_subsystem_devtype(
                    mon,
                    subsystem.toUtf8().constData(),
                    deviceType.isNull() || deviceType.isEmpty()
                        ? NULL
                        : deviceType.toUtf8().constData()
                );
                if(err < 0){
                    O_WARNING("UDev::Monitor Unable to add filter: " << strerror(err));
                }
            }
        }
        O_DEBUG("UDev::Monitor enabling...");
        int err = udev_monitor_enable_receiving(mon);
        if(err < 0){
            O_WARNING("UDev::Monitor Unable to listen to UDev:" << strerror(err));
            udev_monitor_unref(mon);
            return;
        }
        O_DEBUG("UDev::Monitor setting up timer...");
        auto timer = new QTimer();
        timer->setTimerType(Qt::PreciseTimer);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, [this, mon, timer]{
            if(exitRequested){
                O_DEBUG("UDev::Monitor stopping...");
                udev_monitor_unref(mon);
                timer->deleteLater();
                running = false;
                O_DEBUG("UDev::Stopped");
                emit stopped();
                return;
            }
            if(update || !mon){
                O_DEBUG("UDev::Monitor reloading...");
                update = false;
                udev_monitor_unref(mon);
                timer->deleteLater();
                QTimer::singleShot(0, this, &UDev::monitor);
                return;
            }
            udev_device* dev = udev_monitor_receive_device(mon);
            if(dev != nullptr){
                Device device;
                device.action = getActionType(dev);
                auto devNode = udev_device_get_devnode(dev);
                device.path = QString(devNode ? devNode : "");
                auto devSubsystem = udev_device_get_subsystem(dev);
                device.subsystem = QString(devSubsystem ? devSubsystem : "");
                auto devType = udev_device_get_devtype(dev);
                device.deviceType = QString(devType ? devType : "");
                udev_device_unref(dev);
                O_DEBUG("UDev::Monitor UDev event" << device);
                emit event(device);
            }else if(errno && errno != EAGAIN){
                O_WARNING("UDev::Monitor error checking event:" << strerror(errno));
            }
            timer->start(30);
        });
        timer->start(30);
        O_DEBUG("UDev::Monitor event loop started");
    }

    QDebug operator<<(QDebug debug, const UDev::Device& device){
        QDebugStateSaver saver(debug);
        Q_UNUSED(saver)
        debug.nospace() << device.debugString().c_str();
        return debug.maybeSpace();
    }
}
