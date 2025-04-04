#include "libblight.h"
#include "dbus.h"
#include "meta.h"
#include "types.h"
#include "debug.h"

#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace Blight{
    static DBus* dbus = nullptr;

    std::optional<clipboard_t> getClipboard(const std::string& name){
        if(!exists()){
            errno = EAGAIN;
            return {};
        }
        auto reply = dbus->get_property(
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            name,
            "ay"
        );
        if(reply->isError()){
            _WARN(
                "[Blight::%s()::get_property(...)] Error: %s",
                name.c_str(),
                reply->error_message().c_str()
            );
            return {};
        }
        const void* clipboard = nullptr;
        ::size_t size = 0;
        auto res = sd_bus_message_read_array(
            reply->message,
            'y',
            &clipboard,
            &size
        );
        if(res < 0){
            _WARN(
                "[Blight::%s()::sd_bus_message_read_array(...)] Error: %s",
                name.c_str(),
                reply->error_message().c_str()
            );
            return {};
        }
        auto data = new unsigned char[size];
        memcpy(data, clipboard, size);
        return clipboard_t(name, data, static_cast<size_t>(size));
    }

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

    Connection* connection(){
        static Connection* instance = nullptr;
        if(instance == nullptr){
            int fd = open();
            if(fd < 0){
                return nullptr;
            }
            instance = new Connection(fd);
        }
        return instance;
    }

    int open(){
        if(!exists()){
            errno = EAGAIN;
            return -EAGAIN;
        }
        auto reply = dbus->call_method(BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "open");
        if(reply->isError()){
            _WARN("[Blight::open()::call_method(...)] Error: %s", reply->error_message().c_str());
            return reply->return_value;
        }
        auto fd = reply->read_value<int>("h");
        if(!fd.has_value()){
            _WARN("[Blight::open()::read_value(\"h\")] Error: %s", reply->error_message().c_str());
            return reply->return_value;
        }
        int dfd = fcntl(fd.value(), F_DUPFD_CLOEXEC, 3);
        if(dfd < 0){
            _WARN("[Blight::open()::dup(%d)] Error: %s", fd.value(), reply->error_message().c_str());
            return -errno;
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
            _WARN("[Blight::open_input()::call_method(...)] Error: %s", reply->error_message().c_str());
            return reply->return_value;
        }
        auto fd = reply->read_value<int>("h");
        if(!fd.has_value()){
            _WARN("[Blight::open_input()::read_value(\"h\")] Error: %s", reply->error_message().c_str());
            return reply->return_value;
        }
        int dfd = fcntl(fd.value(), F_DUPFD_CLOEXEC, 3);
        if(dfd < 0){
            _WARN("[Blight::open_input()::dup(%d)] Error: %s", fd.value(), reply->error_message().c_str());
            return -errno;
        }
        return dfd;
    }

    std::optional<shared_buf_t> createBuffer(int x, int y, unsigned int width, unsigned int height, int stride, Format format){
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
            .surface = 0
        };
        buf->fd = memfd_create(buf->uuid.c_str(), MFD_ALLOW_SEALING);
        if(buf->fd == -1){
            _WARN("[Blight::createBuffer()::memfd_create(...)] Error: %s", std::strerror(errno));
            delete buf;
            return {};
        }
        if(ftruncate(buf->fd, buf->size())){
            _WARN(
                "[Blight::createBuffer()::ftruncate(%d)] Error: %s",
                buf->size(),
                std::strerror(errno)
            );
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
            _WARN(
                "[Blight::createBuffer()::mmap(%d)] Error: %s",
                buf->size(),
                std::strerror(errno)
            );
            int e = errno;
            buf->close();
            delete buf;
            errno = e;
            return {};
        }
        buf->data = reinterpret_cast<data_t>(data);
        return shared_buf_t(buf);
    }

    std::optional<surface_id_t> addSurface(
        int fd,
        int x,
        int y,
        unsigned int width,
        unsigned int height,
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
            _WARN("[Blight::addSurface()::call_method(...)] Error: %s", reply->error_message().c_str());
            return {};
        }
        auto identifier = reply->read_value<surface_id_t>("q");
        if(!identifier.has_value()){
            _WARN("[Blight::addSurface()::read_value(\"q\")] Error: %s", reply->error_message().c_str());
            return {};
        }
        return identifier.value();
    }

    int repaint(const std::string& identifier){
        if(!exists()){
            errno = EAGAIN;
            return -errno;
        }
        if(identifier.empty()){
            errno = EINVAL;
            return -errno;
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "repaint",
            "s",
            identifier
        );
        if(reply->isError()){
            _WARN(
                "[Blight::repaint(%s)::call_method(...)] Error: %s",
                identifier.c_str(),
                reply->error_message().c_str()
            );
        }
        return reply->return_value;
    }

    int getSurface(surface_id_t identifier){
        if(!exists()){
            errno = EAGAIN;
            return -1;
        }
        if(!identifier){
            errno = EINVAL;
            return -1;
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "getSurface",
            "q",
            identifier
        );
        if(reply->isError()){
            _WARN(
                "[Blight::getSurface(%d)::call_method(...)] Error: %s",
                identifier,
                reply->error_message().c_str()
            );
            return reply->return_value;
        }
        auto fd = reply->read_value<int>("h");
        if(!fd.has_value()){
            _WARN(
                "[Blight::addSurface(%d)::read_value(\"h\")] Error: %s",
                identifier,
                reply->error_message().c_str()
            );
            return reply->return_value;
        }
        int dfd = dup(fd.value());
        if(dfd == -1){
            _WARN(
                "[Blight::addSurface(%d)::dup(%d)] Error: %s",
                identifier,
                fd.value(),
                reply->error_message().c_str()
            );
            return -errno;
        }
        return dfd;
    }

    std::optional<clipboard_t> clipboard(){ return getClipboard("clipboard"); }

    std::optional<clipboard_t> selection(){ return getClipboard("selection"); }

    std::optional<clipboard_t> secondary(){ return getClipboard("secondary"); }


    bool setClipboard(clipboard_t& clipboard){
        if(
           clipboard.name != "clipboard"
           && clipboard.name != "selection"
           && clipboard.name != "secondary"
        ){
            _WARN("[Blight::setClipboard(\"%s\")] Error: Invalid clipboard name", clipboard.name.c_str());
            errno = EINVAL;
            return false;
        }
        char buf[sizeof(clipboard.size) + clipboard.size];
        memcpy(buf, &clipboard.size, sizeof(clipboard.size));
        memcpy(&buf[sizeof(clipboard.size)], clipboard.data.get(), sizeof(clipboard.size));
        sd_bus_message* message;
        auto res = sd_bus_message_new_method_call(
            dbus->bus(),
            &message,
            BLIGHT_SERVICE,
            "/",
            "org.freedesktop.DBus.Properties",
            "Set"
        );
        if(res < 0){
            _WARN(
                "[Blight::setClipboard(\"%s\")::sd_bus_message_new_method_call()] Error: %s",
                clipboard.name.c_str(),
                std::strerror(-res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            // Attempt to reset to the current value
            updateClipboard(clipboard);
            return false;
        }
        res = sd_bus_message_append(
            message,
            "ss",
            BLIGHT_INTERFACE,
            // sd_bus_message_append seems to destroy the string,
            // so a copy is needed
            std::string(clipboard.name).c_str()
        );
        if(res < 0){
            _WARN(
                "[Blight::setClipboard(\"%s\")::sd_bus_message_append()] Error: %s",
                clipboard.name.c_str(),
                std::strerror(-res)
            );
            sd_bus_message_unref(message);
            // Attempt to reset to the current value
            updateClipboard(clipboard);
            return false;
        }
        res = sd_bus_message_open_container(message, 'v', "ay");
        if(res < 0){
            _WARN(
                "[Blight::setClipboard(\"%s\")::sd_bus_message_open_container()] Error: %s",
                clipboard.name.c_str(),
                std::strerror(-res)
            );
            sd_bus_message_unref(message);
            // Attempt to reset to the current value
            updateClipboard(clipboard);
            return false;
        }
        res = sd_bus_message_append_array(
            message,
            'y',
            clipboard.data.get(),
            clipboard.size
        );
        if(res < 0){
            _WARN(
                "[Blight::setClipboard(\"%s\")::sd_bus_message_append_array()] Error: %s",
                clipboard.name.c_str(),
                std::strerror(-res)
            );
            sd_bus_message_unref(message);
            // Attempt to reset to the current value
            updateClipboard(clipboard);
            return false;
        }
        res = sd_bus_message_close_container(message);
        if(res < 0){
            _WARN(
                "[Blight::setClipboard(\"%s\")::sd_bus_message_close_container()] Error: %s",
                clipboard.name.c_str(),
                std::strerror(-res)
            );
            sd_bus_message_unref(message);
            // Attempt to reset to the current value
            updateClipboard(clipboard);
            return false;
        }
        sd_bus_error error = SD_BUS_ERROR_NULL;
        res = sd_bus_call(dbus->bus(), message, 0, &error, NULL);
        if(res < 0){
            _WARN(
                "[Blight::setClipboard(\"%s\")::sd_bus_call()] Error: %s",
                clipboard.name.c_str(),
                (error.message != NULL ? error.message : std::strerror(-res))
            );
            sd_bus_message_unref(message);
            sd_bus_error_free(&error);
            // Attempt to reset to the current value
            updateClipboard(clipboard);
            return false;
        }
        sd_bus_message_unref(message);
        sd_bus_error_free(&error);
        return true;
    }

    bool updateClipboard(clipboard_t& clipboard){
        if(
           clipboard.name != "clipboard"
           && clipboard.name != "selection"
           && clipboard.name != "secondary"
        ){
            _WARN(
                "[Blight::setClipboard(\"%s\")::sd_bus_call()] Error: Invalid clipboard name",
                clipboard.name.c_str()
            );
            errno = EINVAL;
            return false;
        }
        // Reload data just in case;
        auto newClipboard = getClipboard(clipboard.name);
        clipboard.size = newClipboard->size;
        clipboard.data = newClipboard->data;
        return true;
    }
}
