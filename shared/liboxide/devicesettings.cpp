#include "devicesettings.h"
#include "event_device.h"
#include "debug.h"

#include <sys/file.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

namespace Oxide {
    QString execute(const QString& program, const QStringList& args){
        QString output;
        QProcess p;
        p.setProgram(program);
        p.setArguments(args);
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.connect(&p, &QProcess::readyReadStandardOutput, [&p, &output]{
            output += (QString)p.readAllStandardOutput();
        });
        p.start();
        p.waitForFinished();
        return output;
    }
    DeviceSettings& DeviceSettings::instance() {
        static DeviceSettings* INSTANCE = nullptr;
        if(INSTANCE == nullptr){
            INSTANCE = new DeviceSettings();
        }
        return *INSTANCE;
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
            if (ioctl(fd, EVIOCGVERSION, &version)){
                O_DEBUG("    Invalid");
                continue;
            }
            unsigned long bit[EV_MAX];
            ioctl(fd, EVIOCGBIT(0, EV_MAX), bit);
            if (test_bit(EV_KEY, bit)) {
                if (checkBitSet(fd, EV_KEY, BTN_STYLUS) && test_bit(EV_ABS, bit)) {
                    O_DEBUG("    Wacom input device detected");
                    wacomPath = fullPath.toStdString();
                    continue;
                }
                if (checkBitSet(fd, EV_KEY, KEY_POWER)) {
                    O_DEBUG("    Buttons input device detected");
                    buttonsPath = fullPath.toStdString();
                    continue;
                }
            }
            if (checkBitSet(fd, EV_ABS, ABS_MT_SLOT)) {
                O_DEBUG("    Touch input device detected");
                touchPath = fullPath.toStdString();
                continue;
            }
            O_DEBUG("    Invalid");
        }
        if (wacomPath.empty()) {
            O_WARNING("Wacom input device not found");
        }
        if (touchPath.empty()) {
            O_WARNING("Touch input device not found");
        }
        if (buttonsPath.empty()){
            O_WARNING("Buttons input device not found");
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

    const char* DeviceSettings::getWacomEnvSetting() const {
        switch(getDeviceType()) {
            case DeviceType::RM1:
            case DeviceType::RM2:
            default:
                return "";
        }
    }

    static int _touchWidth = -1;
    int DeviceSettings::getTouchWidth() const {
        if(_touchWidth != -1){
            return _touchWidth;
        }
        auto path = getTouchDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _touchWidth = event_device(path, O_RDONLY)
            .abs_info(ABS_MT_POSITION_X)
            .maximum;
        return _touchWidth;
    }

    static int _touchHeight = -1;
    int DeviceSettings::getTouchHeight() const {
        if(_touchHeight != -1){
            return _touchHeight;
        }
        auto path = getTouchDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _touchHeight = event_device(path, O_RDONLY)
            .abs_info(ABS_MT_POSITION_Y)
            .maximum;
        return _touchHeight;
    }
    static int _touchPressure = -1;
    int DeviceSettings::getTouchPressure() const{
        if(_touchPressure != -1){
            return _touchPressure;
        }
        auto path = getTouchDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _touchPressure = event_device(path, O_RDONLY)
            .abs_info(ABS_MT_PRESSURE)
            .maximum;
        return _touchPressure;
    }
    bool DeviceSettings::supportsMultiTouch() const{ return getTouchSlots() > 1; }
    bool DeviceSettings::isTouchTypeB() const{
        switch(getDeviceType()) {
            case DeviceType::RM1:
            case DeviceType::RM2:
                return true;
            default:
                return false;
        }
    }
    static int _touchSlots = -1;
    int DeviceSettings::getTouchSlots() const{
        if(_touchSlots != -1){
            return _touchSlots;
        }
        auto path = getTouchDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return false;
        }
        _touchSlots = event_device(path, O_RDONLY)
            .abs_info(ABS_MT_SLOT)
            .maximum;
        return _touchSlots;
    }

    static int _wacomWidth = -1;
    int DeviceSettings::getWacomWidth() const {
        if(_wacomWidth != -1){
            return _wacomWidth;
        }
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _wacomWidth = event_device(path, O_RDONLY)
            .abs_info(ABS_X)
            .maximum;
        return _wacomWidth;
    }
    static int _wacomHeight = -1;
    int DeviceSettings::getWacomHeight() const {
        if(_wacomHeight != -1){
            return _wacomHeight;
        }
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _wacomHeight = event_device(path, O_RDONLY)
            .abs_info(ABS_Y)
            .maximum;
        return _wacomHeight;
    }
    static int _wacomPressure = -1;
    int DeviceSettings::getWacomPressure() const{
        if(_wacomPressure != -1){
            return _wacomPressure;
        }
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _wacomPressure = event_device(path, O_RDONLY)
            .abs_info(ABS_PRESSURE)
            .maximum;
        return _wacomPressure;
    }
    static int _wacomMinXTilt = 0;
    int DeviceSettings::getWacomMinXTilt() const{
        if(_wacomMinXTilt != 0){
            return _wacomMinXTilt;
        }
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _wacomMinXTilt = event_device(path, O_RDONLY)
            .abs_info(ABS_TILT_X)
            .minimum;
        return _wacomMinXTilt;
    }
    static int _wacomMinYTilt = 0;
    int DeviceSettings::getWacomMinYTilt() const{
        if(_wacomMinYTilt != 0){
            return _wacomMinYTilt;
        }
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _wacomMinYTilt = event_device(path, O_RDONLY)
            .abs_info(ABS_TILT_Y)
            .minimum;
        return _wacomMinYTilt;
    }
    static int _wacomMaxXTilt = 0;
    int DeviceSettings::getWacomMaxXTilt() const{
        if(_wacomMaxXTilt != 0){
            return _wacomMaxXTilt;
        }
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _wacomMaxXTilt = event_device(path, O_RDONLY)
            .abs_info(ABS_TILT_X)
            .maximum;
        return _wacomMaxXTilt;
    }
    static int _wacomMaxYTilt = 0;
    int DeviceSettings::getWacomMaxYTilt() const{
        if(_wacomMaxYTilt != 0){
            return _wacomMaxYTilt;
        }
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return -1;
        }
        _wacomMaxYTilt = event_device(path, O_RDONLY)
            .abs_info(ABS_TILT_Y)
            .maximum;
        return _wacomMaxYTilt;
    }
    const QStringList DeviceSettings::getLocales(){ return execute("localectl", QStringList() << "list-locales" << "--no-pager").split("\n"); }
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
    const QStringList DeviceSettings::getTimezones(){ return execute("timedatectl", QStringList() << "list-timezones" << "--no-pager").split("\n"); }
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
    void DeviceSettings::setupQtEnvironment(QtEnvironmentType type){
        auto qt_version = qVersion();
        qDebug() << "Qt Runtime: " << qt_version;
        qDebug() << "Qt Build: " << QT_VERSION_STR;
        if (strcmp(qt_version, QT_VERSION_STR) != 0){
            qDebug() << "Version mismatch!";
        }
#ifdef DEBUG
        if(debugEnabled()){
            qputenv("QT_DEBUG_PLUGINS", "1");
        }
#endif
        if(type != DeviceSettings::Oxide || qEnvironmentVariableIsSet("OXIDE_PRELOAD")){
#ifdef __arm__
            qputenv("QMLSCENE_DEVICE", "epaper");
            qputenv("QT_QUICK_BACKEND","epaper");
            qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
            qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", deviceSettings.getTouchEnvSetting());
            if(type != DeviceSettings::NoPen){
                qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet");
                qputenv("QT_QPA_EVDEV_TABLET_PARAMETERS", deviceSettings.getWacomEnvSetting());
            }
#endif
        }else{
            QCoreApplication::addLibraryPath("/opt/usr/lib/plugins");
            qputenv("QT_QUICK_BACKEND","software");
            qputenv("QMLSCENE_DEVICE","software");
            qputenv("QT_QPA_PLATFORM", "oxide:enable_fonts");
#ifdef DEBUG
            if(debugEnabled()){
#endif
                qputenv("QT_DEBUG_OXIDE_QPA", "1");
#ifdef DEBUG
            }
#endif
        }
    }
    QRect DeviceSettings::screenGeometry(){
        switch(getDeviceType()){
            case DeviceType::RM1:
            case DeviceType::RM2:
                return QRect(0, 0, 1404, 1872);
            default:
                return QRect();
        }
    }
}
