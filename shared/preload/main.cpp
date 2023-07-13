#include <iostream>
#include <chrono>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <mxcfb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <linux/fb.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <liboxide/meta.h>
#include <liboxide/debug.h>
#include <liboxide/devicesettings.h>
#include <liboxide/tarnish.h>
#include <systemd/sd-journal.h>

// Don't import <linux/input.h> to avoid conflicting declarations
#define EVIOCGRAB       _IOW('E', 0x90, int)			/* Grab/Release device */
#define EVIOCREVOKE     _IOW('E', 0x91, int)			/* Revoke device access */
#define EVIOCGNAME(len) _IOC(_IOC_READ, 'E', 0x06, len)	/* get device name */

class ClockWatch {
public:
    std::chrono::high_resolution_clock::time_point t1;

    ClockWatch(){ t1 = std::chrono::high_resolution_clock::now(); }

    auto elapsed(){
        auto t2 = std::chrono::high_resolution_clock::now();
        auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
        return time_span.count();
    }
};

static bool IS_INITIALIZED = false;
thread_local bool IS_OPENING = false;
static bool DEBUG_LOGGING = false;
static bool FAILED_INIT = false;
static int fbFd = -1;
static int touchFd = -1;
static int tabletFd = -1;
static int keyFd = -1;
static ssize_t(*func_write)(int, const void*, size_t);
static int(*func_open)(const char*, int, mode_t);
static int(*func_ioctl)(int, unsigned long, ...);
static QMutex logMutex;
QString appName(){
    if(!qApp->startingUp()){
        return qApp->applicationName();
    }
    static QString name;
    if(!name.isEmpty()){
        return name;
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
    name = QFileInfo("/proc/self/exe").canonicalFilePath();
    if(name.isEmpty()){
        return "libtarnish_preload.so";
    }
    return name;
}

template<class... Args>
void __printf(char const* file, int line, char const* func, int priority, Args... args){
    if(!DEBUG_LOGGING){
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
        "[%i %i %s] %s: %s (%s:%u, %s)\n",
        getpid(),
        getpgrp(),
        appName().toStdString().c_str(),
        level.c_str(),
        msg.toStdString().c_str(),
        file,
        line,
        func
    );
}
#define _PRINTF(priority, ...) __printf("shared/preload/main.cpp", __LINE__, __PRETTY_FUNCTION__, priority, __VA_ARGS__)
#define _DEBUG(...) _PRINTF(LOG_DEBUG, __VA_ARGS__)
#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)
#define _INFO(...) _PRINTF(LOG_INFO, __VA_ARGS__)
#define _CRIT(...) _PRINTF(LOG_CRIT, __VA_ARGS__)

static struct sigaction int_action;
static struct sigaction term_action;
static struct sigaction segv_action;
static struct sigaction abrt_action;
void __sigacton__handler(int signo, siginfo_t* info, void* context){
    Q_UNUSED(info)
    Q_UNUSED(context)
    switch(signo){
        case SIGSEGV:
            sigaction(signo, &segv_action, NULL);
            break;
        case SIGTERM:
            sigaction(signo, &term_action, NULL);
            break;
        case SIGINT:
            sigaction(signo, &int_action, NULL);
            break;
        case SIGABRT:
            sigaction(signo, &abrt_action, NULL);
            break;
            break;
        default:
            signal(signo, SIG_DFL);
    }
    _DEBUG("Signal", strsignal(signo), "recieved.");
    Oxide::Tarnish::disconnect();
    if(!QCoreApplication::startingUp()){
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    raise(signo);
}

bool __sigaction_connect(int signo){
    switch(signo){
        case SIGSEGV:
            sigaction(signo, NULL, &segv_action);
            break;
        case SIGTERM:
            sigaction(signo, NULL, &term_action);
            break;
        case SIGINT:
            sigaction(signo, NULL, &int_action);
            break;
        case SIGABRT:
            sigaction(signo, NULL, &abrt_action);
            break;
        default:
            return false;
    }
    struct sigaction action;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    action.sa_sigaction = &__sigacton__handler;
    return sigaction(signo, &action, NULL) != -1;
}

int fb_ioctl(unsigned long request, char* ptr){
    switch(request){
        // Look at linux/fb.h and mxcfb.h for more possible request types
        // https://www.kernel.org/doc/html/latest/fb/api.html
        case MXCFB_SEND_UPDATE:{
            _DEBUG("ioctl /dev/fb0 MXCFB_SEND_UPDATE");
            mxcfb_update_data* update = reinterpret_cast<mxcfb_update_data*>(ptr);
            auto region = update->update_region;
            // TODO - Add support for recommending these
            //update->update_mode;
            //update->dither_mode;
            //update->temp;
            //update->update_marker;
            Oxide::Tarnish::screenUpdate(QRect(region.top, region.left, region.width, region.height), (EPFrameBuffer::WaveformMode)update->waveform_mode);
            if(deviceSettings.getDeviceType() != Oxide::DeviceSettings::RM2){
                return 0;
            }
            QString path = QFileInfo("/proc/self/exe").canonicalFilePath();
            if(path.isEmpty() || path != "/usr/bin/xochitl"){
                return 0;
            }
            // TODO - notify on rM2 for screensharing
            return 0;
        }
        case MXCFB_WAIT_FOR_UPDATE_COMPLETE:{
            _DEBUG("ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE");
            ClockWatch cz;
            Oxide::Tarnish::waitForLastUpdate();
            _DEBUG("FINISHED WAIT IOCTL", cz.elapsed());
            return 0;
        }
        case FBIOGET_VSCREENINFO:{
            _DEBUG("ioctl /dev/fb0 FBIOGET_VSCREENINFO");
            auto frameBuffer = Oxide::Tarnish::frameBufferImage(QImage::Format_RGB16);
            auto pixelFormat = frameBuffer.pixelFormat();
            fb_var_screeninfo* screenInfo = reinterpret_cast<fb_var_screeninfo*>(ptr);
            screenInfo->xres = frameBuffer.width();
            screenInfo->yres = frameBuffer.height();
            screenInfo->bits_per_pixel = pixelFormat.bitsPerPixel();
            screenInfo->grayscale = 0;
            screenInfo->xres_virtual = frameBuffer.width();
            screenInfo->yres_virtual = frameBuffer.height();
            // QImage::Format_RGB16
            screenInfo->red.offset = 11;
            screenInfo->red.length = 5;
            screenInfo->green.offset = 5;
            screenInfo->green.length = 6;
            screenInfo->blue.offset = 0;
            screenInfo->blue.length = 5;
            Q_UNUSED(screenInfo)
            Q_UNUSED(frameBuffer)
            return 0;
        }
        case FBIOGET_FSCREENINFO:{
            _DEBUG("ioctl /dev/fb0 FBIOGET_FSCREENINFO");
            fb_fix_screeninfo* screeninfo = reinterpret_cast<fb_fix_screeninfo*>(ptr);
            int fd = func_open("/dev/fb0", O_RDONLY, 0);
            if(fd != -1){
                func_ioctl(fd, request, ptr);
            }else{
                // Failed to get the actual information, try to dummy our way with our own
                constexpr char fb_id[] = "mxcfb";
                memcpy(screeninfo->id, fb_id, sizeof(fb_id));
            }
            auto frameBuffer = Oxide::Tarnish::frameBufferImage(QImage::Format_RGB16);
            screeninfo->smem_len = frameBuffer.sizeInBytes();
            screeninfo->smem_start = (unsigned long)Oxide::Tarnish::frameBuffer(QImage::Format_RGB16);
            screeninfo->line_length = frameBuffer.bytesPerLine();
            return 0;
        }
        case FBIOPUT_VSCREENINFO:
            _DEBUG("ioctl /dev/fb0 FBIOPUT_VSCREENINFO");
            // TODO - Explore allowing some screen info updating
            //fb_fix_screeninfo* screeninfo = reinterpret_cast<fb_fix_screeninfo*>(ptr);
            //errno = EACCES;
            //return -1;
            return 0;
        case MXCFB_SET_AUTO_UPDATE_MODE:
            _DEBUG("ioctl /dev/fb0 MXCFB_SET_AUTO_UPDATE_MODE");
            return 0;
        case MXCFB_SET_UPDATE_SCHEME:
            _DEBUG("ioctl /dev/fb0 MXCFB_SET_UPDATE_SCHEME");
            return 0;
        case MXCFB_ENABLE_EPDC_ACCESS:
            _DEBUG("ioctl /dev/fb0 MXCFB_ENABLE_EPDC_ACCESS");
            return 0;
        case MXCFB_DISABLE_EPDC_ACCESS:
            _DEBUG("ioctl /dev/fb0 MXCFB_DISABLE_EPDC_ACCESS");
            return 0;
        default:
            _WARN(
                "UNHANDLED FB IOCTL ",
                QString::number(_IOC_DIR(request)),
                (char)_IOC_TYPE(request),
                QString::number(_IOC_NR(request), 16),
                QString::number(_IOC_SIZE(request)),
                QString::number(request)
            );
            return -1;
    }
}

int touch_ioctl(unsigned long request, char* ptr){
    static auto func_ioctl = (int(*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
    switch(request){
        case EVIOCGNAME(sizeof(char[256])):{
            _DEBUG("ioctl touch EVIOCGNAME");
            int fd = func_open(deviceSettings.getTouchDevicePath(), O_RDONLY, 0);
            if(fd != -1){
                return func_ioctl(fd, request, ptr);
            }
            _WARN("Failed to open touch device for ioctl:", strerror(errno));
            return -1;
        }
        case EVIOCGRAB: return 0;
        case EVIOCREVOKE: return 0;
        default:
            _WARN(
                "UNHANDLED TOUCH IOCTL ",
                QString::number(_IOC_DIR(request)),
                (char)_IOC_TYPE(request),
                QString::number(_IOC_NR(request), 16),
                QString::number(_IOC_SIZE(request)),
                QString::number(request)
            );
            return -1;
    }
}

int tablet_ioctl(unsigned long request, char* ptr){
    switch(request){
        case EVIOCGNAME(sizeof(char[256])):{
            _DEBUG("ioctl tablet EVIOCGNAME");
            int fd = func_open(deviceSettings.getWacomDevicePath(), O_RDONLY, 0);
            if(fd != -1){
                return func_ioctl(fd, request, ptr);
            }
            _WARN("Failed to open wacom device for ioctl:", strerror(errno));
            return -1;
        }
        case EVIOCGRAB: return 0;
        case EVIOCREVOKE: return 0;
        default:
            _WARN(
                "UNHANDLED TABLET IOCTL ",
                QString::number(_IOC_DIR(request)),
                (char)_IOC_TYPE(request),
                QString::number(_IOC_NR(request), 16),
                QString::number(_IOC_SIZE(request)),
                QString::number(request)
            );
            return -1;
    }
}

int key_ioctl(unsigned long request, char* ptr){
    switch(request){
        case EVIOCGNAME(sizeof(char[256])):{
            _DEBUG("ioctl key EVIOCGNAME");
            int fd = func_open(deviceSettings.getButtonsDevicePath(), O_RDONLY, 0);
            if(fd != -1){
                return func_ioctl(fd, request, ptr);
            }
            _WARN("Failed to open key device for ioctl:", strerror(errno));
            return -1;
        }
        case EVIOCGRAB: return 0;
        case EVIOCREVOKE: return 0;
        default:
            _WARN(
                "UNHANDLED KEY IOCTL ",
                QString::number(_IOC_DIR(request)),
                (char)_IOC_TYPE(request),
                QString::number(_IOC_NR(request), 16),
                QString::number(_IOC_SIZE(request)),
                QString::number(request)
            );
            return -1;
    }
}

int open_from_tarnish(const char* pathname){
    if(!IS_INITIALIZED || IS_OPENING){
        return -2;
    }
    IS_OPENING = true;
    int res = -2;
    if(pathname == std::string("/dev/fb0")){
        res = fbFd = Oxide::Tarnish::getFrameBufferFd(QImage::Format_RGB16);
    }else if(pathname == deviceSettings.getTouchDevicePath()){
        res = touchFd = Oxide::Tarnish::getTouchEventPipeFd();
    }else if(pathname == deviceSettings.getWacomDevicePath()){
        res = tabletFd = Oxide::Tarnish::getTabletEventPipeFd();
    }else if(pathname == deviceSettings.getButtonsDevicePath()){
        res = keyFd = Oxide::Tarnish::getKeyEventPipeFd();
    }
    IS_OPENING = false;
    return res;
}

extern "C" {
    __attribute__((visibility("default")))
    void _ZN6QImageC1EiiNS_6FormatE(void* that, int x, int y, int f){
        static bool FIRST_ALLOC = true;
        static const auto qImageCtor = (void(*)(void*, int, int, int))dlsym(RTLD_NEXT, "_ZN6QImageC1EiiNS_6FormatE");
        static const auto qImageCtorWithBuffer = (void(*)(
            void*,
            uint8_t*,
            int32_t,
            int32_t,
            int32_t,
            int,
            void(*)(void*),
            void*
        ))dlsym(RTLD_NEXT, "_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_");
        auto frameBuffer = Oxide::Tarnish::frameBufferImage(QImage::Format_RGB16);
        if(!FIRST_ALLOC || x != frameBuffer.width() || y != frameBuffer.height()){
            qImageCtor(that, x, y, f);
            return;
        }
        _DEBUG("REPLACING THE IMAGE with shared memory");
        FIRST_ALLOC = false;
        qImageCtorWithBuffer(
            that,
            (uint8_t *)Oxide::Tarnish::frameBuffer(QImage::Format_RGB16),
            frameBuffer.width(),
            frameBuffer.height(),
            frameBuffer.sizeInBytes(),
            frameBuffer.format(),
            nullptr,
            nullptr
        );
    }

    __attribute__((visibility("default")))
    int open64(const char* pathname, int flags, mode_t mode = 0){
        static const auto func_open64 = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");
        if(!IS_INITIALIZED){
            return func_open64(pathname, flags, mode);
        }
        _DEBUG("open64", pathname);
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            fd = func_open64(pathname, flags, mode);
        }
        _DEBUG("opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int openat(int dirfd, const char* pathname, int flags, mode_t mode = 0){
        static const auto func_openat = (int(*)(int, const char*, int, mode_t))dlsym(RTLD_NEXT, "openat");
        if(!IS_INITIALIZED){
            return func_openat(dirfd, pathname, flags, mode);
        }
        _DEBUG("openat", pathname);
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            DIR* save = opendir(".");
            fchdir(dirfd);
            char path[PATH_MAX+1];
            getcwd(path, PATH_MAX);
            fchdir(::dirfd(save));
            closedir(save);
            fd = open_from_tarnish(QString("%1/%2").arg(path, pathname).toStdString().c_str());
        }
        if(fd == -2){
            fd = func_openat(dirfd, pathname, flags, mode);
        }
        _DEBUG("opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int open(const char* pathname, int flags, mode_t mode = 0){
        if(!IS_INITIALIZED){
            return func_open(pathname, flags, mode);;
        }
        _DEBUG("open", pathname);
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            fd = func_open(pathname, flags, mode);
        }
        _DEBUG("opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int close(int fd){
        if(IS_INITIALIZED){
            _DEBUG("close", fd);
            if(fbFd != -1 && fd == fbFd){
                // Maybe actually close it?
                return 0;
            }
            if(touchFd != -1 && fd == touchFd){
                // Maybe actually close it?
                return 0;
            }
            if(tabletFd != -1 && fd == tabletFd){
                // Maybe actually close it?
                return 0;
            }
            if(keyFd != -1 && fd == keyFd){
                // Maybe actually close it?
                return 0;
            }
        }
        static const auto func_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
        return func_close(fd);
    }

    __attribute__((visibility("default")))
    int ioctl(int fd, unsigned long request, char* ptr){
        if(IS_INITIALIZED){
            if(fbFd != -1 && fd == fbFd){
                return fb_ioctl(request, ptr);
            }
            if(touchFd != -1 && fd == touchFd){
                return touch_ioctl(request, ptr);
            }
            if(tabletFd != -1 && fd == tabletFd){
                return tablet_ioctl(request, ptr);
            }
            if(keyFd != -1 && fd == keyFd){
                return key_ioctl(request, ptr);
            }
        }
        return func_ioctl(fd, request, ptr);
    }

    __attribute__((visibility("default")))
    ssize_t write(int fd, const void* buf, size_t n){
        if(IS_INITIALIZED){
            if(fd > 2){
                // No need to debug stdout/stderr writes
                _DEBUG("write", fd, n);
            }
            if(fbFd != -1 && fd == fbFd){
                Oxide::Tarnish::lockFrameBuffer();
                auto res = func_write(fd, buf, n);
                Oxide::Tarnish::unlockFrameBuffer();
                return res;
            }
            if(touchFd != -1 && fd == touchFd){
                errno = EBADF;
                return -1;
            }
            if(tabletFd != -1 && fd == tabletFd){
                errno = EBADF;
                return -1;
            }
            if(keyFd != -1 && fd == keyFd){
                errno = EBADF;
                return -1;
            }
        }
        return func_write(fd, buf, n);
    }
    __asm__(".symver write, write@GLIBC_2.4");

//    __attribute__((visibility("default")))
//    bool _Z7qputenvPKcRK10QByteArray(const char* name, const QByteArray& val) {
//        static const auto orig_fn = (bool(*)(const char*, const QByteArray&))dlsym(RTLD_NEXT, "_Z7qputenvPKcRK10QByteArray");
//        if(strcmp(name, "QMLSCENE_DEVICE") == 0 || strcmp(name, "QT_QUICK_BACKEND") == 0){
//            return orig_fn(name, "software");
//        }
//        if(strcmp(name, "QT_QPA_PLATFORM") == 0){
//            return orig_fn(name, "epaper:enable_fonts");
//        }
//        return orig_fn(name, val);
//    }

    void __attribute__ ((constructor)) init(void);
    void init(void){
        func_write = (ssize_t(*)(int, const void*, size_t))dlvsym(RTLD_NEXT, "write", "GLIBC_2.4");
        func_open = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");
        func_ioctl = (int(*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
    }

    static void _libhook_init() __attribute__((constructor));
    static void _libhook_init(){
        DEBUG_LOGGING = qEnvironmentVariableIsSet("OXIDE_PRELOAD_DEBUG");
        if(!__sigaction_connect(SIGSEGV)){
            _CRIT("Failed to connect SIGSEGV:", strerror(errno));
            FAILED_INIT = true;
            DEBUG_LOGGING = true;
            return;
        }
        if(!__sigaction_connect(SIGINT)){
            _CRIT("Failed to connect SIGINT:", strerror(errno));
            FAILED_INIT = true;
            DEBUG_LOGGING = true;
            return;
        }
        if(!__sigaction_connect(SIGTERM)){
            _CRIT("Failed to connect SIGTERM:", strerror(errno));
            FAILED_INIT = true;
            DEBUG_LOGGING = true;
            return;
        }
        if(!__sigaction_connect(SIGABRT)){
            _CRIT("Failed to connect SIGABRT:", strerror(errno));
            FAILED_INIT = true;
            DEBUG_LOGGING = true;
            return;
        }
        bool doConnect = !qEnvironmentVariableIsSet("OXIDE_PRELOAD");
        if(!doConnect){
            bool ok;
            auto epgid = qEnvironmentVariableIntValue("OXIDE_PRELOAD", &ok);
            doConnect = !ok || epgid != getpgrp();
        }
        if(doConnect){
            auto pid = QString::number(getpid());
            _DEBUG("Connecting", pid, "to tarnish");
            auto socket = Oxide::Tarnish::getSocket();
            if(socket == nullptr){
                FAILED_INIT = true;
                DEBUG_LOGGING = true;
                _CRIT("Failed to connect", pid, "to tarnish");
                return;
            }
            _DEBUG("Connected", pid, "to tarnish");
        }
        IS_INITIALIZED = true;
        qputenv("OXIDE_PRELOAD", QByteArray::number(getpgrp()));
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
        if(FAILED_INIT){
            return EXIT_FAILURE;
        }
        auto func_main = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");
        auto res = func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
        Oxide::Tarnish::disconnect();
        return res;
    }
}
