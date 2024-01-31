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
            if(!wacomPath.empty() && !touchPath.empty() && !buttonsPath.empty()){
                break;
            }
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
        QCoreApplication::addLibraryPath("/opt/usr/lib/plugins");
        qputenv("QMLSCENE_DEVICE", "software");
        qputenv("QT_QUICK_BACKEND","software");
        QString platform("oxide:enable_fonts:freetype:freetype");
        if(touch){
            qputenv(
                "QT_QPA_PLATFORM",
                QString("%1:%2")
                    .arg(platform)
                    .arg(deviceSettings.getTouchEnvSetting())
                    .toUtf8()
            );
        }else{
            qputenv("QT_QPA_PLATFORM", platform.toUtf8());
        }
    }

    bool DeviceSettings::keyboardAttached(){ return !physicalKeyboards().empty(); }

    void DeviceSettings::onKeyboardAttachedChanged(std::function<void()> callback){
        auto manager = QGuiApplicationPrivate::inputDeviceManager();
        QObject::connect(
            manager,
            &QInputDeviceManager::deviceListChanged,
            manager,
            [callback](QInputDeviceManager::DeviceType type){
                if(type == QInputDeviceManager::DeviceTypeKeyboard){
                    callback();
                }
            }
        );
    }

    QList<event_device> DeviceSettings::inputDevices(){
        QList<event_device> devices;
        QDir dir("/dev/input");
        for(auto path : dir.entryList(QDir::Files | QDir::NoSymLinks | QDir::System)){
            QString fullPath(dir.path() + "/" + path);
            QFile device(fullPath);
            device.open(QIODevice::ReadOnly);
            int fd = device.handle();
            int version;
            if(ioctl(fd, EVIOCGVERSION, &version)){
                continue;
            }
            devices.append(event_device(fullPath.toStdString(), O_RDWR | O_NONBLOCK));
        }
        return devices;
    }

    void DeviceSettings::onInputDevicesChanged(std::function<void()> callback){
        auto manager = QGuiApplicationPrivate::inputDeviceManager();
        QObject::connect(
            manager,
            &QInputDeviceManager::deviceListChanged,
            manager,
            [callback](QInputDeviceManager::DeviceType type){
                Q_UNUSED(type);
                callback();
            }
        );
    }

    QList<event_device> DeviceSettings::keyboards(){
        QList<event_device> keyboards;
        for(auto device : inputDevices()){
            if(
                device.device == buttonsPath
                || device.device == wacomPath
                || device.device == touchPath
            ){
                continue;
            }
            int fd = device.fd;
            unsigned long bit[EV_MAX];
            ioctl(fd, EVIOCGBIT(0, EV_MAX), bit);
            if(!test_bit(EV_KEY, bit)){
                continue;
            }
            if(checkBitSet(fd, EV_KEY, BTN_STYLUS) && test_bit(EV_ABS, bit)){
                continue;
            }
            auto name = QFileInfo(device.device.c_str()).baseName();
            SysObject sys("/sys/class/input/" + name + "/device");
            auto vendor = sys.strProperty("id/vendor");
            if(vendor == "0000"){
                continue;
            }
            auto product = sys.strProperty("id/product");
            if(product == "0000"){
                continue;
            }
            keyboards.append(device);
        }
        return keyboards;
    }
    static QStringList VIRTUAL_KEYBOARD_IDS(
        QStringList() << "0fac:0ade" << "0fac:1ade" << "0000:ffff"
    );
    QList<event_device> DeviceSettings::physicalKeyboards(){
        QList<event_device> physicalKeyboards;
        for(auto device : keyboards()){
            auto name = QFileInfo(device.device.c_str()).baseName();
            SysObject sys("/sys/class/input/" + name + "/device/id");
            auto id = QString("%1:%2")
                .arg(sys.strProperty("vendor").c_str())
                .arg(sys.strProperty("product").c_str());
            if(!VIRTUAL_KEYBOARD_IDS.contains(id)){
                physicalKeyboards.append(device);
            }
        }
        return physicalKeyboards;
    }
    QList<event_device> DeviceSettings::virtualKeyboards(){
        QList<event_device> physicalKeyboards;
        for(auto device : keyboards()){
            auto name = QFileInfo(device.device.c_str()).baseName();
            SysObject sys("/sys/class/input/" + name + "/device/id");
            auto id = QString("%1:%2")
                .arg(sys.strProperty("vendor").c_str())
                .arg(sys.strProperty("product").c_str());
            if(VIRTUAL_KEYBOARD_IDS.contains(id)){
                physicalKeyboards.append(device);
            }
        }
        return physicalKeyboards;
    }
}
