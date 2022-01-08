#ifndef DEVICESETTINGS_H
#define DEVICESETTINGS_H

#include <string>
#include "sentry_settings.h"

#define deviceSettings DeviceSettings::instance()

class DeviceSettings{
public:
    enum DeviceType { Unknown, RM1, RM2 };
    static DeviceSettings& instance() {
        static DeviceSettings INSTANCE;
        return INSTANCE;
    }
    const char* getButtonsDevicePath() const;
    const char* getWacomDevicePath() const;
    const char* getTouchDevicePath() const;
    const char* getTouchEnvSetting() const;
    DeviceType getDeviceType() const;
    int getTouchWidth() const;
    int getTouchHeight() const;

private:
    DeviceType _deviceType;

    DeviceSettings();
    ~DeviceSettings() {};
    void readDeviceType();
    bool checkBitSet(int fd, int type, int i);
    std::string buttonsPath = "";
    std::string wacomPath = "";
    std::string touchPath = "";
};

#endif // DEVICESETTINGS_H
