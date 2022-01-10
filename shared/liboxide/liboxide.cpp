#include "liboxide.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>
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
void sentry_setup_user(){
#ifdef SENTRY
    if(!sharedSettings.telemetry()){
        return;
    }
    sentry_value_t user = sentry_value_new_object();
    sentry_value_set_by_key(user, "id", sentry_value_new_string(readFile("/etc/machine-id").c_str()));
    sentry_set_user(user);
#endif
}
void sentry_setup_context(){
#ifdef SENTRY
    if(!sharedSettings.telemetry()){
        return;
    }
    std::string version = readFile("/etc/version");
    sentry_set_tag("os.version", version.c_str());
    sentry_value_t device = sentry_value_new_object();
    sentry_value_set_by_key(device, "machine-id", sentry_value_new_string(readFile("/etc/machine-id").c_str()));
    sentry_value_set_by_key(device, "version", sentry_value_new_string(readFile("/etc/version").c_str()));
    sentry_set_context("device", device);
#endif
}

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

namespace Oxide {
    void dispatchToMainThread(std::function<void()> callback){
        if(QThread::currentThread() == qApp->thread()){
            callback();
            return;
        }
        // any thread
        QTimer* timer = new QTimer();
        timer->moveToThread(qApp->thread());
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, [=](){
            // main thread
            callback();
            timer->deleteLater();
        });
        QMetaObject::invokeMethod(timer, "start", Qt::BlockingQueuedConnection, Q_ARG(int, 0));
    }
    namespace Sentry{
        static bool initialized = false;
        void sentry_init(const char* name, char* argv[]){
        #ifdef SENTRY
            if(!sharedSettings.telemetry()){
                return;
            }
            // Setup options
            sentry_options_t* options = sentry_options_new();
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
            // Setup user
            sentry_value_t user = sentry_value_new_object();
            sentry_value_set_by_key(user, "id", sentry_value_new_string(readFile("/etc/machine-id").c_str()));
            sentry_set_user(user);
            // Setup context
            std::string version = readFile("/etc/version");
            sentry_set_tag("os.version", version.c_str());
            sentry_value_t device = sentry_value_new_object();
            sentry_value_set_by_key(device, "machine-id", sentry_value_new_string(readFile("/etc/machine-id").c_str()));
            sentry_value_set_by_key(device, "version", sentry_value_new_string(readFile("/etc/version").c_str()));
            sentry_set_context("device", device);
            // Setup transaction
            sentry_set_transaction(name);
            if(initialized){
                return;
            }
            initialized = true;
            // Add close guard
            QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() {
                if(sharedSettings.telemetry()){
                    sentry_close();
                }
            });
            // Handle telemetry being toggled
            QObject::connect(&sharedSettings, &SharedSettings::telemetryChanged, [name, argv](bool telemetry){
                if(telemetry){
                    sentry_init(name, argv);
                }else{
                    sentry_close();
                }
            });
        #else
            Q_UNUSED(name);
            Q_UNUSED(argv);
        #endif
        }
        void sentry_breadcrumb(const char* category, const char* message, const char* type, const char* level){
        #ifdef SENTRY
            if(!sharedSettings.telemetry()){
                return;
            }
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
    }
    DeviceSettings& DeviceSettings::instance() {
        static DeviceSettings INSTANCE;
        return INSTANCE;
    }
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
    XochitlSettings& XochitlSettings::instance() {
        static XochitlSettings INSTANCE;
        return INSTANCE;
    }
    QString XochitlSettings::passcode(){ return value("Passcode", "").toString(); }
    void XochitlSettings::setPasscode(const QString& passcode){
        m_passcode = passcode;
        setValue("Passcode", passcode);
        sync();
    }
    QMap<QString, QVariantMap> XochitlSettings::wifinetworks(){
        beginGroup("wifinetworks");
        QMap<QString, QVariantMap> wifinetworks;
        for(const QString& key : allKeys()){
            QVariantMap network = value(key).toMap();
            wifinetworks.value(key, network);
        }
        endGroup();
        return wifinetworks;
    }
    void XochitlSettings::setWifinetworks(const QMap<QString, QVariantMap>& wifinetworks){
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
    QVariantMap XochitlSettings::setWifiNetwork(const QString& name, QVariantMap properties){
        beginGroup("wifinetworks");
        setValue(name, properties);
        endGroup();
        sync();
    }
    bool XochitlSettings::wifion(){ return value("wifion").toBool(); }
    void XochitlSettings::setWifion(bool wifion){
        setValue("wifion", wifion);
        sync();
    }
    XochitlSettings::XochitlSettings()
        : QSettings("/home/root/.config/remarkable/xochitl.conf", QSettings::IniFormat),
          fileWatcher(QStringList() << "/home/root/.config/remarkable/xochitl.conf")
    {
        // Load values
        sync();
        // Load value cache
        m_passcode = passcode();
        m_wifinetworks = wifinetworks();
        m_wifion = wifion();
        // Connect event listener to emit events when values change
        connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
            fileWatcher.addPath(path);
            // Load new values
            sync();
            // Check if any values have updated
            auto passcode = this->passcode();
            if(passcode != m_passcode){
                m_passcode = passcode;
                emit passcodeChanged(passcode);
            }
            auto wifinetworks = this->wifinetworks();
            if(wifinetworks != m_wifinetworks){
                m_wifinetworks = wifinetworks;
                emit wifinetworksChanged(wifinetworks);
            }
            auto wifion = this->wifion();
            if(wifion != m_wifion){
                m_wifion = wifion;
                emit wifionChanged(wifion);
            }
        });
    }
    XochitlSettings::~XochitlSettings(){}
    SharedSettings& SharedSettings::instance() {
        static SharedSettings INSTANCE;
        return INSTANCE;
    }
    SharedSettings::SharedSettings()
        : QSettings("/home/root/.config/Eeems/shared.conf", QSettings::IniFormat),
          fileWatcher()
    {
        // Load values
        if(!QFile::exists(fileName())){
            setValue("telemetry", true);
        }else{
            sync();
        }
        if(!fileWatcher.addPath(fileName())){
            qWarning() << "Unable to watch shared settings";
        }
        // Load value cache
        m_telemetry = telemetry();
        // Connect event listener to emit events when values change
        connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
            fileWatcher.addPath(path);
            // Load new values
            sync();
            // Check if any values have updated
            auto telemetry = this->telemetry();
            if(telemetry != m_telemetry){
                m_telemetry= telemetry;
                emit telemetryChanged(telemetry);
            }
        });
    }
    SharedSettings::~SharedSettings(){}
    bool SharedSettings::telemetry(){ return value("telemetry").toBool(); }
    void SharedSettings::setTelemetry(bool telemetry){
        setValue("telemetry", telemetry);
        sync();
    }
}
