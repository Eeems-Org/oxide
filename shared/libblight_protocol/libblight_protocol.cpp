#include "libblight_protocol.h"
#include "socket.h"
#include "_debug.h"

#include <fcntl.h>
#include <cstring>
#include <linux/socket.h>
#include <unistd.h>
#include <sys/mman.h>
#include <random>
#include <sstream>
#include <ios>

#define XCONCATENATE(x, y) x ## y
#define CONCATENATE(x, y) XCONCATENATE(x, y)
#define UNIQ_T(x, uniq) CONCATENATE(__unique_prefix_, CONCATENATE(x, uniq))
#define UNIQ __COUNTER__
#define _STRV_FOREACH(s, l, i) for(typeof(*(l)) *s, *i = (l); (s = i) && *i; i++)
#define STRV_FOREACH(s, l) _STRV_FOREACH(s, l, UNIQ_T(i, UNIQ))

char** strv_free(char** v) {
    if(!v){
        return NULL;
    }
    for(char** i = v; *i; i++){
        free(*i);
    }
    free(v);
    return NULL;
}
inline const char* error_message(const sd_bus_error& err, int return_value){
    return err.message != nullptr ? err.message : std::strerror(-return_value);
}

std::string generate_uuid_v4(){
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };
    return ss.str();
}

using namespace BlightProtocol;
extern "C" {
    int blight_bus_connect_system(blight_bus** bus){
        return sd_bus_default_system(bus);
    }
    int blight_bus_connect_user(blight_bus** bus){
        return sd_bus_default_user(bus);
    }
    void blight_bus_deref(blight_bus* bus){
        sd_bus_unref(bus);
    }
    bool blight_service_available(blight_bus* bus){
        char** names = NULL;
        char** activatable = NULL;
        int res = sd_bus_list_names(bus, &names, &activatable);
        if(res < 0){
            _WARN(
                "[blight_service_available::sd_bus_list_names] Error: %s",
                std::strerror(-res)
            );
            strv_free(names);
            strv_free(activatable);
            return false;
        }
        STRV_FOREACH(i, names){
            if(std::string(*i) == BLIGHT_SERVICE){
                strv_free(names);
                strv_free(activatable);
                return true;
            }
        }
        STRV_FOREACH(i, activatable){
            if(std::string(*i) == BLIGHT_SERVICE){
                strv_free(names);
                strv_free(activatable);
                return true;
            }
        }
        strv_free(names);
        strv_free(activatable);
        return false;
    }
    int blight_service_open(blight_bus* bus){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return -EAGAIN;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "open",
            &error,
            &message,
            ""
        );
        if(res < 0){
            _WARN(
                "[blight_service_open::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int fd;
        res = sd_bus_message_read(message, "h", &fd);
        if(res < 0){
            _WARN(
                "[blight_service_open::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        int e = errno;
        if(message != nullptr){
            sd_bus_message_unref(message);
        }
        sd_bus_error_free(&error);
        if(dfd < 0){
            _WARN(
                "[blight_service_open::fcntl(...)] Error: %s",
                std::strerror(e)
            );
            errno = e;
            return -errno;
        }
        return dfd;
    }
    int blight_service_input_open(blight_bus* bus){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return -EAGAIN;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "openInput",
            &error,
            &message,
            ""
        );
        if(res < 0){
            _WARN(
                "[blight_service_input_open::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int fd;
        res = sd_bus_message_read(message, "h", &fd);
        if(res < 0){
            _WARN(
                "[blight_service_input_open::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        int e = errno;
        if(message != nullptr){
            sd_bus_message_unref(message);
        }
        sd_bus_error_free(&error);
        if(dfd < 0){
            _WARN(
                "[blight_service_input_open::fcntl(...)] Error: %s",
                std::strerror(e)
            );
            errno = e;
            return -errno;
        }
        return dfd;
    }
    blight_header_t blight_header_from_data(blight_data_t data){
        blight_header_t header;
        memcpy(&header, data, sizeof(blight_header_t));
        return header;
    }
    blight_message_t* blight_message_from_data(blight_data_t data){
        auto message = new blight_message_t;
        message->header = blight_header_from_data(data);
        if(message->header.size){
            message->data = new unsigned char[message->header.size];
            memcpy(message->data, &data[sizeof(message->header)], message->header.size);
        }else{
            message->data = nullptr;
        }
        return message;
    }
    int blight_message_from_socket(int fd, blight_message_t** message){
        auto maybe = recv(fd, sizeof(blight_header_t));
        if(!maybe.has_value()){
            if(errno != EAGAIN && errno != EINTR){
                _WARN(
                    "Failed to read connection message header: %s socket=%d",
                    std::strerror(errno),
                    fd
                );
            }
            return -errno;
        }
        auto m = new blight_message_t{
            .header = blight_header_t{
              .type = BlightMessageType::Invalid,
              .ackid = 0,
              .size = 0
            },
            .data = nullptr
        };
        memcpy(&m->header, maybe.value(), sizeof(blight_header_t));
        delete[] maybe.value();
        if(!m->header.size){
            *message = m;
            return 0;
        }
        maybe = recv_blocking(fd, m->header.size);
        if(!maybe.has_value()){
            _WARN(
                "Failed to read connection message data: %s "
                "socket=%d, "
                "ackid=%u, "
                "type=%d, "
                "size=%ld",
                std::strerror(errno),
                fd,
                m->header.ackid,
                m->header.type,
                (long int)m->header.size
            );
            return -errno;
        }
        m->data = maybe.value();
        *message = m;
        return m->header.size;
    }
    void blight_message_deref(blight_message_t* message){
        if(message->data != nullptr){
            if(!message->header.size){
                _WARN("Freeing blight_message_t with a data pointer but a size of 0");
            }
            delete[] message->data;
        }
        delete message;
    }
    int blight_send_message(
        int fd,
        BlightMessageType type,
        unsigned int ackid,
        uint32_t size,
        blight_data_t data
    ){
        if(size && data == nullptr){
            _WARN("Data missing when size is not zero");
            return -EINVAL;
        }
        blight_header_t header{
            .type = type,
            .ackid = ackid,
            .size = size
        };
        if(!send_blocking(fd, (blight_data_t)&header, sizeof(blight_header_t))){
            _WARN("Failed to write connection message header: %s", std::strerror(errno));
            return -errno;
        }
        if(size && !send_blocking(fd, data, size)){
            _WARN("Failed to write connection message data: %s", std::strerror(errno));
            return -errno;
        }
        return size;
    }
    blight_buf_t* blight_create_buffer(
        int x,
        int y,
        int width,
        int height,
        int stride,
        BlightImageFormat format
    ){
        int fd = memfd_create(generate_uuid_v4().c_str(), MFD_ALLOW_SEALING);
        if(fd < 0){
            return nullptr;
        }
        ssize_t size = stride * height;
        if(ftruncate(fd, size)){
            int e = errno;
            close(fd);
            errno = e;
            return nullptr;
        }
        void* data = mmap(
            NULL,
            size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED_VALIDATE,
            fd,
            0
        );
        if(data == MAP_FAILED || data == nullptr){
            int e = errno;
            close(fd);
            errno = e;
            return nullptr;
        }
        return new blight_buf_t{
            .fd = fd,
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .stride = stride,
            .format = format,
            .data = (blight_data_t)data
        };
    }
    void blight_buffer_deref(blight_buf_t* buf){
        if(buf == nullptr){
            return;
        }
        if(buf->data != nullptr){
            ssize_t size = buf->stride * buf->height;
            if (munmap(buf->data, size) != 0) {
                _WARN("Failed to unmap buffer: %s", std::strerror(errno));
            }
        }
        delete buf;
    }
    blight_surface_id_t blight_add_surface(blight_bus* bus, blight_buf_t* buf){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return 0;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "addSurface",
            &error,
            &message,
            "hiiiiii",
            buf->fd,
            buf->x,
            buf->y,
            buf->width,
            buf->height,
            buf->stride,
            buf->format
        );
        if(res < 0){
            _WARN(
                "[blight_add_surface::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return 0;
        }
        blight_surface_id_t identifier;
        res = sd_bus_message_read(message, "q", &identifier);
        if(res < 0){
            _WARN(
                "[blight_add_surface::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return 0;
        }
        return identifier;
    }
    blight_packet_repaint_t* blight_cast_to_repaint_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Repaint){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_repaint_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_repaint_t*)message->data;
    }
    blight_packet_move_t* blight_cast_to_move_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Move){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_move_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_move_t*)message->data;
    }
    blight_packet_surface_info_t* blight_cast_to_surface_info_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Info){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_surface_info_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_surface_info_t*)message->data;
    }
}
