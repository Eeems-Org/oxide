#include "devicesettings.h"

#include <private/qguiapplication_p.h>
#include <private/qinputdevicemanager_p.h>

#include "debug.h"
#include "liboxide.h"

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

using namespace Oxide;

namespace Oxide {
    DeviceSettings& DeviceSettings::instance() {
        static DeviceSettings INSTANCE;
        return INSTANCE;
    }
    DeviceSettings::DeviceSettings(): _deviceType(DeviceType::RM1) {
        readDeviceType();

        O_DEBUG("Looking for input devices...");
        QDir dir("/dev/input");
        for(auto path : dir.entryList(QDir::Files | QDir::NoSymLinks | QDir::System)){
            O_DEBUG(("  Checking " + path + "...").toStdString().c_str());
            QString fullPath(dir.path() + "/" + path);
            QFile device(fullPath);
            device.open(QIODevice::ReadOnly);
            int fd = device.handle();
            int version;
            if(ioctl(fd, EVIOCGVERSION, &version)){
                O_DEBUG("    Invalid");
                continue;
            }
            unsigned long bit[EV_MAX];
            ioctl(fd, EVIOCGBIT(0, EV_MAX), bit);
            if(test_bit(EV_KEY, bit)){
                if(checkBitSet(fd, EV_KEY, BTN_STYLUS) && test_bit(EV_ABS, bit)){
                    O_DEBUG("    Wacom input device detected");
                    if(wacomPath.empty()){
                        wacomPath = fullPath.toStdString();
                    }
                    continue;
                }
                if(checkBitSet(fd, EV_KEY, KEY_POWER)){
                    O_DEBUG("    Buttons input device detected");
                    if(buttonsPath.empty()){
                        buttonsPath = fullPath.toStdString();
                    }
                    continue;
                }
            }
            if(checkBitSet(fd, EV_ABS, ABS_MT_SLOT)){
                O_DEBUG("    Touch input device detected");
                if(touchPath.empty()){
                    touchPath = fullPath.toStdString();
                }
                continue;
            }
            O_DEBUG("    Invalid");
        }
        if(wacomPath.empty()){
            O_WARNING("Wacom input device not found");
        }else{
            O_DEBUG(("Wacom input device: " + wacomPath).c_str());
        }
        if(touchPath.empty()){
            O_WARNING("Touch input device not found");
        }else{
            O_DEBUG(("Touch input device: " + touchPath).c_str());
        }
        if(buttonsPath.empty()){
            O_WARNING("Buttons input device not found");
        }else{
            O_DEBUG(("Buttons input device: " + buttonsPath).c_str());
        }
    }
    DeviceSettings::~DeviceSettings(){}
    bool DeviceSettings::checkBitSet(int fd, int type, int i) {
        unsigned long bit[NBITS(KEY_MAX)];
        ioctl(fd, EVIOCGBIT(type, KEY_MAX), bit);
        return test_bit(i, bit);
    }

    void DeviceSettings::readDeviceType() {
        QFile file("/sys/devices/soc0/machine");
        if(!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)){
            O_DEBUG("Couldn't open " << file.fileName());
            _deviceType = DeviceType::Unknown;
            return;
        }
        QTextStream in(&file);
        QString modelName = in.readLine();
        if (modelName.startsWith("reMarkable 2")) {
            O_DEBUG("RM2 detected...");
            _deviceType = DeviceType::RM2;
            return;
        }
        O_DEBUG("RM1 detected...");
        _deviceType = DeviceType::RM1;
    }

    DeviceSettings::DeviceType DeviceSettings::getDeviceType() const { return _deviceType; }

    const char* DeviceSettings::getButtonsDevicePath() const { return buttonsPath.c_str(); }

    const char* DeviceSettings::getWacomDevicePath() const { return wacomPath.c_str(); }

    const char* DeviceSettings::getTouchDevicePath() const { return touchPath.c_str(); }
    const char* DeviceSettings::getDeviceName() const {
        switch(getDeviceType()){
            case DeviceType::RM1:
                return "reMarkable 1";
            case DeviceType::RM2:
                return "reMarkable 2";
            default:
                return "Unknown";
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
    const QStringList DeviceSettings::getLocales() {
        return execute("localectl", QStringList() << "list-locales" << "--no-pager").split("\n");
    }
    QString DeviceSettings::getLocale() {
        QFile file("/etc/locale.conf");
        if(file.open(QFile::ReadOnly)){
            while(!file.atEnd()){
                QString line = file.readLine();
                QStringList fields = line.split("=");
                if(fields.first().trimmed() != "LANG"){
                    continue;
                }
                return fields.at(1).trimmed();
            }
        }
        return qEnvironmentVariable("LANG", "C");
    }
    void DeviceSettings::setLocale(const QString& locale) {
        if(debugEnabled()){
            qDebug() << "Setting locale:" << locale;
        }
        qputenv("LANG", locale.toUtf8());
        QProcess::execute("localectl", QStringList() << "set-locale" << locale);
    }
    const QStringList DeviceSettings::getTimezones() {
        return execute("timedatectl", QStringList() << "list-timezones" << "--no-pager").split("\n");
    }
    QString DeviceSettings::getTimezone() {
        auto lines = execute("timedatectl", QStringList() << "show").split("\n");
        for(auto line : lines){
            QStringList fields = line.split("=");
            if(fields.first().trimmed() != "Timezone"){
                continue;
            }
            return fields.at(1).trimmed();
        }
        return "UTC";
    }
    void DeviceSettings::setTimezone(const QString& timezone) {
        if(debugEnabled()){
            qDebug() << "Setting timezone:" << timezone;
        }
        QProcess::execute("timedatectl", QStringList() << "set-timezone" << timezone);
    }
    void DeviceSettings::setupQtEnvironment(bool touch){
        auto qt_version = qVersion();
        if (strcmp(qt_version, QT_VERSION_STR) != 0){
            qDebug() << "Version mismatch, Runtime: " << qt_version << ", Build: " << QT_VERSION_STR;
        }
    #ifdef __arm__
        qputenv("QMLSCENE_DEVICE", "epaper");
        qputenv("QT_QUICK_BACKEND","epaper");
        qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
    #endif
        if(touch){
            qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", deviceSettings.getTouchEnvSetting());
            qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet");
        }
    }
    bool DeviceSettings::keyboardAttached(){
        QDir dir("/dev/input");
        for(auto path : dir.entryList(QDir::Files | QDir::NoSymLinks | QDir::System)){
            O_DEBUG(("  Checking " + path + "...").toStdString().c_str());
            QString fullPath(dir.path() + "/" + path);
            if(
                fullPath == QString::fromStdString(buttonsPath)
                || fullPath == QString::fromStdString(wacomPath)
                || fullPath == QString::fromStdString(touchPath)
                ){
                continue;
            }
            QFile device(fullPath);
            device.open(QIODevice::ReadOnly);
            int fd = device.handle();
            int version;
            if(ioctl(fd, EVIOCGVERSION, &version)){
                continue;
            }
            unsigned long bit[EV_MAX];
            ioctl(fd, EVIOCGBIT(0, EV_MAX), bit);
            if(!test_bit(EV_KEY, bit)){
                continue;
            }
            if(checkBitSet(fd, EV_KEY, BTN_STYLUS) && test_bit(EV_ABS, bit)){
                continue;
            }
            SysObject sys("/sys/class/input/" + path + "/device");
            auto vendor = sys.strProperty("id/vendor");
            if(vendor == "0000"){
                continue;
            }
            auto product = sys.strProperty("id/product");
            if(product == "0000"){
                continue;
            }
            auto id = vendor+":"+product;
            if(id == "0fac:0ade" || id == "0fac:1ade"){
                continue;
            }
            O_DEBUG("Keyboard found: " << sys.strProperty("name").c_str());
            return true;
        }
        O_DEBUG("No keyboard found");
        return false;
    }
    void DeviceSettings::onKeyboardAttachedChanged(std::function<void()> callback){
        auto manager = QGuiApplicationPrivate::inputDeviceManager();
        QObject::connect(manager, &QInputDeviceManager::deviceListChanged, [callback](QInputDeviceManager::DeviceType type){
            if(type == QInputDeviceManager::DeviceTypeKeyboard){
                callback();
            }
        });
    }
}
