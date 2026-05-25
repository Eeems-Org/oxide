#include "system.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "debug.h"
#include "libblight.h"
#include "meta.h"

namespace Blight {
    int frameBuffer()
    {
        if (!exists()) {
            errno = EAGAIN;
            return -1;
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "frameBuffer"
        );
        if (reply->isError()) {
            _WARN(
                "[Blight::frameBuffer()::call_method(...)] Error: %s",
                reply->error_message().c_str()
            );
            return reply->return_value;
        }
        auto fd = reply->read_value<int>("h");
        if (!fd.has_value()) {
            _WARN(
                "[Blight::frameBuffer()::read_value(\"h\")] Error: %s",
                reply->error_message().c_str()
            );
            return reply->return_value;
        }
        int dfd = dup(fd.value());
        if (dfd == -1) {
            _WARN(
                "[Blight::frameBuffer()::dup(%d)] Error: %s",
                fd.value(),
                reply->error_message().c_str()
            );
            return -errno;
        }
        return dfd;
    }
    bool enterExclusiveMode()
    {
        if (!exists()) {
            errno = EAGAIN;
            return false;
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "enterExclusiveMode"
        );
        if (reply->isError()) {
            _WARN(
                "[Blight::enterExclusiveMode()::call_method(...)] Error: %s",
                reply->error_message().c_str()
            );
            return false;
        }
        return true;
    }
    bool exitExclusiveMode()
    {
        if (!exists()) {
            errno = EAGAIN;
            return false;
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "exitExclusiveMode"
        );
        if (reply->isError()) {
            _WARN(
                "[Blight::exitExclusiveMode()::call_method(...)] Error: %s",
                reply->error_message().c_str()
            );
            return false;
        }
        return true;
    }
    bool exclusiveModeRepaint()
    {
        if (!exists()) {
            errno = EAGAIN;
            return false;
        }
        auto reply = dbus->call_method(
            BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "exclusiveModeRepaint"
        );
        if (reply->isError()) {
            _WARN(
                "[Blight::exclusiveModeRepaint()::call_method(...)] Error: %s",
                reply->error_message().c_str()
            );
            return false;
        }
        return true;
    }
} // namespace Blight
