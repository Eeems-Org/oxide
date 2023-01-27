#include "oxide_sentry.h"
#include "liboxide.h"

#include <QtGlobal>
#include <QUuid>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QLibraryInfo>

#include <sstream>
#include <fstream>
#include <string.h>
#include <systemd/sd-id128.h>

// String: 5aa5ca39ee0b4f48927529ca17519524
// UUID: 5aa5ca39-ee0b-4f48-9275-29ca17519524
#define OXIDE_UID SD_ID128_MAKE(5a,a5,ca,39,ee,0b,4f,48,92,75,29,ca,17,51,95,24)

#ifdef SENTRY
#define SAMPLE_RATE 1.0
std::string readFile(const std::string& path){
    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
#endif

static void* invalid_mem = (void *)1;

void logMachineIdError(int error, QString name, QString path){
    if(error == -ENOENT){
        O_WARNING("/etc/machine-id is missing");
    }else  if(error == -ENOMEDIUM){
        O_WARNING(path + " is empty or all zeros");
    }else if(error == -EIO){
        O_WARNING(path + " has the incorrect format");
    }else if(error == -EPERM){
        O_WARNING(path + " access denied");
    } if(error == -EINVAL){
        O_WARNING("Error while reading " + name + ": Buffer invalid");
    }else if(error == -ENXIO){
        O_WARNING("Error while reading " + name + ": No invocation ID is set");
    }else if(error == -EOPNOTSUPP){
        O_WARNING("Error while reading " + name + ": Operation not supported");
    }else{
        O_WARNING("Unexpected error code reading " + name + ":" << strerror(error));
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

namespace Oxide::Sentry{
#ifdef SENTRY
    static bool initialized = false;
    Transaction::Transaction(sentry_transaction_t* t){
        inner = t;
    }
    Span::Span(sentry_span_t* s){
        inner = s;
    }
#else
    Transaction::Transaction(void* t){
        Q_UNUSED(t);
        inner = nullptr;
    }
    Span::Span(void* s){
        Q_UNUSED(s);
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
#ifdef SENTRY
    sentry_options_t* options = sentry_options_new();
#endif
    void sentry_init(const char* name, char* argv[], bool autoSessionTracking){
#ifdef SENTRY
        if(sharedSettings.crashReport()){
            sentry_options_set_sample_rate(options, SAMPLE_RATE);
        }else{
            sentry_options_set_sample_rate(options, 0.0);
        }
        if(sharedSettings.telemetry()){
            sentry_options_set_traces_sample_rate(options, SAMPLE_RATE);
            sentry_options_set_max_spans(options, 1000);
        }else{
            sentry_options_set_traces_sample_rate(options, 0.0);
        }
        if(!sharedSettings.telemetry() && !sharedSettings.crashReport()){
            sentry_user_consent_revoke();
        }else{
            sentry_user_consent_give();
        }
        sentry_options_set_auto_session_tracking(options, autoSessionTracking && sharedSettings.telemetry());
        if(initialized){
            return;
        }
        initialized = true;
        // Setup options
        sentry_options_set_dsn(options, "https://a0136a12d63048c5a353c4a1c2d38914@sentry.eeems.codes/2");
        sentry_options_set_symbolize_stacktraces(options, true);
        if(QLibraryInfo::isDebugBuild()){
            sentry_options_set_environment(options, "debug");
        }else{
            sentry_options_set_environment(options, "release");
        }
        sentry_options_set_debug(options, debugEnabled());
        sentry_options_set_database_path(options, "/home/root/.cache/Eeems/sentry");
        sentry_options_set_release(options, (std::string(name) + "@2.4").c_str());
        sentry_init(options);

        // Setup user
        sentry_value_t user = sentry_value_new_object();
        sentry_value_set_by_key(user, "id", sentry_value_new_string(machineId()));
        sentry_set_user(user);
        // Setup context
        std::string version = readFile("/etc/version");
        sentry_set_tag("os.version", version.c_str());
        sentry_set_tag("name", name);
        sentry_value_t device = sentry_value_new_object();
        sentry_value_set_by_key(device, "machine-id", sentry_value_new_string(machineId()));
        sentry_value_set_by_key(device, "version", sentry_value_new_string(readFile("/etc/version").c_str()));
        sentry_value_set_by_key(device, "model", sentry_value_new_string(deviceSettings.getDeviceName()));
        sentry_set_context("device", device);
        // Setup transaction
        sentry_set_transaction(name);
        // Add close guard
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() {
            sentry_close();
        });
        // Handle settings changing
        QObject::connect(&sharedSettings, &SharedSettings::telemetryChanged, [name, argv, autoSessionTracking](bool telemetry){
            Q_UNUSED(telemetry)
            O_DEBUG("Telemetry changed to" << telemetry);
            sentry_init(name, argv, autoSessionTracking);
        });
        QObject::connect(&sharedSettings, &SharedSettings::crashReportChanged, [name, argv, autoSessionTracking](bool crashReport){
            Q_UNUSED(crashReport)
            O_DEBUG("CrashReport changed to" << crashReport);
            sentry_init(name, argv, autoSessionTracking);
        });
#else
        Q_UNUSED(name);
        Q_UNUSED(argv);
        Q_UNUSED(autoSessionTracking);
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
    Transaction* start_transaction(const std::string& name, const std::string& action){
#ifdef SENTRY
        sentry_transaction_context_t* context = sentry_transaction_context_new(name.c_str(), action.c_str());
        // Hack to force transactions to be reported even though SAMPLE_RATE is 100%
        sentry_transaction_context_set_sampled(context, 1);
        sentry_transaction_t* transaction = sentry_transaction_start(context, sentry_value_new_null());
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
    void sentry_transaction(const std::string& name, const std::string& action, std::function<void(Transaction* transaction)> callback){
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
    Span* start_span(Transaction* transaction, const std::string& operation, const std::string& description){
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
    Span* start_span(Span* parent, const std::string& operation, const std::string& description){
#ifdef SENTRY
        if(parent == nullptr){
            return nullptr;
        }
        return new Span(sentry_span_start_child(parent->inner, (char*)operation.c_str(), (char*)description.c_str()));
#else
        Q_UNUSED(parent);
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
    void sentry_span(Transaction* transaction, const std::string& operation, const std::string& description, std::function<void()> callback){
#ifdef SENTRY
        sentry_span(transaction, operation, description, [callback](Span* s){
            Q_UNUSED(s);
            callback();
        });
#else
        Q_UNUSED(transaction);
        Q_UNUSED(operation);
        Q_UNUSED(description);
        callback();
#endif
    }
    void sentry_span(Transaction* transaction, const std::string& operation, const std::string& description, std::function<void(Span* span)> callback){
#ifdef SENTRY
        if(!sharedSettings.telemetry() || transaction == nullptr || transaction->inner == nullptr){
            callback(nullptr);
            return;
        }
        Span* span = start_span(transaction, operation, description);
        auto scopeGuard = qScopeGuard([span] {
            stop_span(span);
        });
        callback(span);
#else
        Q_UNUSED(transaction);
        Q_UNUSED(operation);
        Q_UNUSED(description);
        callback(nullptr);
#endif
    }
    void sentry_span(Span* parent, const std::string& operation, const std::string& description, std::function<void()> callback){
#ifdef SENTRY
        sentry_span(parent, operation, description, [callback](Span* s){
            Q_UNUSED(s);
            callback();
        });
#else
        Q_UNUSED(parent);
        Q_UNUSED(operation);
        Q_UNUSED(description);
        callback();
#endif
    }

    void sentry_span(Span* parent, const std::string& operation, const std::string& description, std::function<void(Span* span)> callback){
#ifdef SENTRY
            if(!sharedSettings.telemetry() || parent == nullptr || parent->inner == nullptr){
                callback(nullptr);
                return;
            }
            Span* span = start_span(parent, operation, description);
            auto scopeGuard = qScopeGuard([span] {
                stop_span(span);
            });
            callback(span);
#else
            Q_UNUSED(parent);
            Q_UNUSED(operation);
            Q_UNUSED(description);
            callback(nullptr);
#endif
    }
    void trigger_crash(){ memset((char *)invalid_mem, 1, 100); }
}
