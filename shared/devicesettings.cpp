#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <QDebug>
#include <QDir>
#include <QFile>

#include <linux/input.h>

#include "devicesettings.h"

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

DeviceSettings::DeviceSettings(): _deviceType(DeviceType::RM1) {
    readDeviceType();

    QDir dir("/dev/input");
    qDebug() << "Looking for input devices...";
    for(auto path : dir.entryList(QDir::Files | QDir::NoSymLinks | QDir::System)){
        qDebug() << ("  Checking " + path + "...").toStdString().c_str();
        QString fullPath(dir.path() + "/" + path);
        QFile device(fullPath);
        device.open(QIODevice::ReadOnly);
        int fd = device.handle();
        int version;
        if (ioctl(fd, EVIOCGVERSION, &version)){
            qDebug() << "    Invalid";
            continue;
        }
        unsigned long bit[EV_MAX];
        ioctl(fd, EVIOCGBIT(0, EV_MAX), bit);
        if (test_bit(EV_KEY, bit)) {
            if (checkBitSet(fd, EV_KEY, BTN_STYLUS) && test_bit(EV_ABS, bit)) {
                qDebug() << "    Wacom input device detected";
                wacomPath = fullPath.toStdString().c_str();
                continue;
            }
            if (checkBitSet(fd, EV_KEY, KEY_POWER)) {
                qDebug() << "    Buttons input device detected";
                buttonsPath = fullPath.toStdString().c_str();
                continue;
            }
        }
        if(test_bit(EV_REL, bit) && checkBitSet(fd, EV_ABS, ABS_MT_SLOT)) {
            qDebug() << "    Touch input device detected";
            touchPath = fullPath.toStdString().c_str();
            continue;
        }
        qDebug() << "    Invalid";
    }
    if (wacomPath == nullptr) {
        qWarning() << "Wacom input device not found";
        wacomPath = "";
    }
    if (touchPath == nullptr) {
        qWarning() << "Touch input device not found";
        touchPath = "";
    }
    if (buttonsPath == nullptr){
        qWarning() << "Buttons input device not found";
        buttonsPath = "";
    }
}
bool DeviceSettings::checkBitSet(int fd, int type, int i) {
    unsigned long bit[NBITS(KEY_MAX)];
    ioctl(fd, EVIOCGBIT(type, KEY_MAX), bit);
    return test_bit(i, bit);
}

void DeviceSettings::readDeviceType() {
    QFile file("/sys/devices/soc0/machine");
    if(!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)){
        qDebug() << "Couldn't open " << file.fileName();
        _deviceType = DeviceType::Unknown;
        return;
    }
    QTextStream in(&file);
    QString modelName = in.readLine();
    if (modelName.startsWith("reMarkable 2")) {
        qDebug() << "RM2 detected...";
        _deviceType = DeviceType::RM2;
        return;
     }
     qDebug() << "RM1 detected...";
     _deviceType = DeviceType::RM1;
}

DeviceSettings::DeviceType DeviceSettings::getDeviceType() const { return _deviceType; }

const char* DeviceSettings::getButtonsDevicePath() const { return buttonsPath; }

const char* DeviceSettings::getWacomDevicePath() const { return wacomPath; }

const char* DeviceSettings::getTouchDevicePath() const { return touchPath; }

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
