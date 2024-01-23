#pragma once

#include <systemd/sd-journal.h>
#include <unistd.h>
#include <linux/prctl.h>

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
    std::string path(realpath("/proc/self/exe", NULL));
    fprintf(
        stderr,
        "[%i:%i:%i %s - %s] %s: ",
        getpgrp(),
        getpid(),
        gettid(),
        path.c_str(),
        name,
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
static int BLIGHT_DEBUG_LOGGING = 4;
#define _PRINTF(priority, ...) \
if(priority <= BLIGHT_DEBUG_LOGGING){ \
    __log_mutex.lock(); \
    __printf_header(priority); \
    fprintf(stderr, __VA_ARGS__); \
    __printf_footer(__FILE__, __LINE__, __PRETTY_FUNCTION__); \
    __log_mutex.unlock(); \
}
#define _DEBUG(...) _PRINTF(LOG_DEBUG, __VA_ARGS__)
#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)
#define _INFO(...) _PRINTF(LOG_INFO, __VA_ARGS__)
#define _CRIT(...) _PRINTF(LOG_CRIT, __VA_ARGS__)
#define __RIGHT_HERE__ fprintf("<============================ %s:%d", __FILE__, __LINE__)
