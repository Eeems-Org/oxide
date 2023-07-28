#include <QByteArray>
#include <QMutexLocker>
#include <QVariant>

#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <systemd/sd-journal.h>

static bool DEBUG_LOGGING = false;
static bool IS_INITIALIZED = false;
static QMutex logMutex;

std::string readlink_string(const char* link_path){
  char buffer[PATH_MAX];
  ssize_t len = readlink(link_path, buffer, sizeof(buffer) - 1);
  if(len == -1){
    return "";
  }
  buffer[len] = '\0';
  return buffer;
}

// Use this instead of Oxide::getAppName to avoid recursion when logging in open()
std::string appName(){
    static std::string name;
    if(!name.empty()){
        return name;
    }
    name = readlink_string("/proc/self/exe");
    if(name.empty()){
        return "liboxide_qt_preload.so";
    }
    return name;
}

template<class... Args>
void __printf(char const* file, unsigned int line, char const* func, int priority, Args... args){
    if(!DEBUG_LOGGING){
        return;
    }
    std::vector<std::string> list;
    ([&]{ list.push_back(QVariant(args).toString().toStdString()); }(), ...);
    std::string msg;
    for(auto item : list){
        if(msg.empty()){
            msg = item;
        }else{
            msg += ' ' + item;
        }
    }
    sd_journal_send(
        "MESSAGE=%s", msg.c_str(),
        "PRIORITY=%i", priority,
        "CODE_FILE=%s", file,
        "CODE_LINE=%i", line,
        "CODE_FUNC=%s", func,
        "SYSLOG_IDENTIFIER=%s", appName().c_str(),
        "SYSLOG_FACILITY=%s", "LOG_DAEMON",
        NULL
    );
    if(!isatty(fileno(stdin)) || !isatty(fileno(stdout)) || !isatty(fileno(stderr))){
        return;
    }
    QMutexLocker locker(&logMutex);
    Q_UNUSED(logMutex)
    std::string level;
    switch(priority){
        case LOG_INFO:
            level = "Info";
        break;
        case LOG_WARNING:
            level = "Warning";
        break;
        case LOG_CRIT:
            level = "Critical";
        break;
        default:
            level = "Debug";
    }
    fprintf(
        stderr,
        "[%i:%i:%i %s] %s: %s (%s:%u, %s)\n",
        getpgrp(),
        getpid(),
        gettid(),
        appName().c_str(),
        level.c_str(),
        msg.c_str(),
        file,
        line,
        func
    );
}
#define _PRINTF(priority, ...) __printf("shared/preload-qt/main.cpp", __LINE__, __PRETTY_FUNCTION__, priority, __VA_ARGS__)
#define _DEBUG(...) _PRINTF(LOG_DEBUG, __VA_ARGS__)
#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)
#define _INFO(...) _PRINTF(LOG_INFO, __VA_ARGS__)
#define _CRIT(...) _PRINTF(LOG_CRIT, __VA_ARGS__)

extern "C" {
    __attribute__((visibility("default")))
    int setenv(const char* name, const char* value, int overwrite){
        static const auto orig_setenv = (bool(*)(const char*, const char*, int))dlsym(RTLD_NEXT, "setenv");
        if(IS_INITIALIZED){
            if(
               strcmp(name, "QMLSCENE_DEVICE") == 0
               || strcmp(name, "QT_QUICK_BACKEND") == 0
               || strcmp(name, "QT_QPA_PLATFORM") == 0
               || strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0
               || (strcmp(name, "QT_QPA_GENERIC_PLUGINS") == 0 && ::getenv("OXIDE_PRELOAD_FORCE_QSGEPAPER") == nullptr)
               || strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0
               || strcmp(name, "QT_QPA_EVDEV_MOUSE_PARAMETERS") == 0
               || strcmp(name, "QT_QPA_EVDEV_KEYBOARD_PARAMETERS") == 0
               || strcmp(name, "QT_PLUGIN_PATH") == 0
            ){
                _DEBUG("IGNORED setenv", name, value);
                return 0;
            }
        }
        _DEBUG("setenv", name, value);
        return orig_setenv(name, value, overwrite);
    }

    __attribute__((visibility("default")))
    int __libc_start_main(
        int(*_main)(int, char**, char**),
        int argc,
        char** argv,
        int(*init)(int, char**, char**),
        void(*fini)(void),
        void(*rtld_fini)(void),
        void* stack_end
    ){
        // Initialize environment
        if(qEnvironmentVariableIsSet("OXIDE_PRELOAD_FORCE_QSGEPAPER")){
            setenv("QMLSCENE_DEVICE", "epaper", 1);
            setenv("QT_QUICK_BACKEND", "epaper", 1);
        }else{
            setenv("QMLSCENE_DEVICE", "software", 1);
            setenv("QT_QUICK_BACKEND", "software", 1);
        }
        setenv("QT_QPA_PLATFORM", "oxide:enable_fonts", 1);
        if(!qEnvironmentVariableIsSet("OXIDE_PRELOAD_FORCE_QSGEPAPER")){
            setenv("QT_QPA_GENERIC_PLUGINS", "evdevtablet", 1);
        }
        setenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "", 1);
        setenv("QT_QPA_EVDEV_MOUSE_PARAMETERS", "", 1);
        setenv("QT_QPA_EVDEV_KEYBOARD_PARAMETERS", "", 1);
        setenv("QT_PLUGIN_PATH", "/opt/usr/lib/plugins", 1);
        IS_INITIALIZED = true;
        DEBUG_LOGGING = qEnvironmentVariableIsSet("OXIDE_PRELOAD_DEBUG");
        auto func_main = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");
        auto res = func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
        _DEBUG("Exit code:", QString::number(res));
        return res;
    }
}
