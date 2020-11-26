#pragma once

enum DeviceType {
    RM1,
    RM2,
    UNDETERMINED,
    UNKNOWN
};

class Settings {
private:
    DeviceType _deviceType;

    Settings(): _deviceType(DeviceType::UNDETERMINED) {}
    ~Settings() {};
    void readDeviceType();
public:

    static Settings& instance() {
        static Settings INSTANCE;
        return INSTANCE;
    }
    const char* getButtonsDevicePath();
    const char* getWacomDevicePath();
    const char* getTouchDevicePath();
    DeviceType getDeviceType();

};
