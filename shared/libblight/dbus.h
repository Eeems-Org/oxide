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

/*!
 * \brief Encapsulates a reply from a D-Bus method call.
 *
 * This class holds the D-Bus message, error details, and the return code of the last operation.
 */

/*!
 * \brief Constructs a new DBusReply object.
 *
 * Initializes the message pointer to nullptr and sets default values for error tracking.
 */

/*!
 * \brief Destructs the DBusReply object.
 *
 * Releases any resources associated with the D-Bus reply.
 */

/*!
 * \brief Retrieves a descriptive error message.
 *
 * If an error occurred during the D-Bus operation, this function returns the corresponding error message;
 * otherwise, it returns an empty string.
 *
 * \return A string containing the error message or an empty string if no error is present.
 */

/*!
 * \brief Checks if the DBusReply indicates an error.
 *
 * \return true if an error occurred during the last D-Bus operation, false otherwise.
 */

/*!
 * \brief Reads a value of type T from the D-Bus reply message.
 *
 * Attempts to extract a value using the specified D-Bus type signature. On failure, sets errno to the negative return code and returns an empty optional.
 *
 * \tparam T The type to read from the reply.
 * \param argument_type The D-Bus signature of the expected value.
 * \return A std::optional containing the value if successful; otherwise, an empty std::optional.
 */

/*!
 * \brief Defines a shared pointer type for DBusReply objects.
 */

/*!
 * \brief Represents an exception thrown during D-Bus operations.
 *
 * Inherits from std::runtime_error.
 */

/*!
 * \brief Constructs a new DBusException with an error message.
 *
 * \param message A string describing the error.
 */

/*!
 * \brief Provides an interface for managing D-Bus connections and communications.
 *
 * The DBus class handles establishing connections to the D-Bus, invoking methods, and retrieving properties from services.
 */

/*!
 * \brief Creates a new D-Bus connection.
 *
 * Connects to the system bus if \a use_system is true; otherwise, connects to the session bus.
 *
 * \param use_system If true, use the system bus instead of the session bus.
 * \throws DBusException if the connection fails.
 */

/*!
 * \brief Closes the D-Bus connection and releases associated resources.
 */

/*!
 * \brief Returns the underlying sd_bus object for D-Bus communication.
 *
 * \return A pointer to the sd_bus instance.
 */

/*!
 * \brief Retrieves a list of available D-Bus services.
 *
 * \return A vector of strings where each element is a service name available on the D-Bus.
 */

/*!
 * \brief Checks if a given D-Bus service is available.
 *
 * \param service The name of the service to check.
 * \return true if the specified service is present on the bus, false otherwise.
 */

/*!
 * \brief Calls a method on a D-Bus service with arguments.
 *
 * Verifies that the number of argument type characters matches the number of provided arguments.
 * If the counts differ, the reply's return value is set to EINVAL. Invokes the method and returns a DBusReply shared pointer.
 * Updates errno with the negative return code if an error occurs.
 *
 * \param service The target service name.
 * \param path The object path on the D-Bus.
 * \param interface The D-Bus interface name.
 * \param member The method name to call.
 * \param argument_types A string specifying the expected argument types.
 * \param args The arguments to pass to the D-Bus method.
 * \return A shared pointer to a DBusReply containing the result.
 */

/*!
 * \brief Calls a method on a D-Bus service without arguments.
 *
 * A convenience overload that invokes the method with an empty argument type signature.
 *
 * \param service The target service name.
 * \param path The object path on the D-Bus.
 * \param interface The D-Bus interface name.
 * \param member The method name to call.
 * \return A shared pointer to a DBusReply containing the result.
 */

/*!
 * \brief Retrieves the value of a property from a D-Bus service.
 *
 * Queries a property from the specified service and returns the reply.
 *
 * \param service The service name.
 * \param path The object path on the D-Bus.
 * \param interface The interface that contains the property.
 * \param member The property name.
 * \param property_type The D-Bus signature of the property's type.
 * \return A shared pointer to a DBusReply containing the property value.
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
