#include "libblight.h"
#include "dbus.h"
#include "meta.h"

#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace Blight{
    static DBus* dbus = nullptr;

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
        auto reply = dbus->call_method(BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "open");
        if(reply->isError()){
            std::cerr
                << "[Blight::open()::call_method(...)] Error: "
                << reply->error_message()
                << std::endl;
            return reply->return_value;
        }
        auto fd = reply->read_value<int>("h");
        if(!fd.has_value()){
            std::cerr
                << "[Blight::open()::read_value(...)] "
                << reply->error_message()
                << std::endl;
            return reply->return_value;
        }
        int dfd = fcntl(fd.value(), F_DUPFD_CLOEXEC, 3);
        if(dfd < 0){
            std::cerr
                << "[Blight::open()::dup("
                << std::to_string(fd.value())
                << ")] "
                << std::strerror(errno)
                << std::endl;
            errno = -dfd;
        }
        return dfd;
    }

    int open_input(){
        if(!exists()){
            errno = EAGAIN;
            return -EAGAIN;
        }
        auto reply = dbus->call_method(BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "openInput");
        if(reply->isError()){
            std::cerr
                << "[Blight::open()::call_method(...)] Error: "
                << reply->error_message()
                << std::endl;
            return reply->return_value;
        }
        auto fd = reply->read_value<int>("h");
        if(!fd.has_value()){
            std::cerr
                << "[Blight::open()::read_value(...)] "
                << reply->error_message()
                << std::endl;
            return reply->return_value;
        }
        int dfd = fcntl(fd.value(), F_DUPFD_CLOEXEC, 3);
        if(dfd < 0){
            std::cerr
                << "[Blight::open()::dup("
                << std::to_string(fd.value())
                << ")] "
                << std::strerror(errno)
                << std::endl;
            errno = -dfd;
        }
        return dfd;
    }

    std::optional<shared_buf_t> createBuffer(int x, int y, int width, int height, int stride, Format format){
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
                << std::strerror(errno)
                << std::endl;
            delete buf;
            return {};
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
            delete buf;
            errno = e;
            return {};
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
            delete buf;
            errno = e;
            return {};
        }
        buf->data = reinterpret_cast<data_t>(data);
        return shared_buf_t(buf);
    }

    std::optional<std::string> addSurface(
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
            return {};
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "addSurface",
            "hiiiiii",
            fd,
            x,
            y,
            width,
            height,
            stride,
            format
        );
        if(reply->isError()){
            std::cerr
                << "[Blight::addSurface()::call_method(...)] Error: "
                << reply->error_message()
                << std::endl;
            return {};
        }
        auto identifier = reply->read_value<char*>("s");
        if(!identifier.has_value()){
            std::cerr
                << "[Blight::addSurface()::read_value(...)] "
                << reply->error_message()
                << std::endl;
            return {};
        }
        return std::string(identifier.value());
    }

    int repaint(std::string identifier){
        if(!exists()){
            errno = EAGAIN;
            return -errno;
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "repaint",
            "s",
            identifier.c_str()
        );
        if(reply->isError()){
            std::cerr
                << "[Blight::repaint()::call_method(...)] Error: "
                << reply->error_message()
                << std::endl;
        }
        return reply->return_value;
    }

    int getSurface(std::string identifier){
        if(!exists()){
            errno = EAGAIN;
            return -1;
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "getSurface",
            "s",
            identifier.c_str()
        );
        if(reply->isError()){
            std::cerr
                << "[Blight::getSurface()::call_method(...)] Error: "
                << reply->error_message()
                << std::endl;
            return reply->return_value;
        }
        auto fd = reply->read_value<int>("h");
        if(!fd.has_value()){
            std::cerr
                << "[Blight::getBuffer()::read_value(...)] "
                << reply->error_message()
                << std::endl;
            return reply->return_value;

        }
        int dfd = dup(fd.value());
        if(dfd == -1){
            std::cerr
                << "[Blight::getBuffer()::dup("
                << fd.value()
                << ")] "
                << std::strerror(errno)
                << std::endl;
            return -errno;
        }
        return dfd;
    }
}
