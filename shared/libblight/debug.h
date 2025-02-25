/*!
 * \addtogroup Blight
 * @{
 * \file
 */
#pragma once

#include <systemd/sd-journal.h>
#include <unistd.h>
#include <linux/prctl.h>
#include <stdio.h>
#include <mutex>
#include <sys/prctl.h>

static std::mutex __log_mutex;
/*!
 * \brief BLIGHT_DEBUG_LOGGING
 *
 * Used to filter debug messages based on the priority.
 */
static int BLIGHT_DEBUG_LOGGING = 4;
void __printf_header(int priority);
void __printf_footer(char const* file, unsigned int line, char const* func);

/*!
 * \brief Log a message to stderr
 */
#define _PRINTF(priority, ...) \
    if(priority <= BLIGHT_DEBUG_LOGGING){ \
        __log_mutex.lock(); \
        __printf_header(priority); \
        fprintf(stderr, __VA_ARGS__); \
        __printf_footer(__FILE__, __LINE__, __PRETTY_FUNCTION__); \
        __log_mutex.unlock(); \
    }

/*!
 * \brief Log a debug statement to stderrr
 */
#define _DEBUG(...) _PRINTF(LOG_DEBUG, __VA_ARGS__)
/*!
 * \brief Log a warning statement to stderrr
 */
#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)
/*!
 * \brief Log an informational statement to stderrr
 */
#define _INFO(...) _PRINTF(LOG_INFO, __VA_ARGS__)
/*!
 * \brief Log a critical exception statement to stderrr
 */
#define _CRIT(...) _PRINTF(LOG_CRIT, __VA_ARGS__)
/*!
 * \brief Log a debug line indicating that a certain line of code has been reached
 */
#ifndef __RIGHT_HERE__
#define __RIGHT_HERE__ fprintf(stderr, "<============================ %s:%d\n", __FILE__, __LINE__)
#endif
/*! @} */
