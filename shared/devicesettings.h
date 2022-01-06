#ifndef DEVICESETTINGS_H
#define DEVICESETTINGS_H

#include <string>

#define deviceSettings DeviceSettings::instance()

#define DEBUG
#define SENTRY

#ifdef SENTRY
#include <sentry.h>
#define initSentry \
    sentry_options_t *options = sentry_options_new(); \
    sentry_options_set_dsn(options, "https://8d409799a9d640599cc66496fb87edf6@sentry.eeems.codes/2"); \
    sentry_init(options); \
    auto sentryClose = qScopeGuard([] { sentry_close(); }); \
    sentry_capture_event(sentry_value_new_message_event( \
      /*   level */ SENTRY_LEVEL_INFO, \
      /*  logger */ "custom", \
      /* message */ "It works!" \
    )); \
    qDebug() << "Telemetry enabled"
#else
#define initSentry qWarning() << "Telemetry disabled";
#endif

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
