#include "_debug.h"

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <unistd.h>

/**
 * @brief Outputs a formatted log header to stderr.
 *
 * This function prints a log header containing the process group ID, process ID, thread ID, the executable path, and the process name.
 * The log level is determined by the provided priority argument and mapped to one of "Info", "Warning", "Critical", or "Debug".
 *
 * @param priority The log level indicator. Recognized values include LOG_INFO, LOG_WARNING, and LOG_CRIT; unrecognized values default to "Debug".
 */
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

/**
 * @brief Outputs a formatted log footer with location details.
 *
 * This function writes a log footer to standard error that includes the source file name,
 * the line number, and the function name where the log statement was invoked.
 *
 * @param file The name of the source file.
 * @param line The line number in the source file.
 * @param func The name of the function where the log is generated.
 */
void __printf_footer(const char* file, unsigned int line, const char* func){
    fprintf(
        stderr,
        " (%s:%u, %s)\n",
        file,
        line,
        func
    );
}
