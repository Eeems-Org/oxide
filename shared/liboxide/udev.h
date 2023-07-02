/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <libudev.h>

#include <QObject>

using namespace std;

namespace Oxide {
    class UDev : public QObject {
        Q_OBJECT

    public:
        static UDev* singleton();
        explicit UDev();
        ~UDev();

        enum ActionType {
            Add,
            Remove,
            Change,
            Online,
            Offline,
            Unknown
        };
        struct Device {
            QString subsystem;
            QString deviceType;
            QString path;
            ActionType action = Unknown;
            QString actionString() const {
                switch(action){
                case Add:
                    return "ADD";
                case Remove:
                    return "REMOVE";
                case Change:
                    return "CHANGE";
                case Online:
                    return "ONLINE";
                case Offline:
                    return "OFFLINE";
                case Unknown:
                default:
                    return "UNKNOWN";
                }
            }
            string debugString() const {
                return QString("<Device %1/%2 %3>").arg(subsystem, deviceType, actionString()).toStdString();
            }
        };
        static void subsystem(const QString& subsystem, std::function<void(const Device&)> callback);
        static void subsystem(const QString& subsystem, std::function<void()> callback);
        static void deviceType(const QString& subsystem, const QString& deviceType, std::function<void(const Device&)> callback);
        static void deviceType(const QString& subsystem, const QString& deviceType, std::function<void()> callback);
        void start();
        void stop();
        bool isRunning();
        void wait();
        void addMonitor(QString subsystem, QString deviceType);
        void removeMonitor(QString subsystem, QString deviceType);
        QList<Device> getDeviceList(const QString& subsystem);
        ActionType getActionType(udev_device* udevDevice);
        ActionType getActionType(const QString& actionType);

    signals:
        void event(const Device& device);

    private:
        struct udev* udevLib = nullptr;
        bool running = false;
        QMap<QString, QStringList*> monitors;
        QThread _thread;

    protected:
        void monitor();
    };
    QDebug operator<<(QDebug debug, const UDev::Device& device);
}
Q_DECLARE_METATYPE(Oxide::UDev::Device)
