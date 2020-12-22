#pragma once

enum DeviceType {
    Unknown,
    RM1,
    RM2,
};

class DeviceSettings{
private:
    DeviceType _deviceType;

    DeviceSettings();
    ~DeviceSettings() {};
    void readDeviceType();
public:

    static DeviceSettings& instance() {
        static DeviceSettings INSTANCE;
        return INSTANCE;
    }
    const char* getButtonsDevicePath() const;
    const char* getWacomDevicePath() const;
    const char* getTouchDevicePath() const;
    const char* getTouchEnvSetting() const;
    int getFrameBufferWidth() const;
    int getFrameBufferHeight() const;
    int getFrameBufferSize() const;
    DeviceType getDeviceType() const;

};
