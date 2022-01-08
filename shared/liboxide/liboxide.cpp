#include "liboxide.h"

#include <QDebug>
#include <QDir>
#include <QFile>

#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/input.h>

#ifdef SENTRY
std::string readFile(std::string path){
    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
#endif
void sentry_setup_options(const char* name, char* argv[]){
#ifdef SENTRY
    sentry_options_t* options = sentry_options_new(); \
    sentry_options_set_dsn(options, "https://8d409799a9d640599cc66496fb87edf6@sentry.eeems.codes/2");
    sentry_options_set_auto_session_tracking(options, true);
    sentry_options_set_symbolize_stacktraces(options, true);
    sentry_options_set_environment(options, argv[0]);
#ifdef DEBUG
    sentry_options_set_debug(options, true);
#endif
    sentry_options_set_database_path(options, "/home/root/.cache/Eeems/sentry");
    sentry_options_set_release(options, (std::string(name) + "@2.3").c_str());
    sentry_init(options);
#else
    Q_UNUSED(name);
    Q_UNUSED(argv);
#endif
}
void sentry_setup_user(){
#ifdef SENTRY
    sentry_value_t user = sentry_value_new_object();
    sentry_value_set_by_key(user, "id", sentry_value_new_string(readFile("/etc/machine-id").c_str()));
    sentry_set_user(user);
#endif
}
void sentry_setup_context(){
#ifdef SENTRY
    std::string version = readFile("/etc/version");
    sentry_set_tag("os.version", version.c_str());
    sentry_value_t device = sentry_value_new_object();
    sentry_value_set_by_key(device, "machine-id", sentry_value_new_string(readFile("/etc/machine-id").c_str()));
    sentry_value_set_by_key(device, "version", sentry_value_new_string(readFile("/etc/version").c_str()));
    sentry_set_context("device", device);
#endif
}
void initSentry(const char* name, char* argv[]){
#ifdef SENTRY
    sentry_setup_options(name, argv); \
    sentry_setup_user(); \
    sentry_setup_context(); \
    sentry_set_transaction(name); \
    auto sentryClose = qScopeGuard([] { sentry_close(); });
#else
    Q_UNUSED(name);
    Q_UNUSED(argv);
    qWarning() << "Telemetry disabled";
#endif
}
void sentry_breadcrumb(const char* category, const char* message, const char* type, const char* level){
#ifdef SENTRY
    sentry_value_t crumb = sentry_value_new_breadcrumb(type, message);
    sentry_value_set_by_key(crumb, "category", sentry_value_new_string(category));
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string(level));
    sentry_add_breadcrumb(crumb);
#else
    Q_UNUSED(category);
    Q_UNUSED(message);
    Q_UNUSED(type);
    Q_UNUSED(level);
#endif
}

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

namespace Oxide {
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
                    wacomPath = fullPath.toStdString();
                    continue;
                }
                if (checkBitSet(fd, EV_KEY, KEY_POWER)) {
                    qDebug() << "    Buttons input device detected";
                    buttonsPath = fullPath.toStdString();
                    continue;
                }
            }
            if (checkBitSet(fd, EV_ABS, ABS_MT_SLOT)) {
                qDebug() << "    Touch input device detected";
                touchPath = fullPath.toStdString();
                continue;
            }
            qDebug() << "    Invalid";
        }
        if (wacomPath.empty()) {
            qWarning() << "Wacom input device not found";
        }
        if (touchPath.empty()) {
            qWarning() << "Touch input device not found";
        }
        if (buttonsPath.empty()){
            qWarning() << "Buttons input device not found";
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

    const char* DeviceSettings::getButtonsDevicePath() const { return buttonsPath.c_str(); }

    const char* DeviceSettings::getWacomDevicePath() const { return wacomPath.c_str(); }

    const char* DeviceSettings::getTouchDevicePath() const { return touchPath.c_str(); }

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
}