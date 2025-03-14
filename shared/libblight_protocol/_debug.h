#pragma once

#include <mutex>
#include <systemd/sd-journal.h>

static std::mutex __log_mutex;

void __printf_header(int priority);

void __printf_footer(const char* file, unsigned int line, const char* func);

#define _PRINTF(priority, ...) \
    __log_mutex.lock(); \
    __printf_header(priority); \
    fprintf(stderr, __VA_ARGS__); \
    __printf_footer(__FILE__, __LINE__, __PRETTY_FUNCTION__); \
    __log_mutex.unlock(); \


#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)
