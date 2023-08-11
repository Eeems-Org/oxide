#include <QMutex>
#include <QString>
#include <QVariant>

#include <thread>
#include <chrono>
#include <filesystem>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/fcntl.h>
#include <sys/socket.h>
#include <systemd/sd-journal.h>

static int(*func_open)(const char*, int, mode_t) = nullptr;
static QMutex logMutex;
static std::thread stateThread;
std::atomic<bool> stateThreadRunning = false;
static int stateFds[2] = {-1, -1};
static QMutex stateMutex;

// Use this instead of Oxide::getAppName to avoid recursion when logging in open()
QString appName(){
    static QString name;
    if(!name.isEmpty()){
        return name;
    }
    if(func_open == nullptr){
        return "liboxide_sysfs_preload.so";
    }
    int fd = func_open("/proc/self/comm", O_RDONLY, 0);
    if(fd != -1){
        FILE* f = fdopen(fd, "r");
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        char* where = new char[size];
        rewind(f);
        fread(where, sizeof(char), size, f);
        name = QString::fromLatin1(where, size);
        delete[] where;
    }
    if(!name.isEmpty()){
        return name;
    }
    name = QString::fromStdString(std::filesystem::canonical("/proc/self/exe").string());
    if(name.isEmpty()){
        return "liboxide_sysfs_preload.so";
    }
    return name;
}

template<class... Args>
void __printf(char const* file, unsigned int line, char const* func, int priority, Args... args){
    if(!qEnvironmentVariableIsSet("OXIDE_PRELOAD_DEBUG")){
        return;
    }
    QStringList list;
    ([&]{ list << QVariant(args).toString(); }(), ...);
    auto msg = list.join(' ');
    sd_journal_send(
        "MESSAGE=%s", msg.toStdString().c_str(),
        "PRIORITY=%i", priority,
        "CODE_FILE=%s", file,
        "CODE_LINE=%i", line,
        "CODE_FUNC=%s", func,
        "SYSLOG_IDENTIFIER=%s", appName().toStdString().c_str(),
        "SYSLOG_FACILITY=%s", "LOG_DAEMON",
        NULL
    );
    if(!isatty(fileno(stdin)) || !isatty(fileno(stdout)) || !isatty(fileno(stderr))){
        return;
    }
    QMutexLocker locker(&logMutex);
    Q_UNUSED(locker)
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
        appName().toStdString().c_str(),
        level.c_str(),
        msg.toStdString().c_str(),
        file,
        line,
        func
    );
}
#define _PRINTF(priority, ...) __printf("shared/preload-sysfs/main.cpp", __LINE__, __PRETTY_FUNCTION__, priority, __VA_ARGS__)
#define _DEBUG(...) _PRINTF(LOG_DEBUG, __VA_ARGS__)
#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)
#define _INFO(...) _PRINTF(LOG_INFO, __VA_ARGS__)
#define _CRIT(...) _PRINTF(LOG_CRIT, __VA_ARGS__)

void __thread_run(int fd){
    char line[PIPE_BUF];
    forever{
        int res = read(fd, line, PIPE_BUF);
        if(res == -1){
            if(errno == EINTR){
                continue;
            }
            if(errno == EAGAIN || errno == EIO){
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            _WARN("/sys/power/state pipe failed to read:", strerror(errno));
            _DEBUG("/sys/power/state pip fd:", fd);
            break;
        }
        if(res == 0){
            continue;
        }
        auto data = QString::fromStdString(std::string(line, res)).trimmed();
        if((QStringList() << "mem" << "freeze" << "standby").contains(data)){
            _INFO("Suspending system due to", data, "request");
            system("systemctl suspend");
        }else{
            _WARN("Unknown power state call:", data);
        }
    }
    stateThreadRunning = false;
}

int __open(const char* pathname, int flags){
    if(strcmp(pathname, "/sys/power/state") != 0){
        return -2;
    }
    stateMutex.lock();
    if(stateFds[0] != -1){
        if(!stateThreadRunning){
            stateThread.join();
            stateThreadRunning = true;
            stateThread = std::thread(__thread_run, stateFds[1]);
        }
        stateMutex.unlock();
        _INFO("Getting /sys/power/state pipe");
        return stateFds[0];
    }
    _INFO("Opening /sys/power/state pipe");
    int socketFlags = SOCK_STREAM;
    if((flags & O_NONBLOCK) || (flags & O_NDELAY)){
        socketFlags |= SOCK_NONBLOCK;
    }
    if(socketpair(AF_UNIX, socketFlags, 0, stateFds) == 0){
        stateThreadRunning = true;
        stateThread = std::thread(__thread_run, stateFds[1]);
        _INFO("/sys/power/state pipe opened");
    }else{
        _WARN("Unable to open /sys/power/state pipe:", strerror(errno));
    }
    stateMutex.unlock();
    return stateFds[0];
}

extern "C" {
    __attribute__((visibility("default")))
    int open64(const char* pathname, int flags, mode_t mode = 0){
        static const auto func_open64 = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");
        int fd = __open(pathname, flags);
        if(fd == -2){
            fd = func_open64(pathname, flags, mode);
        }
        return fd;
    }

    __attribute__((visibility("default")))
    int openat(int dirfd, const char* pathname, int flags, mode_t mode = 0){
        static const auto func_openat = (int(*)(int, const char*, int, mode_t))dlsym(RTLD_NEXT, "openat");
        int fd = __open(pathname, flags);
        if(fd == -2){
            DIR* save = opendir(".");
            fchdir(dirfd);
            char path[PATH_MAX+1];
            getcwd(path, PATH_MAX);
            fchdir(::dirfd(save));
            closedir(save);
            fd = __open(QString("%1/%2").arg(path, pathname).toStdString().c_str(), flags);
        }
        if(fd == -2){
            fd = func_openat(dirfd, pathname, flags, mode);
        }
        return fd;
    }

    __attribute__((visibility("default")))
    int open(const char* pathname, int flags, mode_t mode = 0){
        int fd = __open(pathname, flags);
        if(fd == -2){
            fd = func_open(pathname, flags, mode);
        }
        return fd;
    }

    void __attribute__ ((constructor)) init(void);
    void init(void){
        func_open = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");
    }
}
