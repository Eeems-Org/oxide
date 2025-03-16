#include "_debug.h"

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <unistd.h>

/**
 * @brief Prints a formatted log header to stderr based on the specified priority.
 *
 * Constructs a log header that includes the process group ID, process ID, thread ID, the executable path,
 * the process name, and the log level. The log level is determined by the `priority` parameter as follows:
 * - LOG_INFO: "Info"
 * - LOG_WARNING: "Warning"
 * - LOG_CRIT: "Critical"
 * - Any other value: "Debug"
 *
 * The function retrieves the process name using prctl and the executable path using realpath, then frees
 * the allocated memory for the executable path after printing.
 *
 * @param priority Numeric priority indicating the log level.
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
 * @brief Prints a log footer with source location details.
 *
 * Outputs a formatted footer to stderr that includes the source file name,
 * line number, and function name where the log entry was generated.
 *
 * @param file The name of the source file.
 * @param line The line number in the source file.
 * @param func The name of the function generating the log footer.
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
