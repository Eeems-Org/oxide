#include "libblight.h"
#include "dbus.h"
#include "meta.h"

#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static Blight::DBus* dbus = nullptr;

namespace Blight{
    bool connect(bool use_system){
        if(dbus != nullptr){
            return true;
        }
        try{
            dbus = new DBus(use_system);
            return true;
        }catch(const Blight::DBusException& e){
            std::cerr << e.what() << std::endl;
            return false;
        }
    }
    bool exists(){ return connect() && dbus->has_service(BLIGHT_SERVICE); }

    int open(){
        if(!exists()){
            errno = EAGAIN;
            return -EAGAIN;
        }
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* msg = nullptr;
        int res = sd_bus_call_method(
            dbus->bus(),
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "open",
            &err,
            &msg,
            ""
        );
        if(res < 0){
            std::cerr
                << "[Blight::open()::sd_bus_call_method(...)] Error: "
                << (err.message == NULL ? std::strerror(-res) : err.message)
                << std::endl;
            sd_bus_error_free(&err);
            sd_bus_message_unref(msg);
            errno = -res;
            return res;
        }
        sd_bus_error_free(&err);
        int fd;
        res = sd_bus_message_read(msg, "h", &fd);
        if(res < 0){
            std::cerr
                << "[Blight::open()::sd_bus_message_read(...)] "
                << std::strerror(-res)
                << std::endl;
            sd_bus_message_unref(msg);
            errno = -res;
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if(dfd < 0){
            std::cerr
                << "[Blight::open()::dup("
                << std::to_string(fd)
                << ")] "
                << std::strerror(errno)
                << std::endl;
            errno = -dfd;
        }
        sd_bus_message_unref(msg);
        return dfd;
    }

    shared_buf_t createBuffer(int x, int y, int width, int height, int stride, Format format){
        auto buf = new buf_t{
            .fd = -1,
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .stride = stride,
            .format = format,
            .data = nullptr,
            .uuid = buf_t::new_uuid(),
            .surface = ""
        };
        buf->fd = memfd_create(buf->uuid.c_str(), MFD_ALLOW_SEALING);
        if(buf->fd == -1){
            std::cerr
                << "[Blight::createBuffer()::memfd_create(...)] "
                << std::strerror(-buf->fd)
                << std::endl;
            return shared_buf_t(buf);
        }
        if(ftruncate(buf->fd, buf->size())){
            std::cerr
                << "[Blight::createBuffer()::ftruncate(...)] "
                << std::strerror(errno)
                << std::endl
                << "    Requested size: "
                << std::to_string(buf->size())
                << std::endl;
            int e = errno;
            buf->close();
            errno = e;
            return shared_buf_t(buf);
        }
        void* data = mmap(
            NULL,
            buf->size(),
            PROT_READ | PROT_WRITE,
            MAP_SHARED_VALIDATE,
            buf->fd,
            0
        );
        if(data == MAP_FAILED || data == nullptr){
            std::cerr
                << "[Blight::createBuffer()::mmap(...)] "
                << std::strerror(errno)
                << std::endl
                << "    Requested size: "
                << std::to_string(buf->size())
                << std::endl;
            int e = errno;
            buf->close();
            errno = e;
            return shared_buf_t(buf);
        }
        buf->data = reinterpret_cast<data_t>(data);
        return shared_buf_t(buf);
    }

    std::string addSurface(
        int fd,
        int x,
        int y,
        int width,
        int height,
        int stride,
        Format format
    ){
        if(!exists()){
            errno = EAGAIN;
            return "";
        }
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* msg = nullptr;
        int res = sd_bus_call_method(
            dbus->bus(),
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "addSurface",
            &err,
            &msg,
            "hiiiiii",
            fd,
            x,
            y,
            width,
            height,
            stride,
            format
        );
        if(res < 0){
            std::cerr
                << "[Blight::addSurface()::sd_bus_call_method(...)] Error: "
                << (err.message == NULL ? std::strerror(-res) : err.message)
                << std::endl;
            sd_bus_error_free(&err);
            sd_bus_message_unref(msg);
            errno = -res;
            return "";
        }
        char* identifier;
        res = sd_bus_message_read(msg, "s", &identifier);
        sd_bus_message_unref(msg);
        if(res < 0){
            std::cerr
                << "[Blight::addSurface()::sd_bus_message_read(...)] "
                << std::strerror(-res)
                << std::endl;
            errno = -res;
            return "";
        }
        return std::string(identifier);
    }

    int repaint(std::string identifier){
        if(!exists()){
            errno = EAGAIN;
            return -errno;
        }
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* msg = nullptr;
        int res = sd_bus_call_method(
            dbus->bus(),
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "repaint",
            &err,
            &msg,
            "s",
            identifier.c_str()
        );
        if(res < 0){
            std::cerr
                << "[Blight::repaint()::sd_bus_call_method(...)] Error: "
                << (err.message == NULL ? std::strerror(-res) : err.message)
                << std::endl;
            errno = -res;
        }
        sd_bus_error_free(&err);
        sd_bus_message_unref(msg);
        return res;
    }

    int getSurface(std::string identifier){
        if(!exists()){
            errno = EAGAIN;
            return -1;
        }
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* msg = nullptr;
        int res = sd_bus_call_method(
            dbus->bus(),
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "getSurface",
            &err,
            &msg,
            "s",
            identifier.c_str()
        );
        if(res < 0){
            std::cerr
                << "[Blight::getBuffer()::sd_bus_call_method(...)] Error: "
                << (err.message == NULL ? std::strerror(-res) : err.message)
                << std::endl;
            sd_bus_error_free(&err);
            sd_bus_message_unref(msg);
            errno = -res;
            return -res;
        }
        int fd;
        res = sd_bus_message_read(msg, "h", &fd);
        if(res < 0){
            std::cerr
                << "[Blight::getBuffer()::sd_bus_message_read(...)] "
                << std::strerror(-res)
                << std::endl;
            errno = -res;
            return -res;
        }
        int dfd = dup(fd);
        if(dfd == -1){
            std::cerr
                << "[Blight::getBuffer()::dup("
                << fd
                << ")] "
                << std::strerror(errno)
                << std::endl;
            return -errno;
        }
        return dfd;
    }
}
