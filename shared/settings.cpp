#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <QDebug>
#include "settings.h"

char* readFile(const char* path) {
    FILE *file = fopen(path, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        long length=ftell(file);
        fseek (file, 0, SEEK_SET);
        char *buffer = (char*)malloc(length+1);
        if (buffer) {
            fread (buffer, 1, length, file);
            buffer[length] = '\0';
        }
        fclose(file);
        return buffer;
    }
    return nullptr;
}

bool doesStartWithPrefixes(const char* const prefixes[], char* toCheck) {
    int length = strlen(toCheck);
    for (int i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        int prefixLength = strlen(prefixes[i]);
        if (prefixLength < length && strncmp(prefixes[i], toCheck, prefixLength) == 0) {
            return true;
        }
    }
    return false;
}

void Settings::readDeviceType() {
    char* device_type = readFile("/sys/devices/soc0/machine");
    _deviceType = DeviceType::UNKNOWN;

    if (device_type) {

        const char* const remarkable1[] {
            "reMarkable 1",
            "reMarkable Prototype 1",
        };
        const char* const remarkable2[] {
            "reMarkable 2"
        };

        if (doesStartWithPrefixes(remarkable1, device_type)) {
            _deviceType = DeviceType::RM1;
            qDebug() << "RM1 detected...";
        }
        else if (doesStartWithPrefixes(remarkable2, device_type)) {
            _deviceType = DeviceType::RM2;
            qDebug() << "RM2 detected...";

        }
        free(device_type);
    }
}

DeviceType Settings::getDeviceType() {
    if (_deviceType == DeviceType::UNDETERMINED) {
        readDeviceType();
    }
    return _deviceType;
}

const char* Settings::getButtonsDevicePath() {
    switch(getDeviceType()) {
    case DeviceType::RM1:
        return "/dev/input/event2";
    case DeviceType::RM2:
        return "/dev/input/event0";
    default:
        return "";
    }
}

const char* Settings::getWacomDevicePath() {
    switch(getDeviceType()) {
    case DeviceType::RM1:
        return "/dev/input/event0";
    case DeviceType::RM2:
        return "/dev/input/event1";
    default:
        return "";
    }
}

const char* Settings::getTouchDevicePath() {
    switch(getDeviceType()) {
    case DeviceType::RM1:
        return "/dev/input/event1";
    case DeviceType::RM2:
        return "/dev/input/event2";
    default:
        return "";
    }
}
