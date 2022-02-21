#include "liboxide.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QCryptographicHash>
#include <QUuid>
#include <QLibraryInfo>

#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/input.h>
#include <systemd/sd-id128.h>

// String: 5aa5ca39ee0b4f48927529ca17519524
// UUID: 5aa5ca39-ee0b-4f48-9275-29ca17519524
#define OXIDE_UID SD_ID128_MAKE(5a,a5,ca,39,ee,0b,4f,48,92,75,29,ca,17,51,95,24)

#ifdef SENTRY
#define SAMPLE_RATE 1.0
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

static void *invalid_mem = (void *)1;

void logMachineIdError(int error, QString name, QString path){
    if(error == -ENOENT){
        qWarning() << "/etc/machine-id is missing";
    }else  if(error == -ENOMEDIUM){
        qWarning() << path + " is empty or all zeros";
    }else if(error == -EIO){
        qWarning() << path + " has the incorrect format";
    }else if(error == -EPERM){
        qWarning() << path + " access denied";
    } if(error == -EINVAL){
        qWarning() << "Error while reading " + name + ": Buffer invalid";
    }else if(error == -ENXIO){
        qWarning() << "Error while reading " + name + ": No invocation ID is set";
    }else if(error == -EOPNOTSUPP){
        qWarning() << "Error while reading " + name + ": Operation not supported";
    }else{
        qWarning() << "Unexpected error code reading " + name + ":" << strerror(error);
    }
}
std::string getAppSpecific(sd_id128_t base){
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buf[SD_ID128_STRING_MAX];
    hash.addData(sd_id128_to_string(base, buf));
    hash.addData(sd_id128_to_string(OXIDE_UID, buf));
    auto r = hash.result();
    r[6] = (r.at(6) & 0x0F) | 0x40;
    r[8] = (r.at(8) & 0x3F) | 0x80;
    QUuid uid(r.at(0), r.at(1), r.at(2), r.at(3), r.at(4), r.at(5), r.at(6), r.at(7), r.at(8), r.at(9), r.at(10));
    return uid.toString((QUuid::Id128)).toStdString();
}

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
#ifdef SENTRY
        Transaction::Transaction(sentry_transaction_t* t)
        {
            inner = t;
        }
        Span::Span(sentry_span_t* s)
        {
            inner = s;
        }
#else
        Transaction::Transaction(void* t){
            inner = nullptr;
        }
        Span::Span(void* s){
            inner = nullptr;
        }
#endif
        const char* bootId(){
            static std::string bootId("");
            if(!bootId.empty()){
                return bootId.c_str();
            }
            sd_id128_t id;
            int ret = sd_id128_get_boot(&id);
            // TODO - eventually replace with the following when supported by the
            //        reMarkable kernel
            // int ret = sd_id128_get_boot_app_specific(OXIDE_UID, &id);
            if(ret == EXIT_SUCCESS){
                bootId = getAppSpecific(id);
                // TODO - eventually replace with the following when supported by the
                //        reMarkable kernel
                //char buf[SD_ID128_STRING_MAX];
                //bootId = sd_id128_to_string(id, buf);
                return bootId.c_str();
            }
            logMachineIdError(ret, "boot_id", "/proc/sys/kernel/random/boot_id");
            return "";
        }
        const char* machineId(){
            static std::string machineId("");
            if(!machineId.empty()){
                return machineId.c_str();
            }
            sd_id128_t id;
            int ret = sd_id128_get_machine(&id);
            // TODO - eventually replace with the following when supported by the
            //        reMarkable kernel
            // int ret = sd_id128_get_machine_app_specific(OXIDE_UID, &id);
            if(ret == EXIT_SUCCESS){
                machineId = getAppSpecific(id);
                // TODO - eventually replace with the following when supported by the
                //        reMarkable kernel
                //char buf[SD_ID128_STRING_MAX];
                //machineId = sd_id128_to_string(id, buf);
                return machineId.c_str();
            }
            logMachineIdError(ret, "machine-id", "/etc/machine-id");
            return "";
        }
        bool enabled(){
            return sharedSettings.crashReport() || sharedSettings.telemetry();
        }
        void sentry_init(const char* name, char* argv[], bool autoSessionTracking){
#ifdef SENTRY
            if(!enabled()){
                return;
            }
            // Setup options
            sentry_options_t* options = sentry_options_new();
            sentry_options_set_dsn(options, "https://8d409799a9d640599cc66496fb87edf6@sentry.eeems.codes/2");
            sentry_options_set_auto_session_tracking(options, autoSessionTracking && sharedSettings.telemetry());
            if(sharedSettings.telemetry()){
                sentry_options_set_sample_rate(options, SAMPLE_RATE);
                sentry_options_set_max_spans(options, 1000);
            }
            sentry_options_set_symbolize_stacktraces(options, true);
            if(QLibraryInfo::isDebugBuild()){
                sentry_options_set_environment(options, "debug");
            }else{
                sentry_options_set_environment(options, "release");
            }
#ifdef DEBUG
            sentry_options_set_debug(options, debugEnabled());
#endif
            sentry_options_set_database_path(options, "/home/root/.cache/Eeems/sentry");
            sentry_options_set_release(options, (std::string(name) + "@2.3").c_str());
            sentry_init(options);
            // Setup user
            sentry_value_t user = sentry_value_new_object();
            sentry_value_set_by_key(user, "id", sentry_value_new_string(machineId()));
            sentry_set_user(user);
            // Setup context
            std::string version = readFile("/etc/version");
            sentry_set_tag("os.version", version.c_str());
            sentry_value_t device = sentry_value_new_object();
            sentry_value_set_by_key(device, "machine-id", sentry_value_new_string(machineId()));
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
                sentry_close();
            });
            // Handle settings changing
            QObject::connect(&sharedSettings, &SharedSettings::telemetryChanged, [name, argv, autoSessionTracking](bool telemetry){
                Q_UNUSED(telemetry)
                sentry_close();
                sentry_init(name, argv, autoSessionTracking);
            });
            QObject::connect(&sharedSettings, &SharedSettings::crashReportChanged, [name, argv, autoSessionTracking](bool crashReport){
                Q_UNUSED(crashReport)
                sentry_close();
                sentry_init(name, argv, autoSessionTracking);
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
        Transaction* start_transaction(std::string name, std::string action){
#ifdef SENTRY
            sentry_transaction_context_t* context = sentry_transaction_context_new(name.c_str(), action.c_str());
            sentry_transaction_t* transaction = sentry_transaction_start(context, sentry_value_new_null());
            // Hack to force transactions to be reported even though SAMPLE_RATE is 100%
            sentry_value_set_by_key(transaction->inner, "sampled", sentry_value_new_bool(true));
            return new Transaction(transaction);
#else
            Q_UNUSED(name);
            Q_UNUSED(action);
            return nullptr;
#endif
        }
        void stop_transaction(Transaction* transaction){
#ifdef SENTRY
            if(transaction != nullptr && transaction->inner != nullptr){
                sentry_transaction_finish(transaction->inner);
            }
#else
            Q_UNUSED(transaction);
#endif
        }
        void sentry_transaction(std::string name, std::string action, std::function<void(Transaction* transaction)> callback){
#ifdef SENTRY
            if(!sharedSettings.telemetry()){
                callback(nullptr);
                return;
            }
            Transaction* transaction = start_transaction(name, action);
            auto scopeGuard = qScopeGuard([transaction] {
                stop_transaction(transaction);
            });
            callback(transaction);
#else
            Q_UNUSED(name);
            Q_UNUSED(action);
            callback(nullptr);
#endif
        }
        Span* start_span(Transaction* transaction, std::string operation, std::string description){
#ifdef SENTRY
            if(transaction == nullptr){
                return nullptr;
            }
            return new Span(sentry_transaction_start_child(transaction->inner, (char*)operation.c_str(), (char*)description.c_str()));
#else
            Q_UNUSED(transaction);
            Q_UNUSED(operation);
            Q_UNUSED(description);
            return nullptr;
#endif
        }
        void stop_span(Span* span){
#ifdef SENTRY
            if(span != nullptr && span->inner != nullptr){
                sentry_span_finish(span->inner);
            }
#else
            Q_UNUSED(span);
#endif
        }
        void sentry_span(Transaction* transaction, std::string operation, std::string description, std::function<void()> callback){
#ifdef SENTRY
            if(!sharedSettings.telemetry() || transaction == nullptr || transaction->inner == nullptr){
                callback();
                return;
            }
            Span* span = start_span(transaction, operation, description);
            auto scopeGuard = qScopeGuard([span] {
                stop_span(span);
            });
            callback();
#else
            Q_UNUSED(transaction);
            Q_UNUSED(operation);
            Q_UNUSED(description);
            callback();
#endif
        }
        void trigger_crash(){ memset((char *)invalid_mem, 1, 100); }

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
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, crashReport, true)
}
