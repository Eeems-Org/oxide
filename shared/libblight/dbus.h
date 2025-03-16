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

/**
 * @brief Represents a reply received from a D-Bus method call.
 *
 * Encapsulates the response from a D-Bus operation, including the raw message pointer,
 * any associated error information, and the return value of the operation.
 */

/**
 * @brief Retrieves the error message from the reply.
 *
 * Converts the stored error information into a human‐readable string.
 *
 * @return A string describing the error if one exists; otherwise, an empty string.
 */

/**
 * @brief Determines if the reply contains an error.
 *
 * Inspects the return value and error descriptor to ascertain whether an error occurred
 * during the D-Bus operation.
 *
 * @return true if the reply indicates an error; false otherwise.
 */

/**
 * @brief Extracts a value from the reply message.
 *
 * Attempts to read a value of type T from the reply using the specified D-Bus type signature.
 * If the extraction fails, errno is set to the negative return value and an empty optional is returned.
 *
 * @tparam T The type of the value to extract.
 * @param argument_type A string representing the expected D-Bus type signature of the value.
 * @return An optional containing the value if successfully read, or empty if an error occurred.
 */

/**
 * @brief Exception type for D-Bus related errors.
 *
 * This exception is thrown when a D-Bus operation encounters a failure.
 */

/**
 * @brief Constructs a new DBusException with a descriptive error message.
 *
 * @param message A string detailing the error that occurred.
 */

/**
 * @brief Facilitates interactions with the D-Bus.
 *
 * Provides methods to establish a connection, query available services, call methods,
 * and retrieve properties from D-Bus services.
 */

/**
 * @brief Initializes a new D-Bus connection.
 *
 * Establishes a connection to the D-Bus. If the parameter \a use_system is true, the system bus is used;
 * otherwise, the session bus is selected. A DBusException is thrown if the connection attempt fails.
 *
 * @param use_system Connect to the system bus if true; otherwise, connect to the session bus.
 * @exception DBusException Thrown if the connection to the specified bus fails.
 */

/**
 * @brief Cleans up the D-Bus connection.
 *
 * Closes the connection to the D-Bus and releases any associated resources.
 */

/**
 * @brief Retrieves the underlying sd_bus connection.
 *
 * Provides direct access to the raw sd_bus pointer used for D-Bus communication.
 *
 * @return A pointer to the underlying sd_bus object.
 */

/**
 * @brief Enumerates all known services on the D-Bus.
 *
 * Queries the D-Bus for the list of available service names.
 *
 * @return A vector of strings where each string represents a known D-Bus service.
 *
 * @sa has_service
 */

/**
 * @brief Checks for the existence of a service on the D-Bus.
 *
 * Determines whether the specified service is among the currently available services.
 *
 * @param service The name of the service to check.
 * @return true if the service is available; false otherwise.
 *
 * @sa services
 */

/**
 * @brief Calls a D-Bus method with provided arguments.
 *
 * Invokes the specified method on a D-Bus service using the given service name, object path,
 * interface, and method member. It verifies that the number of arguments matches the expected count
 * based on the provided argument type string. If the argument count mismatches, the return value is set to EINVAL.
 * In the event of an error during the call, errno is updated accordingly.
 *
 * @tparam Args Types of the arguments passed to the method.
 * @param service The target service name.
 * @param path The object path on the service.
 * @param interface The interface name where the method resides.
 * @param member The method name to be invoked.
 * @param argument_types A string representing the D-Bus type signatures of the expected arguments.
 * @param args The arguments to be sent with the method call.
 * @return A shared pointer to a DBusReply containing the result of the method invocation.
 */

/**
 * @brief Calls a D-Bus method that requires no arguments.
 *
 * A convenience overload for invoking a D-Bus method without any arguments.
 *
 * @param service The target service name.
 * @param path The object path on the service.
 * @param interface The interface name under which the method is defined.
 * @param member The method name to be called.
 * @return A shared pointer to a DBusReply containing the outcome of the method call.
 */

/**
 * @brief Retrieves a property value from a D-Bus service.
 *
 * Queries the specified property from a D-Bus service using the provided object path, interface,
 * and property name. The property is expected to conform to the provided D-Bus type signature.
 *
 * @param service The name of the service.
 * @param path The object path where the property resides.
 * @param interface The interface that contains the property.
 * @param member The property name to retrieve.
 * @param property_type A string representing the D-Bus type signature of the property.
 * @return A shared pointer to a DBusReply containing the property value.
 */
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
    typedef std::shared_ptr<DBusReply> dbus_reply_t;
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
