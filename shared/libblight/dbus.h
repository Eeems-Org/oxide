/*!
 * \addtogroup Blight
 * @{
 * \file
 */
#pragma once
#include "libblight_global.h"

#include <systemd/sd-bus.h>
#include <vector>
#include <tuple>

namespace Blight {
    /*!
     * \brief The DBusReply class
     */
    class LIBBLIGHT_EXPORT DBusReply{
    public:
        DBusReply();
        ~DBusReply();

        /*!
         * \brief Message descriptor
         */
        sd_bus_message* message = nullptr;
        /*!
         * \brief Error descriptor
         */
        sd_bus_error error;
        /*!
         * \brief Return value of the last operation
         */
        int return_value;
        /*!
         * \brief Error message as a string
         * \return
         */
        std::string error_message();
        /*!
         * \brief If this reply is an error
         * \return
         */
        bool isError();

        /*!
         * \brief Read a value from the reply message
         * \param argument_type Type of data to read from the reply message
         * \return
         */
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
    /*!
     * \brief Shared pointer of a DBusReply
     */
    LIBBLIGHT_EXPORT typedef std::shared_ptr<DBusReply> dbus_reply_t;
    /*!
     * \brief An exception raised by DBus
     * \sa Blight::DBus::DBus(bool)
     */
    class LIBBLIGHT_EXPORT DBusException : public std::runtime_error{
    public:
        DBusException(const std::string& message);
    };
    /*!
     * \brief A helper class for dealing with sd-bus
     */
    class LIBBLIGHT_EXPORT DBus {
    public:
        /*!
         * \brief Create a new DBus connection
         * \param use_system Use the system bus instead of the session bus
         * \exception Blight::DBusException Failed to connect to the bus
         */
        DBus(bool use_system = false);
        ~DBus();
        /*!
         * \brief underlying sd_bus object
         * \return The sd_bus object used by sd-bus
         */
        sd_bus* bus();
        /*!
         * \brief All the known services on the bus
         * \return All the known services on the bus
         * \sa has_service(std::string)
         */
        std::vector<std::string> services();
        /*!
         * \brief Check if a service is available on the bus.
         * \param service Service name.
         * \return If  the service is available.
         * \sa services()
         */
        bool has_service(std::string service);
        /*!
         * \brief Call a DBus method
         * \param service Service name
         * \param path Path
         * \param interface Interface name
         * \param member Method name
         * \param argument_types Argument types
         * \param args Arguments
         * \return DBusReply
         */
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
        /*!
         * \brief Call a DBus method
         * \param service Service name
         * \param path Path
         * \param interface Interface name
         * \param member Method name
         * \return DBusReply
         */
        inline dbus_reply_t call_method(
            const std::string& service,
            const std::string& path,
            const std::string& interface,
            const std::string& member
        ){ return call_method(service, path, interface, member, ""); }
        /*!
         * \brief Get a property from DBus
         * \param service Service name
         * \param path Path
         * \param interface Interface name
         * \param member Property name
         * \param property_type Property type
         * \return DBusReply
         */
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
/*! @} */
