#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <QDebug>
#include <QFile>
#include "settings.h"

Settings::Settings(): _deviceType(DeviceType::RM1) {
    readDeviceType();
}

void Settings::readDeviceType() {
    QFile file("/sys/devices/soc0/machine");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        qDebug() << "Couldn't open " << file.fileName();
        return;
    }
    QTextStream in(&file);
    QString modelName = in.readLine();
    if (modelName.startsWith("reMarkable 2")) {
        qDebug() << "RM2 detected...";
        _deviceType = DeviceType::RM2;
     }
}

DeviceType Settings::getDeviceType() const {
    return _deviceType;
}

const char* Settings::getButtonsDevicePath() const {
    switch(getDeviceType()) {
    case DeviceType::RM1:
        return "/dev/input/event2";
    case DeviceType::RM2:
        return "/dev/input/event0";
    default:
        return "";
    }
}

const char* Settings::getWacomDevicePath() const {
    switch(getDeviceType()) {
    case DeviceType::RM1:
        return "/dev/input/event0";
    case DeviceType::RM2:
        return "/dev/input/event1";
    default:
        return "";
    }
}

const char* Settings::getTouchDevicePath() const {
    switch(getDeviceType()) {
    case DeviceType::RM1:
        return "/dev/input/event1";
    case DeviceType::RM2:
        return "/dev/input/event2";
    default:
        return "";
    }
}
