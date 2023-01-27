#include "liboxide.h"

#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>

#include <linux/input.h>


#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

namespace Oxide {
    void dispatchToMainThread(std::function<void()> callback){
        if(QThread::currentThread() == qApp->thread()){
            callback();
            return;
        }
        // any thread
        QTimer* timer = new QTimer();
        timer->moveToThread(qApp->thread());
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, [=](){
            // main thread
            callback();
            timer->deleteLater();
        });
        QMetaObject::invokeMethod(timer, "start", Qt::BlockingQueuedConnection, Q_ARG(int, 0));
    }
    DeviceSettings& DeviceSettings::instance() {
        static DeviceSettings INSTANCE;
        return INSTANCE;
    }
    DeviceSettings::DeviceSettings(): _deviceType(DeviceType::RM1) {
        readDeviceType();

        O_DEBUG("Looking for input devices...");
        QDir dir("/dev/input");
        for(auto path : dir.entryList(QDir::Files | QDir::NoSymLinks | QDir::System)){
            O_DEBUG(("  Checking " + path + "...").toStdString().c_str());
            QString fullPath(dir.path() + "/" + path);
            QFile device(fullPath);
            device.open(QIODevice::ReadOnly);
            int fd = device.handle();
            int version;
            if (ioctl(fd, EVIOCGVERSION, &version)){
                O_DEBUG("    Invalid");
                continue;
            }
            unsigned long bit[EV_MAX];
            ioctl(fd, EVIOCGBIT(0, EV_MAX), bit);
            if (test_bit(EV_KEY, bit)) {
                if (checkBitSet(fd, EV_KEY, BTN_STYLUS) && test_bit(EV_ABS, bit)) {
                    O_DEBUG("    Wacom input device detected");
                    wacomPath = fullPath.toStdString();
                    continue;
                }
                if (checkBitSet(fd, EV_KEY, KEY_POWER)) {
                    O_DEBUG("    Buttons input device detected");
                    buttonsPath = fullPath.toStdString();
                    continue;
                }
            }
            if (checkBitSet(fd, EV_ABS, ABS_MT_SLOT)) {
                O_DEBUG("    Touch input device detected");
                touchPath = fullPath.toStdString();
                continue;
            }
            O_DEBUG("    Invalid");
        }
        if (wacomPath.empty()) {
            O_WARNING("Wacom input device not found");
        }
        if (touchPath.empty()) {
            O_WARNING("Touch input device not found");
        }
        if (buttonsPath.empty()){
            O_WARNING("Buttons input device not found");
        }
    }
    DeviceSettings::~DeviceSettings(){}
    bool DeviceSettings::checkBitSet(int fd, int type, int i) {
        unsigned long bit[NBITS(KEY_MAX)];
        ioctl(fd, EVIOCGBIT(type, KEY_MAX), bit);
        return test_bit(i, bit);
    }

    void DeviceSettings::readDeviceType() {
        QFile file("/sys/devices/soc0/machine");
        if(!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)){
            O_DEBUG("Couldn't open " << file.fileName());
            _deviceType = DeviceType::Unknown;
            return;
        }
        QTextStream in(&file);
        QString modelName = in.readLine();
        if (modelName.startsWith("reMarkable 2")) {
            O_DEBUG("RM2 detected...");
            _deviceType = DeviceType::RM2;
            return;
         }
         O_DEBUG("RM1 detected...");
         _deviceType = DeviceType::RM1;
    }

    DeviceSettings::DeviceType DeviceSettings::getDeviceType() const { return _deviceType; }

    const char* DeviceSettings::getButtonsDevicePath() const { return buttonsPath.c_str(); }

    const char* DeviceSettings::getWacomDevicePath() const { return wacomPath.c_str(); }

    const char* DeviceSettings::getTouchDevicePath() const { return touchPath.c_str(); }
    const char* DeviceSettings::getDeviceName() const {
        switch(getDeviceType()){
            case DeviceType::RM1:
                return "reMarkable 1";
            case DeviceType::RM2:
                return "reMarkable 2";
            default:
                return "Unknown";
        }
    }

    const char* DeviceSettings::getTouchEnvSetting() const {
        switch(getDeviceType()) {
            case DeviceType::RM1:
                return "rotate=180";
            case DeviceType::RM2:
                return "rotate=180:invertx";
            default:
                return "";
        }
    }

    int DeviceSettings::getTouchWidth() const {
        switch(getDeviceType()) {
            case DeviceType::RM1:
                return 767;
            case DeviceType::RM2:
                return 1403;
            default:
                return 0;
        }
    }
    int DeviceSettings::getTouchHeight() const {
        switch(getDeviceType()) {
            case DeviceType::RM1:
                return 1023;
            case DeviceType::RM2:
                return 1871;
            default:
                return 0;
        }
    }
    WifiNetworks XochitlSettings::wifinetworks(){
        beginGroup("wifinetworks");
        QMap<QString, QVariantMap> wifinetworks;
        for(const QString& key : allKeys()){
            QVariantMap network = value(key).toMap();
            wifinetworks[key] = network;
        }
        endGroup();
        return wifinetworks;
    }
    void XochitlSettings::setWifinetworks(const WifiNetworks& wifinetworks){
        beginGroup("wifinetworks");
        for(const QString& key : wifinetworks.keys()){
            setValue(key, wifinetworks.value(key));
        }
        endGroup();
        sync();
    }
    QVariantMap XochitlSettings::getWifiNetwork(const QString& name){
        beginGroup("wifinetworks");
        QVariantMap network = value(name).toMap();
        endGroup();
        return network;
    }
    void XochitlSettings::setWifiNetwork(const QString& name, QVariantMap properties){
        beginGroup("wifinetworks");
        setValue(name, properties);
        endGroup();
        sync();
    }
    void XochitlSettings::resetWifinetworks(){}
    XochitlSettings::~XochitlSettings(){}
    SharedSettings::~SharedSettings(){}
    O_SETTINGS_PROPERTY_BODY(XochitlSettings, QString, General, passcode)
    O_SETTINGS_PROPERTY_BODY(XochitlSettings, bool, General, wifion)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, version)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, firstLaunch, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, telemetry, false)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, applicationUsage, false)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, crashReport, true)
}
