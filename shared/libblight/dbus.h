#pragma once
#include "libblight_global.h"

#include <systemd/sd-bus.h>
#include <vector>
#include <tuple>

namespace Blight {
    class LIBBLIGHT_EXPORT DBusReply{
    public:
        DBusReply();
        ~DBusReply();

        sd_bus_message* message = nullptr;
        sd_bus_error error;
        int return_value;
        std::string error_message();
        bool isError();

        template<typename T>
        std::optional<T> read_value(const char* argument_type){
            T argument;
            return_value = sd_bus_message_read(message, argument_type, &argument);
            if(isError()){
                errno = -return_value;
                return {};
            }
            return argument;
        }
    };

    LIBBLIGHT_EXPORT typedef std::shared_ptr<DBusReply> dbus_reply_t;

    class LIBBLIGHT_EXPORT DBusException : public std::runtime_error{
    public:
        DBusException(const std::string message);
    };
    class LIBBLIGHT_EXPORT DBus {
    public:
        DBus(bool use_system = false);
        ~DBus();
        sd_bus* bus();
        std::vector<std::string> services();
        bool has_service(std::string service);
        template <typename ... Args>
        dbus_reply_t call_method(
            const std::string& service,
            const std::string& path,
            const std::string& interface,
            const std::string& member,
            const std::string& argument_types,
            Args... args
        ){
            auto res = dbus_reply_t(new DBusReply());
            if(argument_types.size() != sizeof...(args)){
                res->return_value = EINVAL;
                return res;
            }
            res->return_value = sd_bus_call_method(
                m_bus,
                service.c_str(),
                path.c_str(),
                interface.c_str(),
                member.c_str(),
                &res->error,
                &res->message,
                argument_types.c_str(),
                args...
            );
            if(res->isError()){
                errno = -res->return_value;
            }
            return res;
        }

        inline dbus_reply_t call_method(
            const std::string& service,
            const std::string& path,
            const std::string& interface,
            const std::string& member
        ){ return call_method(service, path, interface, member, ""); }

        dbus_reply_t get_property(
            const std::string& service,
            const std::string& path,
            const std::string& interface,
            const std::string& member,
            const std::string& property_type
        );

    private:
        sd_bus* m_bus;
    };
}
