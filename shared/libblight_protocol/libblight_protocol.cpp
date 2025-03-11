#include "libblight_protocol.h"

#include <systemd/sd-journal.h>
#include <unistd.h>
#include <linux/prctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <mutex>
#include <sys/prctl.h>
#include <cstring>
#include <linux/socket.h>

#define XCONCATENATE(x, y) x ## y
#define CONCATENATE(x, y) XCONCATENATE(x, y)
#define UNIQ_T(x, uniq) CONCATENATE(__unique_prefix_, CONCATENATE(x, uniq))
#define UNIQ __COUNTER__
#define _STRV_FOREACH(s, l, i) for(typeof(*(l)) *s, *i = (l); (s = i) && *i; i++)
#define STRV_FOREACH(s, l) _STRV_FOREACH(s, l, UNIQ_T(i, UNIQ))

static std::mutex __log_mutex;

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
    char name[16];
    prctl(PR_GET_NAME, name);
    auto selfpath = realpath("/proc/self/exe", NULL);
    fprintf(
        stderr,
        "[%i:%i:%i %s - %s] %s: ",
        getpgrp(),
        getpid(),
        gettid(),
        selfpath,
        name,
        level.c_str()
    );
    free(selfpath);
}

void __printf_footer(const char* file, unsigned int line, const char* func){
    fprintf(
        stderr,
        " (%s:%u, %s)\n",
        file,
        line,
        func
    );
}

#define _PRINTF(priority, ...) \
    __log_mutex.lock(); \
    __printf_header(priority); \
    fprintf(stderr, __VA_ARGS__); \
    __printf_footer(__FILE__, __LINE__, __PRETTY_FUNCTION__); \
    __log_mutex.unlock(); \


#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)

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

#include <iostream>
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
        return dfd;
    }
    blight_header_t blight_header_from_data(blight_data_t data){
        blight_header_t header;
        memcpy(&header, data, sizeof(blight_header_t));
        return header;
    }
    blight_message_t blight_message_from_data(blight_data_t data){
        blight_message_t message;
        message.header = blight_header_from_data(data);
        message.data = new unsigned char[message.header.size];
        memcpy(message.data, &data[sizeof(message.header)], message.header.size);
        return message;
    }
}
