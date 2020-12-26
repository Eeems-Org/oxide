#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <QDebug>
#include <QFile>
#include "devicesettings.h"

DeviceSettings::DeviceSettings(): _deviceType(DeviceType::RM1) {
    readDeviceType();
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

DeviceSettings::DeviceType DeviceSettings::getDeviceType() const {
    return _deviceType;
}

const char* DeviceSettings::getButtonsDevicePath() const {
    switch(getDeviceType()) {
        case DeviceType::RM1:
            return "/dev/input/event2";
        case DeviceType::RM2:
            return "/dev/input/event0";
        default:
            return "";
    }
}

const char* DeviceSettings::getWacomDevicePath() const {
    switch(getDeviceType()) {
        case DeviceType::RM1:
            return "/dev/input/event0";
        case DeviceType::RM2:
            return "/dev/input/event1";
        default:
            return "";
    }
}

const char* DeviceSettings::getTouchDevicePath() const {
    switch(getDeviceType()) {
        case DeviceType::RM1:
            return "/dev/input/event1";
        case DeviceType::RM2:
            return "/dev/input/event2";
        default:
            return "";
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
