#include "fb.h"
#include "input.h"
#include "libc.h"
#include "state.h"

#include <asm/ioctl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <libblight.h>
#include <libblight/clock.h>
#include <libblight/connection.h>
#include <libblight/debug.h>
#include <libblight/socket.h>
#include <libblight/system.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <mutex>
#include <mxcfb.h>
#include <stdio.h>
#include <string>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <systemd/sd-journal.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
    void __realpath(const char* pathname, std::string& path)
    {
        auto absolutepath = realpath(pathname, NULL);
        path = absolutepath;
        free(absolutepath);
    }

    int __open(const char* pathname, int flags)
    {
        if (!Client::INITIALIZED) {
            return -2;
        }
        std::string actualpath(pathname);
        if (std::filesystem::exists(actualpath)) {
            __realpath(pathname, actualpath);
        }
        int res = -2;
        if (Client::FAKE_RM1 && actualpath == "/sys/devices/soc0/machine") {
            int fd = memfd_create("machine", MFD_ALLOW_SEALING);
            std::string data("reMarkable 1.0");
            // Don't include trailing null
            Libc::write(fd, data.data(), data.size());
            fcntl(
                fd,
                F_ADD_SEALS,
                F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE |
                    F_SEAL_FUTURE_WRITE
            );
            lseek(fd, 0, SEEK_SET);
            return fd;
        } else if (actualpath == "/tmp/epframebuffer.lock") {
            if (FB::epframebufferLockFd > 0) {
                return FB::epframebufferLockFd;
            }
            res = FB::epframebufferLockFd = Libc::open(
                ("/tmp/epframebuffer." + std::to_string(getpid()) + ".lock")
                    .c_str(),
                flags
            );
        } else if (actualpath == "/tmp/epd.lock") {
            if (FB::epdLockFd > 0) {
                return FB::epdLockFd;
            }
            res = FB::epdLockFd = Libc::open(
                ("/tmp/epd." + std::to_string(getpid()) + ".lock").c_str(),
                flags
            );
        } else if (
            actualpath == "/dev/fb0" || actualpath == "/dev/shm/swtfb.01"
        ) {
            if (!Client::HANDLE_FB) {
                if (FB::frameBuffer < 0) {
                    FB::frameBuffer = Blight::frameBuffer();
                }
                return FB::frameBuffer;
            }
            if (FB::buffer->format != Blight::Format::Format_Invalid) {
                return FB::buffer->fd;
            }
            auto surfaceIds = FB::connection->surfaces();
            auto deviceFormat = FB::deviceFormat();
            if (!surfaceIds.empty()) {
                for (auto& identifier : surfaceIds) {
                    auto maybe = FB::connection->getBuffer(identifier);
                    if (!maybe.has_value()) {
                        continue;
                    }
                    auto buffer = maybe.value();
                    if (buffer->data == nullptr) {
                        continue;
                    }
                    if (buffer->x != 0 || buffer->y != 0 ||
                        buffer->width != FB::deviceXres() ||
                        buffer->height != FB::deviceYres() ||
                        buffer->stride != FB::deviceStride() ||
                        buffer->format != deviceFormat) {
                        continue;
                    }
                    _INFO("Reusing existing surface: %s", identifier);
                    FB::buffer = buffer;
                    break;
                }
            }
            if (FB::buffer->format == Blight::Format::Format_Invalid) {
                /// Emulate rM1 screen
                auto maybe = Blight::createBuffer(
                    0,
                    0,
                    FB::deviceXres(),
                    FB::deviceYres(),
                    FB::deviceStride(),
                    deviceFormat
                );
                if (!maybe.has_value()) {
                    return -1;
                }
                FB::buffer = maybe.value();
                // We don't ever plan on resizing, and we shouldn't let anything
                // try
                int flags = fcntl(FB::buffer->fd, F_GET_SEALS);
                fcntl(
                    FB::buffer->fd,
                    F_ADD_SEALS,
                    flags | F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW
                );
                res = FB::buffer->fd;
                if (res < 0) {
                    _CRIT("Failed to create buffer: %s", std::strerror(errno));
                    std::exit(errno);
                }
                // Initialize the buffer with white
                memset(
                    (std::uint16_t*)FB::buffer->data,
                    std::uint16_t(0xFFFFFFFF),
                    2808 * 1872
                );
                _INFO(
                    "Created buffer %s on fd %d",
                    FB::buffer->uuid.c_str(),
                    FB::buffer->fd
                );
                return res;
            }
        } else if (actualpath.starts_with("/dev/input/event")) {
            _INFO("Opening event device: %s", actualpath.c_str());
            if (FB::connection->input_handle() > 0) {
                Input::mutex.lock();
                if (Input::fds.empty() &&
                    getenv("OXIDE_PRELOAD_DISABLE_INPUT") == nullptr) {
                    new std::thread(Input::read);
                }
                std::string path(basename(actualpath.c_str()));
                try {
                    unsigned int device = stol(path.substr(5));
                    if (Input::fds.contains(device)) {
                        res = Input::fds[device][1];
                    } else {
                        int fds[2];
                        // TODO - what if they open it a second time with
                        // different flags?
                        int socketFlags = SOCK_STREAM;
                        if (flags & O_NONBLOCK || flags & O_NDELAY) {
                            socketFlags |= SOCK_NONBLOCK;
                        }
                        res = ::socketpair(AF_UNIX, socketFlags, 0, fds);
                        if (res < 0) {
                            _WARN(
                                "Failed to open event socket stream %s",
                                std::strerror(errno)
                            );
                        } else {
                            // Force writing side to be non blocking
                            int fd = Input::fds[device][0] = fds[0];
                            int flags = fcntl(fd, F_GETFD, NULL);
                            fcntl(fd, F_SETFD, flags | O_NONBLOCK);
                            res = Input::fds[device][1] = fds[1];
                            Input::deviceMap[res] =
                                Libc::open(actualpath.c_str(), O_RDWR, 0);
                            _DEBUG(
                                "Input::deviceMap[%d] = %d;",
                                res,
                                Input::deviceMap[res]
                            )
                        }
                    }
                } catch (std::invalid_argument&) {
                    _WARN(
                        "Resolved event device name invalid: %s", path.c_str()
                    );
                    res = -1;
                } catch (std::out_of_range&) {
                    _WARN(
                        "Resolved event device number out of range: %s",
                        path.c_str()
                    );
                    res = -1;
                }
                Input::mutex.unlock();
            } else {
                _WARN("Could not open connection input stream", "");
                res = -1;
            }
        }
        if (res == -1) {
            errno = EIO;
        }
        return res;
    }
} // namespace
#define alias(name)                                                            \
    __asm__(".globl  " #name "\n.type   " #name ", %function\n" #name          \
            "  = _" #name "\n")

#define symver(name) __asm__(".symver " #name " , " #name "@GLIBC_2.4")
#define _symver(name) __asm__(".symver _" #name ", " #name "@GLIBC_2.4")

extern "C"
{
    __attribute__((visibility("default"))) int msgget(key_t key, int msgflg)
    {
        if (!Client::INITIALIZED || !Client::HANDLE_FB) {
            return Libc::msgget(key, msgflg);
        }
        // Catch rm2fb ipc
        if (key == 0x2257c) {
            // inject our own ipc here
            if (FB::msgq == -1) {
                FB::msgq = Libc::msgget(0x2257d, msgflg);
            }
            return FB::msgq;
        }
        return Libc::msgget(key, msgflg);
    }
    symver(msgget);

    __attribute__((visibility("default"))) int
    msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg)
    {
        if (!Client::INITIALIZED || !Client::HANDLE_FB) {
            return Libc::msgsnd(msqid, msgp, msgsz, msgflg);
        }
        if (msqid == FB::msgq) {
            if (!FB::buffer->surface) {
                Blight::addSurface(FB::buffer);
            }
            if (!FB::buffer->surface) {
                _CRIT("Failed to create surface: %s", std::strerror(errno));
                std::exit(errno);
                return -1;
            }
            _DEBUG("%s", "rm2fb ipc repaint");
            auto buf = (swtfb::swtfb_update*)msgp;
            auto region = buf->mdata.update.update_region;
            FB::connection->repaint(
                FB::buffer, region.left, region.top, region.width, region.height
            );
            return 0;
        }
        return Libc::msgsnd(msqid, msgp, msgsz, msgflg);
    }
    symver(msgsnd);

    __attribute__((visibility("default"))) int
    _open64(const char* pathname, int flags, ...)
    {
        if (!Client::INITIALIZED) {
            va_list args;
            va_start(args, flags);
            int fd = Libc::open64(pathname, flags, args);
            va_end(args);
            return fd;
        }
        _DEBUG("open64 %s", pathname);
        int fd = __open(pathname, flags);
        if (fd == -2) {
            va_list args;
            va_start(args, flags);
            fd = Libc::open64(pathname, flags, args);
            va_end(args);
        }
        _DEBUG("opened %s with fd %d", pathname, fd);
        return fd;
    }
    _symver(open64);
    alias(open64);

    __attribute__((visibility("default"))) int
    _openat(int dirfd, const char* pathname, int flags, ...)
    {
        if (!Client::INITIALIZED) {
            va_list args;
            va_start(args, flags);
            int fd = Libc::openat(dirfd, pathname, flags, args);
            va_end(args);
            return fd;
        }
        _DEBUG("openat %s", pathname);
        int fd = __open(pathname, flags);
        if (fd == -2) {
            DIR* save = opendir(".");
            fchdir(dirfd);
            char path[PATH_MAX + 1];
            getcwd(path, PATH_MAX);
            fchdir(::dirfd(save));
            closedir(save);
            std::string fullpath(path);
            fullpath += "/";
            fullpath += pathname;
            fd = __open(fullpath.c_str(), flags);
        }
        if (fd == -2) {
            va_list args;
            va_start(args, flags);
            fd = Libc::openat(dirfd, pathname, flags, args);
            va_end(args);
        }
        _DEBUG("opened %s with fd %d", pathname, fd);
        return fd;
    }
    _symver(openat);
    alias(openat);

    __attribute__((visibility("default"))) int
    _openat64(int dirfd, const char* pathname, int flags, ...)
    {
        if (!Client::INITIALIZED) {
            va_list args;
            va_start(args, flags);
            int fd = Libc::openat(dirfd, pathname, flags, args);
            va_end(args);
            return fd;
        }
        _DEBUG("openat64 %s", pathname);
        int fd = __open(pathname, flags);
        if (fd == -2) {
            DIR* save = opendir(".");
            fchdir(dirfd);
            char path[PATH_MAX + 1];
            getcwd(path, PATH_MAX);
            fchdir(::dirfd(save));
            closedir(save);
            std::string fullpath(path);
            fullpath += "/";
            fullpath += pathname;
            fd = __open(fullpath.c_str(), flags);
        }
        if (fd == -2) {
            va_list args;
            va_start(args, flags);
            fd = Libc::openat64(dirfd, pathname, flags, args);
            va_end(args);
        }
        _DEBUG("opened %s with fd %d", pathname, fd);
        return fd;
    }
    _symver(openat64);
    alias(openat64);

    __attribute__((visibility("default"))) int
    _open(const char* pathname, int flags, ...)
    {
        if (!Client::INITIALIZED) {
            va_list args;
            va_start(args, flags);
            int fd = Libc::open(pathname, flags, args);
            ;
            va_end(args);
            return fd;
        }
        _DEBUG("open %s", pathname);
        int fd = __open(pathname, flags);
        if (fd == -2) {
            va_list args;
            va_start(args, flags);
            fd = Libc::open(pathname, flags, args);
            va_end(args);
        }
        _DEBUG("opened %s with fd %d", pathname, fd);
        return fd;
    }
    _symver(open);
    alias(open);

    __attribute__((visibility("default"))) int close(int fd)
    {
        if (Client::INITIALIZED) {
            _DEBUG("close %d", fd);
            if (FB::is_fb(fd)) {
                // Maybe actually close it?
                return 0;
            }
            if (Input::deviceMap.contains(fd)) {
                // Maybe actually close it?
                return 0;
            }
            if (fd == FB::epframebufferLockFd) {
                int res = Libc::close(FB::epframebufferLockFd);
                if (res == 0) {
                    FB::epframebufferLockFd = -1;
                    unlink(("/tmp/epframebuffer." + std::to_string(getpid()) +
                            ".lock")
                               .c_str());
                }
                return res;
            }
            if (fd == FB::epdLockFd) {
                int res = Libc::close(FB::epdLockFd);
                if (res == 0) {
                    FB::epdLockFd = -1;
                    unlink(("/tmp/epd." + std::to_string(getpid()) + ".lock")
                               .c_str());
                }
                return res;
            }
        }
        return Libc::close(fd);
    }
    symver(close);

    __attribute__((visibility("default"))) int
    ioctl(int fd, unsigned long request, ...)
    {
        va_list args;
        va_start(args, request);
        char* ptr = va_arg(args, char*);
        if (Client::INITIALIZED) {
            if (FB::is_fb(fd)) {
                int res = FB::ioctl(request, ptr);
                va_end(args);
                return res;
            }
            if (!Client::HANDLE_FB && fd == FB::frameBuffer) {
                int res = FB::exclusive_ioctl(request, ptr);
                va_end(args);
                return res;
            }
            if (Input::deviceMap.contains(fd)) {
                int res = Input::ioctlv(Input::deviceMap[fd], request, ptr);
                va_end(args);
                return res;
            }
        }
        _DEBUG("unhandled ioctl %d %lu", fd, request);
        int res = Libc::ioctl(fd, request, ptr);
        va_end(args);
        return res;
    }
    symver(ioctl);
    __asm__(".globl  ioctl\n"
            ".type   ioctl, %function\n"
            "ioctl   = __ioctl_time64\n");

    __attribute__((visibility("default"))) ssize_t
    _write(int fd, const void* buf, size_t n)
    {
        if (fd < 3) {
            // No need to handle stdout/stderr writes
            return Libc::write(fd, buf, n);
        }
        if (Client::INITIALIZED) {
            _DEBUG("write %d %zu", fd, n);
            if (FB::is_fb(fd)) {
                auto res = Libc::write(fd, buf, n);
                return res;
            }
        }
        return Libc::write(fd, buf, n);
    }
    _symver(write);
    alias(write);

    __attribute__((visibility("default"))) ssize_t
    _writev(int fd, const iovec* iov, int iovcnt)
    {
        if (fd < 3) {
            // No need to handle stdout/stderr writes
            return Libc::writev(fd, iov, iovcnt);
        }
        if (Client::INITIALIZED) {
            _DEBUG("writev %d %d", fd, iovcnt);
            if (FB::is_fb(fd)) {
                auto res = Libc::writev(fd, iov, iovcnt);
                return res;
            }
        }
        return Libc::writev(fd, iov, iovcnt);
    }
    _symver(writev);
    alias(writev);

    __attribute__((visibility("default"))) ssize_t
    _writev64(int fd, const iovec* iov, int iovcnt)
    {
        if (fd < 3) {
            // No need to handle stdout/stderr writes
            return Libc::writev64(fd, iov, iovcnt);
        }
        if (Client::INITIALIZED) {
            _DEBUG("writv64 %d %d", fd, iovcnt);
            if (FB::is_fb(fd)) {
                auto res = Libc::writev64(fd, iov, iovcnt);
                return res;
            }
        }
        return Libc::writev64(fd, iov, iovcnt);
    }
    _symver(writev64);
    alias(writev64);

    __attribute__((visibility("default"))) ssize_t
    _pwrite(int fd, const void* buf, size_t n, int offset)
    {
        if (fd < 3) {
            // No need to handle stdout/stderr writes
            return Libc::pwrite(fd, buf, n, offset);
        }
        if (Client::INITIALIZED) {
            _DEBUG("pwrite %d %zu %d", fd, n, offset);
            if (FB::is_fb(fd)) {
                auto res = Libc::pwrite(fd, buf, n, offset);
                return res;
            }
        }
        return Libc::pwrite(fd, buf, n, offset);
    }
    _symver(pwrite);
    alias(pwrite);

    __attribute__((visibility("default"))) ssize_t
    _pwrite64(int fd, const void* buf, size_t n, int offset)
    {
        if (fd < 3) {
            // No need to handle stdout/stderr writes
            return Libc::pwrite64(fd, buf, n, offset);
        }
        if (Client::INITIALIZED) {
            _DEBUG("pwrite64 %d %zu %d", fd, n, offset);
            if (FB::is_fb(fd)) {
                auto res = Libc::pwrite64(fd, buf, n, offset);
                return res;
            }
        }
        return Libc::pwrite64(fd, buf, n, offset);
    }
    _symver(pwrite64);
    alias(pwrite64);

    __attribute__((visibility("default"))) ssize_t
    _pwritev(int fd, const iovec* iov, int iovcnt, int offset)
    {
        if (fd < 3) {
            // No need to handle stdout/stderr writes
            return Libc::pwritev(fd, iov, iovcnt, offset);
        }
        if (Client::INITIALIZED) {
            _DEBUG("pwritev %d %d %d", fd, iovcnt, offset);
            if (FB::is_fb(fd)) {
                auto res = Libc::pwritev(fd, iov, iovcnt, offset);
                return res;
            }
        }
        return Libc::pwritev(fd, iov, iovcnt, offset);
    }
    _symver(pwritev);
    alias(pwritev);

    __attribute__((visibility("default"))) ssize_t
    _pwritev64(int fd, const iovec* iov, int iovcnt, int offset)
    {
        if (fd < 3) {
            // No need to handle stdout/stderr writes
            return Libc::pwritev64(fd, iov, iovcnt, offset);
        }
        if (Client::INITIALIZED) {
            _DEBUG("pwritev64 %d %d %d", fd, iovcnt, offset);
            if (FB::is_fb(fd)) {
                auto res = Libc::pwritev64(fd, iov, iovcnt, offset);
                return res;
            }
        }
        return Libc::pwritev64(fd, iov, iovcnt, offset);
    }
    _symver(pwritev64);
    alias(pwritev64);

    __attribute__((visibility("default"))) int
    setenv(const char* name, const char* value, int overwrite)
    {
        if (getenv("OXIDE_PRELOAD_FORCE_QT") != nullptr &&
            (strcmp(name, "QMLSCENE_DEVICE") == 0 ||
             strcmp(name, "QT_QUICK_BACKEND") == 0 ||
             strcmp(name, "QT_QPA_PLATFORM") == 0 ||
             strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0 ||
             strcmp(name, "QT_QPA_GENERIC_PLUGINS") == 0 ||
             strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0 ||
             strcmp(name, "QT_QPA_EVDEV_MOUSE_PARAMETERS") == 0 ||
             strcmp(name, "QT_QPA_EVDEV_KEYBOARD_PARAMETERS") == 0 ||
             strcmp(name, "QT_PLUGIN_PATH") == 0)) {
            _DEBUG("IGNORED setenv %s=%s", name, value);
            return 0;
        }
        _DEBUG("setenv %s=%s", name, value);
        return Libc::setenv(name, value, overwrite);
    }
    symver(setenv);

    __attribute__((visibility("default"))) int flock(int fd, int op)
    {
        if (!Client::INITIALIZED) {
            return Libc::flock(fd, op);
        }
        _DEBUG("flock %d %d", fd, op);
        if (fd == FB::epframebufferLockFd) {
            switch (op) {
                case LOCK_SH:
                case LOCK_EX:
                case LOCK_UN:
                    return 0;
                default:
                    break;
            }
        }
        return Libc::flock(fd, op);
    }
    symver(flock);

    void __attribute__((constructor)) init(void);
    void init(void)
    {
        auto debugLevel = getenv("OXIDE_PRELOAD_DEBUG");
        if (debugLevel != nullptr) {
            try {
                set_blight_debug_level(std::stoi(debugLevel));
            } catch (std::invalid_argument&) {
            } catch (std::out_of_range&) {
                _WARN("OXIDE_PRELOAD_DEBUG invalid value");
            }
        }
        auto batch_size = getenv("OXIDE_INPUT_BATCH_SIZE");
        if (batch_size != nullptr) {
            try {
                Input::BATCH_SIZE = std::stoi(batch_size);
            } catch (std::invalid_argument&) {
            } catch (std::out_of_range&) {
            }
        }
        std::string path;
        __realpath("/proc/self/exe", path);
        if (path != "/usr/bin/xochitl" &&
            (!path.starts_with("/opt") || path.starts_with("/opt/sbin")) &&
            (!path.starts_with("/home") ||
             path.starts_with("/home/root/.entware/sbin"))) {
            // We ignore this executable
            set_blight_debug_level(0);
            Client::FAILED_INIT = false;
            return;
        }
        if (getenv("OXIDE_PRELOAD_FORCE_RM1") != nullptr) {
            Client::FAKE_RM1 = true;
        }
        Client::HANDLE_FB = getenv("OXIDE_PRELOAD_EXPOSE_FB") == nullptr;
        _DEBUG("Handle framebuffer: %d", Client::HANDLE_FB);
        auto pid = getpid();
        _DEBUG("Connecting %d to blight", pid);
#ifdef __arm__
        bool connected = Blight::connect(true);
#else
        bool connected = Blight::connect(false);
#endif
        if (!connected) {
            _CRIT(
                "%s",
                "Could not connect to display server: %s",
                std::strerror(errno)
            );
            std::quick_exit(EXIT_FAILURE);
        }
        FB::connection = Blight::connection();
        if (FB::connection == nullptr) {
            _WARN(
                "Failed to connect to display server: %s", std::strerror(errno)
            );
            std::exit(errno);
        }
        FB::connection->onDisconnect([](int res) {
            _WARN("Disconnected from display server: %s", std::strerror(res));
            // TODO - handle reconnect
            std::exit(res);
        });
        _DEBUG("Connected %d to blight on %d", pid, FB::connection->handle());
        Libc::setenv("OXIDE_PRELOAD", std::to_string(getpgrp()).c_str(), true);
        FB::init();
        if (Client::deviceType == Client::DeviceType::RM2) {
            Libc::setenv("RM2FB_ACTIVE", "1", true);
            Libc::setenv("RM2FB_SHIM", "1", true);
            if (path != "/usr/bin/xochitl" &&
                getenv("OXIDE_PRELOAD_ALLOW_RM2FB") == nullptr) {
                Libc::setenv("RM2FB_DISABLE", "1", true);
            } else {
                unsetenv("RM2FB_DISABLE");
            }
        }
        if (getenv("OXIDE_PRELOAD_FORCE_QT") != nullptr) {
            Libc::setenv("QMLSCENE_DEVICE", "software", 1);
            Libc::setenv("QT_QUICK_BACKEND", "software", 1);
            Libc::setenv("QT_QPA_PLATFORM", "oxide:enable_fonts", 1);
            Libc::setenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "", 1);
            Libc::setenv("QT_QPA_EVDEV_MOUSE_PARAMETERS", "", 1);
            Libc::setenv("QT_QPA_EVDEV_KEYBOARD_PARAMETERS", "", 1);
            Libc::setenv("QT_PLUGIN_PATH", "/opt/usr/lib/plugins", 1);
        }
        if (Client::HANDLE_FB) {
            FB::buffer = Blight::buf_t::new_ptr();
        } else {
            Blight::setFlags(
                std::string("connection/") + std::to_string(getpid()),
                std::vector<std::string>{ "system" }
            );
            Blight::enterExclusiveMode();
        }
        Client::FAILED_INIT = false;
        Client::INITIALIZED = true;
        FB::connection->focused();
        _DEBUG("blight_client initialized")
    }

    __attribute__((visibility("default"))) int __libc_start_main(
        int (*_main)(int, char**, char**),
        int argc,
        char** argv,
        int (*init)(int, char**, char**),
        void (*fini)(void),
        void (*rtld_fini)(void),
        void* stack_end
    )
    {
        if (Client::FAILED_INIT) {
            return EXIT_FAILURE;
        }
        auto func_main =
            (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");
        _DEBUG("Starting main(%d, ...)", argc);
        auto res =
            func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
        _DEBUG("Main exit code: %d", res);
        if (!Client::HANDLE_FB) {
            Blight::exitExclusiveMode();
        }
        return res;
    }
}
