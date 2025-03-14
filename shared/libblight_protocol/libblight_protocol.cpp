#include "libblight_protocol.h"
#include "socket.h"
#include "_debug.h"

#include <fcntl.h>
#include <cstring>
#include <linux/socket.h>
#include <unistd.h>

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
#define error_message(err, return_value) \
    (err.message != nullptr ? err.message : std::strerror(-return_value))

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
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if(dfd < 0){
            _WARN(
                "[blight_service_open::fcntl(...)] Error: %s",
                error_message(error, res)
            );
            errno = -dfd;
        }
        if(message != nullptr){
            sd_bus_message_unref(message);
        }
        sd_bus_error_free(&error);
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
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if(dfd < 0){
            _WARN(
                "[blight_service_input_open::fcntl(...)] Error: %s",
                error_message(error, res)
            );
            errno = -dfd;
        }
        if(message != nullptr){
            sd_bus_message_unref(message);
        }
        sd_bus_error_free(&error);
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
        *message = m;
        return m->header.size;
    }
    void blight_message_deref(blight_message_t* message){
        if(message->data != nullptr){
            if(!message->header.size){
                _WARN("Freeing blight_message_t with a data pointer but a size of 0");
            }
            delete message->data;
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
        _WARN("%d %d %d", type, ackid, size);
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
}
