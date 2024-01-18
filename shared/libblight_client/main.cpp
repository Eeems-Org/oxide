#include <iostream>
#include <chrono>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <mxcfb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <linux/fb.h>
#include <linux/input-event-codes.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <systemd/sd-journal.h>
#include <libblight.h>
#include <libblight/connection.h>

#include "input.h"

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
static bool DEBUG_LOGGING = false;
static bool FAILED_INIT = true;
static bool IS_XOCHITL = false;
static bool IS_LOADABLE_APP = false;
static bool DO_HANDLE_FB = true;
static int fbFd = -1;
static Blight::Connection* blightConnection = nullptr;
static ssize_t(*func_write)(int, const void*, size_t);
static ssize_t(*func_writev)(int, const iovec*, int);
static ssize_t(*func_writev64)(int, const iovec*, int);
static ssize_t(*func_pwrite)(int, const void*, size_t, int);
static ssize_t(*func_pwrite64)(int, const void*, size_t, int);
static ssize_t(*func_pwritev)(int, const iovec*, int, int);
static ssize_t(*func_pwritev64)(int, const iovec*, int, int);
static int(*func_open)(const char*, int, mode_t);
static int(*func_ioctl)(int, unsigned long, ...);
static int(*func_close)(int);

void __printf_header(int priority){
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
        "[%i:%i:%i libblight_client] %s: ",
        getpgrp(),
        getpid(),
        gettid(),
        level.c_str()
    );
}
void __printf_footer(char const* file, unsigned int line, char const* func){
    fprintf(
        stderr,
        " (%s:%u, %s)\n",
        file,
        line,
        func
    );
}
#define _DEBUG_ENABLED \
    DEBUG_LOGGING \
    && isatty(fileno(stdin)) \
    && isatty(fileno(stdout)) \
    && isatty(fileno(stderr))
#define _PRINTF(priority, fmt, ...) \
    if(_DEBUG_ENABLED){ \
        __printf_header(priority); \
        fprintf(stderr, fmt, __VA_ARGS__); \
        __printf_footer("shared/preload/main.cpp", __LINE__, __PRETTY_FUNCTION__); \
    }
#define _DEBUG(...) _PRINTF(LOG_DEBUG, __VA_ARGS__)
#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)
#define _INFO(...) _PRINTF(LOG_INFO, __VA_ARGS__)
#define _CRIT(...) _PRINTF(LOG_CRIT, __VA_ARGS__)


int fb_ioctl(unsigned long request, char* ptr){
    switch(request){
        // Look at linux/fb.h and mxcfb.h for more possible request types
        // https://www.kernel.org/doc/html/latest/fb/api.html
        case MXCFB_SEND_UPDATE:{
            _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SEND_UPDATE")
            ClockWatch cz;
            // mxcfb_update_data* update = reinterpret_cast<mxcfb_update_data*>(ptr);
            // auto region = update->update_region;
            // region.left
            // region.top
            // region.width
            // region.height
            _DEBUG("%s %f", "ioctl /dev/fb0 MXCFB_SEND_UPDATE done:", cz.elapsed())
            // TODO - notify on rM2 for screensharing
//            if(deviceSettings.getDeviceType() != Oxide::DeviceSettings::RM2){
//                return 0;
//            }
//            QString path = QFileInfo("/proc/self/exe").canonicalFilePath();
//            if(path.isEmpty() || path != "/usr/bin/xochitl"){
//                return 0;
//            }
            return 0;
        }
        case MXCFB_WAIT_FOR_UPDATE_COMPLETE:{
            _DEBUG("%s", "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE");
            ClockWatch cz;
            // mxcfb_update_marker_data* update = reinterpret_cast<mxcfb_update_marker_data*>(ptr);
            // update->update_marker;
            _DEBUG("%s %f", "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE done:", cz.elapsed());
            return 0;
        }
        case FBIOGET_VSCREENINFO:{
            _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_VSCREENINFO");
            fb_var_screeninfo* screenInfo = reinterpret_cast<fb_var_screeninfo*>(ptr);
            int fd = func_open("/dev/fb0", O_RDONLY, 0);
            if(fd == -1){
                return -1;
            }
            if(func_ioctl(fd, request, ptr) == -1){
                return -1;
            }
            // screenInfo->xres = width();
            // screenInfo->yres = height();
            // screenInfo->xres_virtual = width();
            // screenInfo->yres_virtual = height();
            // screenInfo->bits_per_pixel = stride();
            screenInfo->grayscale = 0;
            // QImage::Format_RGB16
            screenInfo->red.offset = 11;
            screenInfo->red.length = 5;
            screenInfo->red.msb_right = 0;
            screenInfo->green.offset = 5;
            screenInfo->green.length = 6;
            screenInfo->green.msb_right = 0;
            screenInfo->blue.offset = 0;
            screenInfo->blue.length = 5;
            screenInfo->blue.msb_right = 0;
            return 0;
        }
        case FBIOGET_FSCREENINFO:{
            _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_FSCREENINFO");
            fb_fix_screeninfo* screeninfo = reinterpret_cast<fb_fix_screeninfo*>(ptr);
            int fd = func_open("/dev/fb0", O_RDONLY, 0);
            if(fd == -1){
                // Failed to get the actual information, try to dummy our way with our own
                constexpr char fb_id[] = "mxcfb";
                memcpy(screeninfo->id, fb_id, sizeof(fb_id));
            }else if(func_ioctl(fd, request, ptr) == -1){
                return -1;
            }
            // screeninfo->smem_len = size();
            // screeninfo->smem_start = data();
            // screeninfo->line_length = stride();
            return 0;
        }
        case FBIOPUT_VSCREENINFO:{
            _DEBUG("%s", "ioctl /dev/fb0 FBIOPUT_VSCREENINFO");
            // TODO - Explore allowing some screen info updating
            // fb_var_screeninfo* screenInfo = reinterpret_cast<fb_fix_screeninfo*>(ptr);
            return 0;
        }
        case MXCFB_SET_AUTO_UPDATE_MODE:
            _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_AUTO_UPDATE_MODE");
            return 0;
        case MXCFB_SET_UPDATE_SCHEME:
            _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_UPDATE_SCHEME");
            return 0;
        case MXCFB_ENABLE_EPDC_ACCESS:
            _DEBUG("%s", "ioctl /dev/fb0 MXCFB_ENABLE_EPDC_ACCESS");
            return 0;
        case MXCFB_DISABLE_EPDC_ACCESS:
            _DEBUG("%s", "ioctl /dev/fb0 MXCFB_DISABLE_EPDC_ACCESS");
            return 0;
        default:
            _WARN(
                "%s %lu %c %lu %lu %lu",
                "UNHANDLED FB IOCTL ",
                _IOC_DIR(request),
                (char)_IOC_TYPE(request),
                _IOC_NR(request),
                _IOC_SIZE(request),
                request
            );
            return 0;
    }
}

int evdev_ioctl(const char* path, unsigned long request, char* ptr){
    static auto func_ioctl = (int(*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
    switch(request){
        case EVIOCGID:
        case EVIOCGREP:
        case EVIOCGVERSION:
        case EVIOCGKEYCODE:
        case EVIOCGKEYCODE_V2:
            int fd = func_open(path, O_RDONLY, 0);
            auto res = func_ioctl(fd, request, ptr);
            func_close(fd);
            return res;
    }
    unsigned int cmd = request;
    switch(EVIOC_MASK_SIZE(cmd)){
        case EVIOCGPROP(0):
        case EVIOCGMTSLOTS(0):
        case EVIOCGKEY(0):
        case EVIOCGLED(0):
        case EVIOCGSND(0):
        case EVIOCGSW(0):
        case EVIOCGNAME(0):
        case EVIOCGPHYS(0):
        case EVIOCGUNIQ(0):
        case EVIOC_MASK_SIZE(EVIOCSFF):
            int fd = func_open(path, O_RDONLY, 0);
            auto res = func_ioctl(fd, request, ptr);
            func_close(fd);
            return res;
    }

    if(_IOC_DIR(cmd) == _IOC_READ){
        if((_IOC_NR(cmd) & ~EV_MAX) == _IOC_NR(EVIOCGBIT(0, 0))){
            int fd = func_open(path, O_RDONLY, 0);
            auto res = func_ioctl(fd, request, ptr);
            func_close(fd);
            return res;
        }else if((_IOC_NR(cmd) & ~ABS_MAX) == _IOC_NR(EVIOCGABS(0))){
            int fd = func_open(path, O_RDONLY, 0);
            auto res = func_ioctl(fd, request, ptr);
            func_close(fd);
            return res;
        }
    }
    return -2;
}

// int touch_ioctl(unsigned long request, char* ptr){
//     switch(request){
//         case EVIOCGRAB: return 0;
//         case EVIOCREVOKE: return 0;
//         default:
//             auto res = evdev_ioctl(deviceSettings.getTouchDevicePath(), request, ptr);
//             if(res == -2){
//                 _WARN(
//                     "%s %lu %c %lu %lu %lu",
//                     "UNHANDLED TOUCH IOCTL ",
//                     _IOC_DIR(request),
//                     (char)_IOC_TYPE(request),
//                     _IOC_NR(request),
//                     _IOC_SIZE(request),
//                     request
//                 );
//                 return -1;
//             }
//             return res;
//     }
// }

// int tablet_ioctl(unsigned long request, char* ptr){
//     switch(request){
//         case EVIOCGRAB: return 0;
//         case EVIOCREVOKE: return 0;
//         default:
//             auto res = evdev_ioctl(deviceSettings.getWacomDevicePath(), request, ptr);
//             if(res == -2){
//                 _WARN(
//                     "%s %lu %c %lu %lu %lu",
//                     "UNHANDLED WACOM IOCTL ",
//                     _IOC_DIR(request),
//                     (char)_IOC_TYPE(request),
//                     _IOC_NR(request),
//                     _IOC_SIZE(request),
//                     request
//                 );
//                 return -1;
//             }
//             return res;
//     }
// }

// int key_ioctl(unsigned long request, char* ptr){
//     switch(request){
//         case EVIOCGRAB: return 0;
//         case EVIOCREVOKE: return 0;
//         default:
//             auto res = evdev_ioctl(deviceSettings.getButtonsDevicePath(), request, ptr);
//             if(res == -2){
//                 _WARN(
//                     "%s %lu %c %lu %lu %lu",
//                     "UNHANDLED KEY IOCTL ",
//                     _IOC_DIR(request),
//                     (char)_IOC_TYPE(request),
//                     _IOC_NR(request),
//                     _IOC_SIZE(request),
//                     request
//                 );
//                 return -1;
//             }
//             return res;
//     }
// }

int open_from_blight(const char* pathname){
    if(!IS_INITIALIZED){
        return -2;
    }
    int res = -2;
    if(DO_HANDLE_FB && strcmp(pathname, "/dev/fb0") == 0){
        // TODO figure this out dynamically
        auto buf = Blight::createBuffer(0, 0, 260, 1408, 520, Blight::Format::Format_RGB16);
        res = fbFd = buf->fd;
    }
    if(res == -1){
        errno = EIO;
    }
    return res;
}

extern "C" {
    __attribute__((visibility("default")))
    int open64(const char* pathname, int flags, mode_t mode = 0){
        static const auto func_open64 = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");
        _DEBUG("%s %s", "open64", pathname);
        if(!IS_INITIALIZED){
            int fd = func_open64(pathname, flags, mode);
            _DEBUG("%s %s %s %d", "opened", pathname, "with fd", fd);
            return fd;
        }
        int fd = open_from_blight(pathname);
        if(fd == -2){
            fd = func_open64(pathname, flags, mode);
        }
        _DEBUG("%s %s %s %d", "opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int openat(int dirfd, const char* pathname, int flags, mode_t mode = 0){
        static const auto func_openat = (int(*)(int, const char*, int, mode_t))dlsym(RTLD_NEXT, "openat");
        if(!IS_INITIALIZED){
            int fd = func_openat(dirfd, pathname, flags, mode);
            _DEBUG("%s %s %s %d", "opened", pathname, "with fd", fd);
            return fd;
        }
        _DEBUG("%s %s", "openat", pathname);
        int fd = open_from_blight(pathname);
        if(fd == -2){
            DIR* save = opendir(".");
            fchdir(dirfd);
            char path[PATH_MAX+1];
            getcwd(path, PATH_MAX);
            fchdir(::dirfd(save));
            closedir(save);
            std::string fullpath(path);
            fullpath += "/";
            fullpath += pathname;
            fd = open_from_blight(fullpath.c_str());
        }
        if(fd == -2){
            fd = func_openat(dirfd, pathname, flags, mode);
        }
        _DEBUG("%s %s %s %d", "opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int open(const char* pathname, int flags, mode_t mode = 0){
        if(!IS_INITIALIZED){
            int fd = func_open(pathname, flags, mode);;
            _DEBUG("%s %s %s %d", "opened", pathname, "with fd", fd);
            return fd;
        }
        _DEBUG("%s %s", "open", pathname);
        int fd = open_from_blight(pathname);
        if(fd == -2){
            fd = func_open(pathname, flags, mode);
        }
        _DEBUG("%s %s %s %d", "opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int close(int fd){
        if(IS_INITIALIZED){
            _DEBUG("%s %d", "close", fd);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
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
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                return fb_ioctl(request, ptr);
            }
        }
        return func_ioctl(fd, request, ptr);
    }

    __attribute__((visibility("default")))
    ssize_t _write(int fd, const void* buf, size_t n){
        if(fd < 3){
            // No need to debug stdout/stderr writes
            return func_write(fd, buf, n);
        }
        if(IS_INITIALIZED){
            _DEBUG("%s %d %zu", "write", fd, n);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                auto res = func_write(fd, buf, n);
                return res;
            }
        }
        return func_write(fd, buf, n);
    }
    __asm__(".symver _write, write@GLIBC_2.4");

    __attribute__((visibility("default")))
    ssize_t _writev(int fd, const iovec* iov, int iovcnt){
        if(fd < 3){
            // No need to debug stdout/stderr writes
            return func_writev(fd, iov, iovcnt);
        }
        if(IS_INITIALIZED){
            _DEBUG("%s %d %d", "writev", fd, iovcnt);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                auto res = func_writev(fd, iov, iovcnt);
                return res;
            }
        }
        return func_writev(fd, iov, iovcnt);
    }
    __asm__(".symver _writev, writev@GLIBC_2.4");

    __attribute__((visibility("default")))
    ssize_t _writev64(int fd, const iovec* iov, int iovcnt){
        if(fd < 3){
            // No need to debug stdout/stderr writes
            return func_writev64(fd, iov, iovcnt);
        }
        if(IS_INITIALIZED){
            _DEBUG("%s %d %d", "writev64", fd, iovcnt);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                auto res = func_writev64(fd, iov, iovcnt);
                return res;
            }
        }
        return func_writev64(fd, iov, iovcnt);
    }
    __asm__(".symver _writev64, writev64@GLIBC_2.4");

    __attribute__((visibility("default")))
    ssize_t _pwrite(int fd, const void* buf, size_t n, int offset){
        if(fd < 3){
            // No need to debug stdout/stderr writes
            return func_pwrite(fd, buf, n, offset);
        }
        if(IS_INITIALIZED){
            _DEBUG("%s %d %zu %d", "pwrite", fd, n, offset);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                auto res = func_pwrite(fd, buf, n, offset);
                return res;
            }
        }
        return func_pwrite(fd, buf, n, offset);
    }
    __asm__(".symver _pwrite, pwrite@GLIBC_2.4");

    __attribute__((visibility("default")))
    ssize_t _pwrite64(int fd, const void* buf, size_t n, int offset){
        if(fd < 3){
            // No need to debug stdout/stderr writes
            return func_pwrite64(fd, buf, n, offset);
        }
        if(IS_INITIALIZED){
            _DEBUG("%s %d %zu %d", "pwrite64", fd, n, offset);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                auto res = func_pwrite64(fd, buf, n, offset);
                return res;
            }
        }
        return func_pwrite64(fd, buf, n, offset);
    }
    __asm__(".symver _pwrite64, pwrite64@GLIBC_2.4");

    __attribute__((visibility("default")))
    ssize_t _pwritev(int fd, const iovec* iov, int iovcnt, int offset){
        if(fd < 3){
            // No need to debug stdout/stderr writes
            return func_pwritev(fd, iov, iovcnt, offset);
        }
        if(IS_INITIALIZED){
            _DEBUG("%s %d %d %d", "pwritev", fd, iovcnt, offset);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                auto res = func_pwritev(fd, iov, iovcnt, offset);
                return res;
            }
        }
        return func_pwritev(fd, iov, iovcnt, offset);
    }
    __asm__(".symver _pwritev, pwritev@GLIBC_2.4");

    __attribute__((visibility("default")))
    ssize_t _pwritev64(int fd, const iovec* iov, int iovcnt, int offset){
        if(fd < 3){
            // No need to debug stdout/stderr writes
            return func_pwritev64(fd, iov, iovcnt, offset);
        }
        if(IS_INITIALIZED){
            _DEBUG("%s %d %d %d", "pwritev64", fd, iovcnt, offset);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                auto res = func_pwritev64(fd, iov, iovcnt, offset);
                return res;
            }
        }
        return func_pwritev64(fd, iov, iovcnt, offset);
    }
    __asm__(".symver _pwritev64, pwritev64@GLIBC_2.4");

    void __attribute__ ((constructor)) init(void);
    void init(void){
        func_write = (ssize_t(*)(int, const void*, size_t))dlvsym(RTLD_NEXT, "write", "GLIBC_2.4");
        func_writev = (ssize_t(*)(int, const iovec*, int))dlvsym(RTLD_NEXT, "writev", "GLIBC_2.4");
        func_writev64 = (ssize_t(*)(int, const iovec*, int))dlvsym(RTLD_NEXT, "writev64", "GLIBC_2.4");
        func_pwrite = (ssize_t(*)(int, const void*, size_t, int))dlvsym(RTLD_NEXT, "pwrite", "GLIBC_2.4");
        func_pwrite64 = (ssize_t(*)(int, const void*, size_t, int))dlvsym(RTLD_NEXT, "pwrite64", "GLIBC_2.4");
        func_pwritev = (ssize_t(*)(int, const iovec*, int, int))dlvsym(RTLD_NEXT, "pwritev", "GLIBC_2.4");
        func_pwritev64 = (ssize_t(*)(int, const iovec*, int, int))dlvsym(RTLD_NEXT, "pwritev64", "GLIBC_2.4");
        func_open = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");
        func_ioctl = (int(*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
        func_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
    }

    static void _libhook_init() __attribute__((constructor));
    static void _libhook_init(){
        DEBUG_LOGGING = getenv("OXIDE_PRELOAD_DEBUG") != nullptr;
        DO_HANDLE_FB = getenv("OXIDE_PRELOAD_EXPOSE_FB") == nullptr;
        _DEBUG("%s %d", "Handle framebuffer:", DO_HANDLE_FB);
        std::string path(realpath("/proc/self/exe", NULL));
        IS_XOCHITL = path == "/usr/bin/xochitl";
        IS_LOADABLE_APP = IS_XOCHITL || path.empty() || path.starts_with("/opt") || path.starts_with("/home");
        if(IS_LOADABLE_APP){
            auto spid = getenv("OXIDE_PRELOAD");
            bool doConnect = spid == nullptr;
            if(!doConnect){
                try{
                    auto epgid = std::stoi(spid);
                    doConnect = epgid != getpgrp();
                }
                catch(std::invalid_argument&){}
                catch(std::out_of_range&){}
            }
            if(doConnect){
                auto pid = getpid();
                _DEBUG("%s %d %s", "Connecting", pid, "to blight");
                auto fd = Blight::open();
                // TODO - handle error if invalid fd
                blightConnection = new Blight::Connection(fd);
                blightConnection->onDisconnect([](int res){
                    // TODO - handle reconnect or force quit
                });
                _DEBUG("%s %d %s", "Connected", pid, "to blight");
            }
        }
        FAILED_INIT = false;
        IS_INITIALIZED = IS_LOADABLE_APP;
        setenv("OXIDE_PRELOAD", std::to_string(getpgrp()).c_str(), true);
        _DEBUG("%s %d", "Should handle app:", IS_LOADABLE_APP);
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
        return func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
    }
}
