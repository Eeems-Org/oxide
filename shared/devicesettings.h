#ifndef DEVICESETTINGS_H
#define DEVICESETTINGS_H

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

private:
    DeviceType _deviceType;

    DeviceSettings();
    ~DeviceSettings() {};
    void readDeviceType();
};

#endif // DEVICESETTINGS_H
