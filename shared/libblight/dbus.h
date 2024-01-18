#pragma once
#include "libblight_global.h"

#include <systemd/sd-bus.h>
#include <vector>

namespace Blight {
    class LIBBLIGHT_EXPORT DBusException : public std::runtime_error{
    public:
        DBusException(std::string message);
    };
    class LIBBLIGHT_EXPORT DBus {
    public:
        DBus(bool use_system = false);
        ~DBus();
        sd_bus* bus();
        std::vector<std::string> services();
        bool has_service(std::string service);

    private:
        sd_bus* m_bus;
    };
}
