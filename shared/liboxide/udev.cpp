#include "udev.h"
#include "debug.h"

#include <fcntl.h>
#include <cerrno>

#include <QtConcurrent/QtConcurrentRun>
#include <QThread>

namespace Oxide {

    UDev::UDev(QObject *parent) : QObject(), _thread(this){
        qRegisterMetaType<Device>("UDev::Device");
        udevLib = udev_new();
        connect(parent, &QObject::destroyed, this, &QObject::deleteLater);
        connect(&_thread, &QThread::started, this, &UDev::run);
        connect(&_thread, &QThread::finished, [this]{
            running = false;
        });
        moveToThread(&_thread);
    }

    UDev::~UDev(){
        if(udevLib != nullptr){
            udev_unref(udevLib);
            udevLib = nullptr;
        }
    }

    void UDev::start(){
        if(running){
            return;
        }
        wait();
        qDebug() << "UDev::Starting...";
        running = true;
        _thread.start();
        qDebug() << "UDev::Started";
    }

    void UDev::stop(){
        qDebug() << "UDev::Stopping...";
        if(running){
            running = false;
            wait();
        }
        qDebug() << "UDev::Stopped";
    }

    bool UDev::isRunning(){ return running || _thread.isRunning(); }

    void UDev::wait(){
        qDebug() << "UDev::Waiting...";
        while(isRunning()){
            thread()->yieldCurrentThread();
        }
        qDebug() << "UDev::Waited";
    }

    void UDev::addMonitor(QString subsystem, QString deviceType){
        QStringList* list;
        if(monitors.contains(subsystem)){
            list = monitors[subsystem];
        }else{
            list = new QStringList();
            monitors.insert(subsystem, list);
        }
        if(!list->contains(deviceType)){
            list->append(deviceType);
        }
    }
    void UDev::removeMonitor(QString subsystem, QString deviceType){
        if(monitors.contains(subsystem)){
            monitors[subsystem]->removeAll(deviceType);
        }
    }

    QList<UDev::Device> UDev::getDeviceList(const QString& subsystem){
        QList<Device> deviceList;
        struct udev_enumerate* udevEnumeration = udev_enumerate_new(udevLib);
        if(udevEnumeration == nullptr){
            return deviceList;
        }
        udev_enumerate_add_match_subsystem(udevEnumeration, subsystem.toUtf8().constData());
        udev_enumerate_scan_devices(udevEnumeration);
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
                device.action = getActionType(udevDevice);
                device.path = path;
                device.subsystem = subsystem;
                device.deviceType = QString(udev_device_get_devtype(udevDevice));
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
        if(actionType == "Change"){
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

    void UDev::run(){
        O_DEBUG("UDev::Monitor starting");
        udev_monitor* mon = udev_monitor_new_from_netlink(udevLib, "udev");
        if(!mon){
            O_WARNING("UDev::Monitor Unable to listen to UDev: Failed to create netlink monitor");
            O_DEBUG(strerror(errno))
            return;
        }
        for(QString subsystem : monitors.keys()){
            for(QString deviceType : *monitors[subsystem]){
                O_DEBUG("UDev::Monitor " << subsystem << deviceType);
                int err = udev_monitor_filter_add_match_subsystem_devtype(
                    mon,
                    subsystem.toUtf8().constData(),
                    deviceType.isNull() || deviceType.isEmpty() ? deviceType.toUtf8().constData() : NULL
                );
                if(err < 0){
                    O_WARNING("UDev::Monitor Unable to add filter: " << strerror(err));
                }
            }
        }
        int err = udev_monitor_enable_receiving(mon);
        if(err < 0){
            O_WARNING("UDev::Monitor Unable to listen to UDev:" << strerror(err));
            udev_monitor_unref(mon);
            return;
        }
        while(running){
            udev_device* dev = udev_monitor_receive_device(mon);
            if(dev != nullptr){
                Device device;
                device.action = getActionType(dev);
                device.path = QString(udev_device_get_devnode(dev));
                device.subsystem = QString(udev_device_get_subsystem(dev));
                device.deviceType = QString(udev_device_get_devtype(dev));
                udev_device_unref(dev);
                O_DEBUG("UDev::Monitor UDev event" << device);
                emit event(device);
            }else if(errno && errno != EAGAIN){
                O_WARNING("UDev::Monitor error checking event:" << strerror(errno));
            }
            auto timestamp = QDateTime::currentMSecsSinceEpoch();
            QThread::yieldCurrentThread();
            if(QDateTime::currentMSecsSinceEpoch() - timestamp < 30){
                QThread::msleep(30);
            }
        }
        O_DEBUG("UDev::Monitor stopping");
        udev_monitor_unref(mon);
    }
    QDebug operator<<(QDebug debug, const UDev::Device& device){
        QDebugStateSaver saver(debug);
        Q_UNUSED(saver)
        debug.nospace() << device.debugString().c_str();
        return debug.maybeSpace();
    }
}
