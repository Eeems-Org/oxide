#include "liboxide.h"
#include "event_device.h"

#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>

#include <pwd.h>
#include <grp.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

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
    // https://stackoverflow.com/a/1643134
    int tryGetLock(char const* lockName){
        mode_t m = umask(0);
        int fd = open(lockName, O_RDWR | O_CREAT, 0666);
        umask(m);
        if(fd < 0){
            return -1;
        }
        if(!flock(fd, LOCK_EX | LOCK_NB)){
            return fd;
        }
        close(fd);
        return -1;
    }
    void releaseLock(int fd, char const* lockName){
        if(fd < 0){
            return;
        }
        if(!flock(fd, F_ULOCK | LOCK_NB)){
            remove(lockName);
        }
        close(fd);
    }
    bool processExists(pid_t pid){ return QFile::exists(QString("/proc/%1").arg(pid)); }
    QList<pid_t> lsof(const QString& path){
        QList<pid_t> pids;
        QDir directory("/proc");
        if (!directory.exists() || directory.isEmpty()){
            qCritical() << "Unable to access /proc";
            return pids;
        }
        QString qpath(QFileInfo(path).canonicalFilePath());
        auto processes = directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
        // Get all pids we care about
        for(QFileInfo fi : processes){
            auto pid = fi.baseName().toUInt();
            if(!pid || !processExists(pid)){
                continue;
            }
            QFile statm(QString("/proc/%1/statm").arg(pid));
            QTextStream stream(&statm);
            if(!statm.open(QIODevice::ReadOnly | QIODevice::Text)){
                continue;
            }
            QString content = stream.readAll().trimmed();
            statm.close();
            // Ignore kernel processes
            if(content == "0 0 0 0 0 0 0"){
                continue;
            }
            QDir fd_directory(QString("/proc/%1/fd").arg(pid));
            if(!fd_directory.exists() || fd_directory.isEmpty()){
                continue;
            }
            auto fds = fd_directory.entryInfoList(QDir::Files | QDir::NoDot | QDir::NoDotDot);
            for(QFileInfo fd : fds){
                if(fd.canonicalFilePath() == qpath){
                    pids.append(pid);
                }
            }
        }
        return pids;
    }
    void dispatchToMainThread(std::function<void()> callback){
        dispatchToMainThread<int>([callback]{
            callback();
            return 0;
        });
    }
    uid_t getUID(const QString& name){
        char buffer[1024];
        struct passwd user;
        struct passwd* result;
        auto status = getpwnam_r(name.toStdString().c_str(), &user, buffer, sizeof(buffer), &result);
        if(status != 0){
            throw std::runtime_error("Failed to get user" + status);
        }
        if(result == NULL){
            throw std::runtime_error("Invalid user name: " + name.toStdString());
        }
        return result->pw_uid;
    }
    gid_t getGID(const QString& name){
        char buffer[1024];
        struct group grp;
        struct group* result;
        auto status = getgrnam_r(name.toStdString().c_str(), &grp, buffer, sizeof(buffer), &result);
        if(status != 0){
            throw std::runtime_error("Failed to get group" + status);
        }
        if(result == NULL){
            throw std::runtime_error("Invalid group name: " + name.toStdString());
        }
        return result->gr_gid;
    }
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

    int DeviceSettings::getTouchWidth() const {
        auto path = getTouchDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return 0;
        }
        return event_device(path, O_RDONLY)
            .abs_info(ABS_MT_POSITION_X)
            .maximum;
    }
    int DeviceSettings::getTouchHeight() const {
        auto path = getTouchDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return 0;
        }
        return event_device(path, O_RDONLY)
            .abs_info(ABS_MT_POSITION_Y)
            .maximum;
    }
    int DeviceSettings::getTouchPressure() const{
        auto path = getTouchDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return 0;
        }
        return event_device(path, O_RDONLY)
            .abs_info(ABS_MT_PRESSURE)
            .maximum;
    }
    bool DeviceSettings::supportsMultiTouch() const{
        auto path = getTouchDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return false;
        }
        return 1 < event_device(path, O_RDONLY)
            .abs_info(ABS_MT_SLOT)
            .maximum;
    }
    bool DeviceSettings::isTouchTypeB() const{
        switch(getDeviceType()) {
            case DeviceType::RM1:
            case DeviceType::RM2:
                return true;
            default:
                return false;
        }
    }

    int DeviceSettings::getWacomWidth() const {
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return 0;
        }
        return event_device(path, O_RDONLY)
            .abs_info(ABS_X)
            .maximum;
    }
    int DeviceSettings::getWacomHeight() const {
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return 0;
        }
        return event_device(path, O_RDONLY)
            .abs_info(ABS_Y)
            .maximum;
    }
    int DeviceSettings::getWacomPressure() const{
        auto path = getWacomDevicePath();
        if(QString::fromLatin1(path).isEmpty()){
            return 0;
        }
        return event_device(path, O_RDONLY)
            .abs_info(ABS_PRESSURE)
            .maximum;
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
    void DeviceSettings::setupQtEnvironment(QtEnvironmentType type){
        auto qt_version = qVersion();
        qDebug() << "Qt Runtime: " << qt_version;
        qDebug() << "Qt Build: " << QT_VERSION_STR;
        if (strcmp(qt_version, QT_VERSION_STR) != 0){
            qDebug() << "Version mismatch!";
        }
#ifdef DEBUG
            qputenv("QT_DEBUG_PLUGINS", "1");
#endif
        if(type != DeviceSettings::Oxide){
#ifdef __arm__
            qputenv("QMLSCENE_DEVICE", "epaper");
            qputenv("QT_QUICK_BACKEND","epaper");
            qputenv("QT_QPA_PLATFORM", "epaper:enable_fonts");
            if(type != DeviceSettings::NoPen){
                qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet");
                qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", deviceSettings.getTouchEnvSetting());
            }
#endif
        }else{
            QCoreApplication::addLibraryPath("/opt/usr/lib/plugins");
            qputenv("QT_QUICK_BACKEND","software");
            qputenv("QMLSCENE_DEVICE","software");
            qputenv("QT_QPA_PLATFORM", "oxide:enable_fonts");
            qputenv("QT_DEBUG_OXIDE_QPA", "1");
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
    WifiNetworks XochitlSettings::wifinetworks(){
        beginGroup("wifinetworks");
        QMap<QString, QVariantMap> wifinetworks;
        for(const QString& key : allKeys()){
            QVariantMap network = value(key).toMap();
            wifinetworks[key] = network;
        }
        endGroup();
        return wifinetworks;
    }
    void XochitlSettings::setWifinetworks(const WifiNetworks& wifinetworks){
        beginGroup("wifinetworks");
        for(const QString& key : wifinetworks.keys()){
            setValue(key, wifinetworks.value(key));
        }
        endGroup();
        sync();
    }
    QVariantMap XochitlSettings::getWifiNetwork(const QString& name){
        beginGroup("wifinetworks");
        QVariantMap network = value(name).toMap();
        endGroup();
        return network;
    }
    void XochitlSettings::setWifiNetwork(const QString& name, QVariantMap properties){
        beginGroup("wifinetworks");
        setValue(name, properties);
        endGroup();
        sync();
    }
    void XochitlSettings::resetWifinetworks(){}
    XochitlSettings::~XochitlSettings(){}
    SharedSettings::~SharedSettings(){}
    O_SETTINGS_PROPERTY_BODY(XochitlSettings, QString, General, passcode)
    O_SETTINGS_PROPERTY_BODY(XochitlSettings, bool, General, wifion)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, version)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, firstLaunch, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, telemetry, false)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, applicationUsage, false)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, crashReport, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, lockOnSuspend, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, autoSleep, 5)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, autoLock, 5)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, pin)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, onLogin)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, onFailedLogin)
}
