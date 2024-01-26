#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <chrono>
#include <sys/poll.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <mxcfb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <mutex>
#include <linux/fb.h>
#include <linux/input.h>
#include <asm/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <systemd/sd-journal.h>
#include <libblight.h>
#include <libblight/connection.h>
#include <libblight/debug.h>
#include <libblight/socket.h>
#include <filesystem>
#include <cstdint>

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
static bool FAILED_INIT = true;
static bool DO_HANDLE_FB = true;
static bool FAKE_RM1 = false;
static Blight::shared_buf_t blightBuffer = Blight::buf_t::new_ptr();
static Blight::Connection* blightConnection = nullptr;
static std::map<int, int[2]> inputFds;
static std::map<int, int> inputDeviceMap;
static ssize_t(*func_write)(int, const void*, size_t);
static ssize_t(*func_writev)(int, const iovec*, int);
static ssize_t(*func_writev64)(int, const iovec*, int);
static ssize_t(*func_pwrite)(int, const void*, size_t, int);
static ssize_t(*func_pwrite64)(int, const void*, size_t, int);
static ssize_t(*func_pwritev)(int, const iovec*, int, int);
static ssize_t(*func_pwritev64)(int, const iovec*, int, int);
static int(*func_open)(const char*, int, ...);
static int(*func_ioctl)(int, unsigned long, ...);
static int(*func_close)(int);
static int(*func_msgget)(key_t, int);
static void*(*func_mmap)(void*, size_t, int, int, int, __off_t);
static int msgq = -1;

void writeInputEvent(int fd, input_event* ie){
    if(!Blight::send_blocking(
        fd,
        reinterpret_cast<Blight::data_t>(ie),
        sizeof(input_event)
    )){
        _WARN("Failed to write input event", strerror(errno));
    }
}

void writeInputEvent(
    int fd,
    timeval time,
    short unsigned int type,
    short unsigned int code,
    int value
){
    input_event ie{
        .time = time,
        .type = type,
        .code = code,
        .value = value
    };
    writeInputEvent(fd, &ie);
}

int __open_input_socketpair(int flags, int fds[2]){
    int socketFlags = SOCK_STREAM;
    if(flags & O_NONBLOCK || flags & O_NDELAY){
        socketFlags |= SOCK_NONBLOCK;
    }
    return ::socketpair(AF_UNIX, socketFlags, 0, fds);
}


void __sendEvents(unsigned int device, std::vector<Blight::partial_input_event_t>& queue){
    if(queue.empty()){
        return;
    }
    timeval time;
    ::gettimeofday(&time, NULL);
    std::vector<input_event> data(queue.size());
    for(unsigned int i = 0; i < queue.size(); i++){
        auto& event = queue[i];
        data[i] = input_event{
            .time = time,
            .type = event.type,
            .code = event.code,
            .value = event.value
        };
    }
    auto fd = inputFds[device][0];
    if(!Blight::send_blocking(
        fd,
        reinterpret_cast<Blight::data_t>(data.data()),
        sizeof(input_event) * data.size()
    )){
        _CRIT("%d input events failed to send: %s", data.size(), std::strerror(errno));
    }else{
        _DEBUG("Sent %d input events", data.size());
    }
    queue.clear();
}


void __readInput(){
    _INFO("%s", "InputWorker starting");
    prctl(PR_SET_NAME, "InputWorker\0", 0, 0, 0);
    auto fd = blightConnection->input_handle();
    std::map<unsigned int,std::vector<Blight::partial_input_event_t>> events;
    while(blightConnection != nullptr){
        auto maybe = blightConnection->read_event();
        if(!maybe.has_value()){
            if(errno != EAGAIN && errno != EINTR){
                _WARN("[InputWorker] Failed to read message %s", std::strerror(errno));
                break;
            }
            for(auto& item : events){
                __sendEvents(item.first, item.second);
            }
            _DEBUG("Waiting for next input event");
            if(!Blight::wait_for_read(fd) && errno != EAGAIN){
                _WARN("[InputWorker] Failed to wait for next input event %s", std::strerror(errno));
                break;
            }
            continue;
        }
        const auto& device = maybe.value().device;
        if(!inputFds.contains(device)){
            _INFO("Ignoring event for unopened device: %d", device);
            continue;
        }
        if(inputFds[device][0] < 0){
            _INFO("Ignoring event for invalid device: %d", device);
            continue;
        }
        auto& event = maybe.value().event;
        auto& queue = events[device];
        queue.push_back(event);
        if(queue.size() > 16){
            __sendEvents(device, queue);
        }
    }
    _INFO("%s", "InputWorker quitting");
}


int __fb_ioctl(unsigned long request, char* ptr){
    switch(request){
        // Look at linux/fb.h and mxcfb.h for more possible request types
        // https://www.kernel.org/doc/html/latest/fb/api.html
        case MXCFB_SEND_UPDATE:{
            _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SEND_UPDATE")
            ClockWatch cz;
            if(blightBuffer->surface.empty()){
                Blight::addSurface(blightBuffer);
            }
            if(blightBuffer->surface.empty()){
                _CRIT("Failed to create surface: %s", std::strerror(errno));
                std::exit(errno);
            }
            auto update = reinterpret_cast<mxcfb_update_data*>(ptr);
            auto region = update->update_region;
            blightConnection->repaint(
                blightBuffer,
                region.left,
                region.top,
                region.width,
                region.height,
                (Blight::WaveformMode)update->waveform_mode,
                update->update_marker
            );
            _DEBUG("ioctl /dev/fb0 MXCFB_SEND_UPDATE done: %f", cz.elapsed())
            // TODO - notify on rM2 for screensharing
            return 0;
        }
        case MXCFB_WAIT_FOR_UPDATE_COMPLETE:{
            _DEBUG("%s", "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE");
            ClockWatch cz;
            auto update = reinterpret_cast<mxcfb_update_marker_data*>(ptr);
            blightConnection->waitForMarker(update->update_marker);
            _DEBUG("ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE done: %f", cz.elapsed());
            return 0;
        }
        case FBIOGET_VSCREENINFO:{
            _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_VSCREENINFO");
            auto screenInfo = reinterpret_cast<fb_var_screeninfo*>(ptr);
            int fd = func_open("/dev/fb0", O_RDONLY, 0);
            if(fd == -1){
                return -1;
            }
            if(func_ioctl(fd, request, ptr) == -1){
                return -1;
            }
            screenInfo->xres = blightBuffer->width;
            screenInfo->yres = blightBuffer->height;
            screenInfo->xoffset = 0;
            screenInfo->yoffset = 0;
            screenInfo->xres_virtual = blightBuffer->width;
            screenInfo->yres_virtual = blightBuffer->height;
            screenInfo->bits_per_pixel = blightBuffer->stride / blightBuffer->width * 8;
            // TODO - determine the following from the buffer format
            // Format_RGB16 / RGB565
            screenInfo->grayscale = 0;
            screenInfo->red.offset = 11;
            screenInfo->red.length = 5;
            screenInfo->red.msb_right = 0;
            screenInfo->green.offset = 5;
            screenInfo->green.length = 6;
            screenInfo->green.msb_right = 0;
            screenInfo->blue.offset = 0;
            screenInfo->blue.length = 5;
            screenInfo->blue.msb_right = 0;
            // TODO what should this even be?
            screenInfo->nonstd = 0;
            screenInfo->activate = 0;
            // It would be cool to have the actual mm width/height here
            screenInfo->height = 0;
            screenInfo->width = 0;
            screenInfo->accel_flags = 0;
            // screenInfo->pixclock = 28800; // Stick with what is reported
            screenInfo->left_margin = 0;
            screenInfo->right_margin = 0;
            screenInfo->upper_margin = 0;
            screenInfo->lower_margin = 0;
            screenInfo->hsync_len = 0;
            screenInfo->vsync_len = 2;
            screenInfo->sync = 0;
            screenInfo->vmode = 0;
            screenInfo->rotate = 0;
            screenInfo->colorspace = 0;
            screenInfo->reserved[0] = 0;
            screenInfo->reserved[1] = 0;
            screenInfo->reserved[2] = 0;
            screenInfo->reserved[3] = 0;
            return 0;
        }
        case FBIOGET_FSCREENINFO:{
            _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_FSCREENINFO");
            auto screenInfo = reinterpret_cast<fb_fix_screeninfo*>(ptr);
            int fd = func_open("/dev/fb0", O_RDONLY, 0);
            if(fd == -1){
                return -1;
            }
            if(func_ioctl(fd, request, ptr) == -1){
                return -1;
            }
            // TODO determine all the values dynamically
            screenInfo->smem_len = blightBuffer->size();
            screenInfo->smem_start = (unsigned long)blightBuffer->data;
            screenInfo->type = 0;
            screenInfo->type_aux = 0;
            screenInfo->visual = 0;
            screenInfo->xpanstep = 8;
            screenInfo->ypanstep = 0;
            screenInfo->ywrapstep = 5772;
            screenInfo->line_length = blightBuffer->stride;
            screenInfo->mmio_start = 0;
            screenInfo->mmio_len = 0;
            screenInfo->accel = 0;
            screenInfo->reserved[0] = 0;
            screenInfo->reserved[1] = 0;
            return 0;
        }
        case FBIOPUT_VSCREENINFO:{
            _DEBUG("%s", "ioctl /dev/fb0 FBIOPUT_VSCREENINFO");
            // TODO - Explore allowing some screen info updating
            auto screenInfo = reinterpret_cast<fb_var_screeninfo*>(ptr);
            _DEBUG(
                "\n"
                "res: %dx%d\n"
                "res_virtual: %dx%d\n"
                "offset: %d, %d\n"
                "bits_per_pixel: %d\n"
                "grayscale: %d\n"
                "red: %d, %d, %d\n"
                "green: %d, %d, %d\n"
                "blue: %d, %d, %d\n"
                "tansp: %d, %d, %d\n"
                "nonstd: %d\n"
                "activate: %d\n"
                "size: %dx%dmm\n"
                "accel_flags: %d\n"
                "pixclock: %d\n"
                "margins: %d %d %d %d\n"
                "hsync_len: %d\n"
                "vsync_len: %d\n"
                "sync: %d\n"
                "vmode: %d\n"
                "rotate: %d\n"
                "colorspace: %d\n"
                "reserved: %d, %d, %d, %d\n",
                screenInfo->xres,
                screenInfo->yres,
                screenInfo->xres_virtual,
                screenInfo->yres_virtual,
                screenInfo->xoffset,
                screenInfo->yoffset,
                screenInfo->bits_per_pixel,
                screenInfo->grayscale,
                screenInfo->red.offset,
                screenInfo->red.length,
                screenInfo->red.msb_right,
                screenInfo->green.offset,
                screenInfo->green.length,
                screenInfo->green.msb_right,
                screenInfo->blue.offset,
                screenInfo->blue.length,
                screenInfo->blue.msb_right,
                screenInfo->transp.offset,
                screenInfo->transp.length,
                screenInfo->transp.msb_right,
                screenInfo->nonstd,
                screenInfo->activate,
                screenInfo->height,
                screenInfo->width,
                screenInfo->accel_flags,
                screenInfo->pixclock,
                screenInfo->left_margin,
                screenInfo->right_margin,
                screenInfo->upper_margin,
                screenInfo->lower_margin,
                screenInfo->hsync_len,
                screenInfo->vsync_len,
                screenInfo->sync,
                screenInfo->vmode,
                screenInfo->rotate,
                screenInfo->colorspace,
                screenInfo->reserved[0],
                screenInfo->reserved[1],
                screenInfo->reserved[2],
                screenInfo->reserved[3]
            );
            // TODO allow resizing?
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
                "UNHANDLED Fb IOCTL %lu %c %lu %lu %lu",
                _IOC_DIR(request),
                (char)_IOC_TYPE(request),
                _IOC_NR(request),
                _IOC_SIZE(request),
                request
            );
            return 0;
    }
}

int __input_ioctlv(int fd, unsigned long request, char* ptr){
    switch(request){
        case EVIOCGRAB: return 0;
        case EVIOCREVOKE: return 0;
        default:
            return func_ioctl(fd, request, ptr);
    }
}

int __open(const char* pathname, int flags){
    if(!IS_INITIALIZED){
        return -2;
    }
    std::string actualpath(pathname);
    if(std::filesystem::exists(actualpath)){
        actualpath = realpath(pathname, NULL);
    }
    int res = -2;
    if(FAKE_RM1 && actualpath == "/sys/devices/soc0/machine"){
        int fd = memfd_create("machine", MFD_ALLOW_SEALING);
        auto data = "reMarkable 1.0";
        func_write(fd, data, sizeof(data));
        int flags = fcntl(fd, F_GET_SEALS);
        fcntl(
            fd,
            F_ADD_SEALS,
            flags
            | F_SEAL_SEAL
            | F_SEAL_SHRINK
            | F_SEAL_GROW
            | F_SEAL_WRITE
            | F_SEAL_FUTURE_WRITE
        );
        return fd;
    }else if(
        DO_HANDLE_FB
        && (
            actualpath == "/dev/fb0"
            || actualpath == "/dev/shm/swtfb.01"
        )
    ){
        if(blightBuffer->format != Blight::Format::Format_Invalid){
            return blightBuffer->fd;
        }
        auto surfaceIds = blightConnection->surfaces();
        if(!surfaceIds.empty()){
            for(auto& identifier : surfaceIds){
                auto maybe = blightConnection->getBuffer(identifier);
                if(!maybe.has_value()){
                    continue;
                }
                auto buffer = maybe.value();
                if(buffer->data == nullptr){
                    continue;
                }
                if(
                    buffer->x != 0
                    || buffer->y != 0
                    || buffer->width != 1404
                    || buffer->height != 1872
                    || buffer->stride != 2808
                    || buffer->format != Blight::Format::Format_RGB16
                ){
                    continue;
                }
                _INFO("Reusing existing surface: %s", identifier.c_str());
                blightBuffer = buffer;
                break;
            }
        }
        if(blightBuffer->format == Blight::Format::Format_Invalid){
            /// Emulate rM1 screen
            auto maybe = Blight::createBuffer(
                0,
                0,
                1404,
                1872,
                2808,
                Blight::Format::Format_RGB16
            );
            if(!maybe.has_value()){
                return -1;
            }
            blightBuffer = maybe.value();
            // We don't ever plan on resizing, and we shouldn't let anything try
            int flags = fcntl(blightBuffer->fd, F_GET_SEALS);
            fcntl(
                blightBuffer->fd,
                F_ADD_SEALS, flags | F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW
            );
            res = blightBuffer->fd;
            if(res < 0){
                _CRIT("Failed to create buffer: %s", std::strerror(errno));
                std::exit(errno);
            }else{
                _INFO("Created buffer %s on fd %d", blightBuffer->uuid.c_str(), blightBuffer->fd);
            }
            return res;
        }
    }else if(actualpath.starts_with("/dev/input/event")){
        _INFO("Opening event device: %s", actualpath.c_str());
        if(blightConnection->input_handle() > 0){
            if(inputFds.empty()){
                new std::thread(__readInput);
            }
            std::string path(basename(actualpath.c_str()));
            try{
                unsigned int device = stol(path.substr(5));
                if(inputFds.contains(device)){
                    res = inputFds[device][1];
                }else{
                    // TODO - what if they open it a second time with different flags?
                    res = __open_input_socketpair(flags, inputFds[device]);
                    if(res < 0){
                        inputFds.erase(device);
                        _WARN("Failed to open event socket stream %s", std::strerror(errno));
                    }else{
                        res = inputFds[device][1];
                        inputDeviceMap[res] = func_open(actualpath.c_str(), O_RDWR, 0);
                    }
                }
            }
            catch(std::invalid_argument&){
                _WARN("Resolved event device name invalid: %s", path.c_str());
                res = -1;
            }
            catch(std::out_of_range&){
                _WARN("Resolved event device number out of range: %s", path.c_str());
                res = -1;
            }
        }else{
            _WARN("Could not open connection input stream", "");
            res = -1;
        }
    }
    if(res == -1){
        errno = EIO;
    }
    return res;
}

namespace swtfb {
    struct xochitl_data {
        int x1;
        int y1;
        int x2;
        int y2;

        int waveform;
        int flags;
    };
    struct wait_sem_data {
        char sem_name[512];
    };
    struct swtfb_update {
        long mtype;
        struct {
            union {
                xochitl_data xochitl_update;
                struct mxcfb_update_data update;
                wait_sem_data wait_update;
            };
        } mdata;
    };
}
extern "C" {
    __attribute__((visibility("default")))
    int msgget(key_t key, int msgflg){
        static const auto func_msgget = (int(*)(key_t, int))dlsym(RTLD_NEXT, "msgget");
        if(!IS_INITIALIZED){
            return func_msgget(key, msgflg);
        }
        // Catch rm2fb ipc
        if(key == 0x2257c){
            // inject our own ipc here
            if(msgq == -1){
                msgq = func_msgget(0x2257d, msgflg);
            }
            return msgq;
        }
        return func_msgget(key, msgflg);
    }

    __attribute__((visibility("default")))
    int msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg){
        static const auto func_msgsnd = (int(*)(int, const void*, size_t, int))dlsym(RTLD_NEXT, "msgsnd");
        if(!IS_INITIALIZED){
            return func_msgsnd(msqid, msgp, msgsz, msgflg);
        }
        if(msqid == msgq){
            if(blightBuffer->surface.empty()){
                Blight::addSurface(blightBuffer);
            }
            if(blightBuffer->surface.empty()){
                _CRIT("Failed to create surface: %s", std::strerror(errno));
                std::exit(errno);
                return -1;
            }
            _DEBUG("%s", "rm2fb ipc repaint");
            auto buf = (swtfb::swtfb_update*)msgp;
            auto region = buf->mdata.update.update_region;
            blightConnection->repaint(
                blightBuffer,
                region.left,
                region.top,
                region.width,
                region.height
            );
            return 0;
        }
        return func_msgsnd(msqid, msgp, msgsz, msgflg);
    }

    __attribute__((visibility("default")))
    int open64(const char* pathname, int flags, ...){
        static const auto func_open64 = (int(*)(const char*, int, ...))dlsym(RTLD_NEXT, "open64");
        if(!IS_INITIALIZED){
            va_list args;
            va_start(args, flags);
            int fd = func_open64(pathname, flags, args);
            va_end(args);
            return fd;
        }
        _DEBUG("open64 %s", pathname);
        int fd = __open(pathname, flags);
        if(fd == -2){
            va_list args;
            va_start(args, flags);
            fd = func_open64(pathname, flags, args);
            va_end(args);
        }
        _DEBUG("opened %s with fd %d", pathname, fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int openat(int dirfd, const char* pathname, int flags, ...){
        static const auto func_openat = (int(*)(int, const char*, int, ...))dlsym(RTLD_NEXT, "openat");
        if(!IS_INITIALIZED){
            va_list args;
            va_start(args, flags);
            int fd = func_openat(dirfd, pathname, flags, args);
            va_end(args);
            return fd;
        }
        _DEBUG("openat %s", pathname);
        int fd = __open(pathname, flags);
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
            fd = __open(fullpath.c_str(), flags);
        }
        if(fd == -2){
            va_list args;
            va_start(args, flags);
            fd = func_openat(dirfd, pathname, flags, args);
            va_end(args);
        }
        _DEBUG("opened %s with fd %d", pathname, fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int open(const char* pathname, int flags, ...){
        if(!IS_INITIALIZED){
            va_list args;
            va_start(args, flags);
            int fd = func_open(pathname, flags, args);;
            va_end(args);
            return fd;
        }
        _DEBUG("open %s", pathname);
        int fd = __open(pathname, flags);
        if(fd == -2){
            va_list args;
            va_start(args, flags);
            fd = func_open(pathname, flags, args);
            va_end(args);
        }
        _DEBUG("opened %s with fd %d", pathname, fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int close(int fd){
        if(IS_INITIALIZED){
            _DEBUG("close %d", fd);
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
                // Maybe actually close it?
                return 0;
            }
            if(inputDeviceMap.contains(fd)){
                // Maybe actually close it?
                return 0;
            }
        }
        static const auto func_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
        return func_close(fd);
    }

    __attribute__((visibility("default")))
    int ioctl(int fd, unsigned long request, ...){
        va_list args;
        va_start(args, request);
        if(IS_INITIALIZED){
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
                char* ptr = va_arg(args, char*);
                int res = __fb_ioctl(request, ptr);
                va_end(args);
                return res;
            }
            if(inputDeviceMap.contains(fd)){
                char* ptr = va_arg(args, char*);
                int res = __input_ioctlv(inputDeviceMap[fd], request, ptr);
                va_end(args);
                return res;
            }
        }
        int res = func_ioctl(fd, request, args);
        va_end(args);
        return res;
    }

    __attribute__((visibility("default")))
    void* mmap(void* addr, size_t len, int prot, int flags, int fd, __off_t offset){
        if(IS_INITIALIZED){
            _DEBUG(
                "mmap 0x%04x %lld %d 0x%02x %d %d",
                reinterpret_cast<std::uintptr_t>(addr),
                len,
                prot,
                flags,
                fd,
                offset
            );
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
                if(len + offset > blightBuffer->size()){
                    _CRIT(
                        "Requested size + offset is larger than the buffer: %d",
                        blightBuffer->size()
                    );
                    errno = EINVAL;
                    return MAP_FAILED;
                }
                return &blightBuffer->data[offset];
            }
        }
        return func_mmap(addr, len, prot, flags, fd, offset);
    }

    __attribute__((visibility("default")))
    int munmap(void* addr, size_t len){
        if(IS_INITIALIZED){
            _DEBUG(
                "munmap 0x%04x %s",
                reinterpret_cast<std::uintptr_t>(addr),
                len
            );
            // TODO - handle when pointer + len are inside the data
            if(DO_HANDLE_FB && addr == blightBuffer->data){
                // Maybe actually close it?
                return 0;
            }
        }
        static const auto func_munmap = (int(*)(void*, size_t))dlsym(RTLD_NEXT, "munmap");
        return func_munmap(addr, len);
    }

    __attribute__((visibility("default")))
    ssize_t _write(int fd, const void* buf, size_t n){
        if(fd < 3){
            // No need to handle stdout/stderr writes
            return func_write(fd, buf, n);
        }
        if(IS_INITIALIZED){
            _DEBUG("write %d %zu", fd, n);
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
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
            // No need to handle stdout/stderr writes
            return func_writev(fd, iov, iovcnt);
        }
        if(IS_INITIALIZED){
            _DEBUG("writev %d %d", fd, iovcnt);
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
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
            // No need to handle stdout/stderr writes
            return func_writev64(fd, iov, iovcnt);
        }
        if(IS_INITIALIZED){
            _DEBUG("writv64 %d %d", fd, iovcnt);
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
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
            // No need to handle stdout/stderr writes
            return func_pwrite(fd, buf, n, offset);
        }
        if(IS_INITIALIZED){
            _DEBUG("pwrite %d %zu %d", fd, n, offset);
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
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
            // No need to handle stdout/stderr writes
            return func_pwrite64(fd, buf, n, offset);
        }
        if(IS_INITIALIZED){
            _DEBUG("pwrite64 %d %zu %d", fd, n, offset);
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
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
            // No need to handle stdout/stderr writes
            return func_pwritev(fd, iov, iovcnt, offset);
        }
        if(IS_INITIALIZED){
            _DEBUG("pwritev %d %d %d", fd, iovcnt, offset);
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
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
            // No need to handle stdout/stderr writes
            return func_pwritev64(fd, iov, iovcnt, offset);
        }
        if(IS_INITIALIZED){
            _DEBUG("pwritev64 %d %d %d", fd, iovcnt, offset);
            if(DO_HANDLE_FB && fd == blightBuffer->fd){
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
        func_open = (int(*)(const char*, int, ...))dlsym(RTLD_NEXT, "open");
        func_ioctl = (int(*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
        func_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
        func_msgget = (int(*)(key_t, int))dlsym(RTLD_NEXT, "msgget");
        func_mmap = (void*(*)(void*, size_t, int, int, int, __off_t))dlsym(RTLD_NEXT, "mmap");
    }

    __attribute__((visibility("default")))
    void _ZN6QImageC1EiiNS_6FormatE(void* data, int width, int height, int format) {
        static bool FIRST_ALLOC = true;
        static const auto qImageCtor = (
            void(*)(void*, int, int, int)
        )dlsym(RTLD_NEXT, "_ZN6QImageC1EiiNS_6FormatE");
        static const auto qImageCtorWithBuffer = (
            void(*)(void*, uint8_t*, int32_t, int32_t, int32_t, int, void(*)(void*), void*)
        )dlsym(RTLD_NEXT, "_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_");
        if(width == blightBuffer->width && height == blightBuffer->height && FIRST_ALLOC) {
            _INFO("Replacing image with buffer");
            FIRST_ALLOC = false;
            qImageCtorWithBuffer(
                data,
                (uint8_t *)blightBuffer->data,
                blightBuffer->width,
                blightBuffer->height,
                blightBuffer->stride,
                format,
                nullptr,
                nullptr
            );
            return;
        }
        qImageCtor(data, width, height, format);
    }

    static void _libhook_init() __attribute__((constructor));
    static void _libhook_init(){
        auto debugLevel = getenv("OXIDE_PRELOAD_DEBUG");
        if(debugLevel != nullptr){
            try{
                BLIGHT_DEBUG_LOGGING = std::stoi(debugLevel);
            }
            catch(std::invalid_argument&){}
            catch(std::out_of_range&){}
        }
        std::string path(realpath("/proc/self/exe", NULL));
        if(
            path != "/usr/bin/xochitl"
            && (
                   !path.starts_with("/opt")
                   || path.starts_with("/opt/sbin")
            )
            && (
                   !path.starts_with("/home")
                   || path.starts_with("/home/root/.entware/sbin")
            )
        ){
            // We ignore this executable
            BLIGHT_DEBUG_LOGGING = 0;
            FAILED_INIT = false;
            return;
        }
        if(getenv("OXIDE_PRELOAD_FORCE_RM1") != nullptr){
            FAKE_RM1 = true;
        }
        DO_HANDLE_FB = getenv("OXIDE_PRELOAD_EXPOSE_FB") == nullptr;
        _DEBUG("Handle framebuffer: %d", DO_HANDLE_FB);
        auto pid = getpid();
        _DEBUG("Connecting %d to blight", pid);
#ifdef __arm__
        bool connected = Blight::connect(true);
#else
        bool connected = Blight::connect(false);
#endif
        if(!connected){
            _CRIT("%s", "Could not connect to display server: %s", std::strerror(errno));
            std::quick_exit(EXIT_FAILURE);
        }
        blightConnection = Blight::connection();
        if(blightConnection == nullptr){
            _WARN("Failed to connect to display server: %s", std::strerror(errno));
            std::exit(errno);
        }
        blightConnection->onDisconnect([](int res){
            _WARN("Disconnected from display server: %s", std::strerror(res));
            // TODO - handle reconnect
            std::exit(res);
        });
        _DEBUG("Connected %d to blight on %d", pid, blightConnection->handle());
        setenv("RM2FB_ACTIVE", "1", true);
        setenv("OXIDE_PRELOAD", std::to_string(getpgrp()).c_str(), true);
        setenv("RM2FB_DISABLE", "1", true);
        setenv("RM2FB_SHIM", "1", true);
        FAILED_INIT = false;
        IS_INITIALIZED = true;
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
        _DEBUG("Starting main(%d, ...)", argc);
        return func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
    }
}
