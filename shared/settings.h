#pragma once

enum DeviceType {
    RM1,
    RM2,
};

class Settings {
private:
    DeviceType _deviceType;

    Settings();
    ~Settings() {};
    void readDeviceType();
public:

    static Settings& instance() {
        static Settings INSTANCE;
        return INSTANCE;
    }
    const char* getButtonsDevicePath() const;
    const char* getWacomDevicePath() const;
    const char* getTouchDevicePath() const;
    const char* getTouchEnvSetting() const;
    DeviceType getDeviceType() const;

};
