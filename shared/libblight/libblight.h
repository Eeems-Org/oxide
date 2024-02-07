/*!
 * \addtogroup Blight
 * \brief The Blight module
 * @{
 * \file
 */
#include "libblight_global.h"
#include "types.h"
#include "connection.h"
#include <optional>

namespace Blight{
    /*!
     * \brief Connect to DBus
     * \param use_system Use the system bus instead of the session bus.
     * \return If the connection was successful.
     * \retval true Connected successfully.
     * \retval false An error occurred.
     */
    LIBBLIGHT_EXPORT bool connect(bool use_system = true);
    /*!
     * \brief If the display server service can be found over DBus
     * \return If the display server service can be found over DBus
     * \retval true The service exists.
     * \retval false The service does not exist, or an error occurred.
     */
    LIBBLIGHT_EXPORT bool exists();
    /*!
     * \brief Get or create the Connection instance to the display server.
     * \return The connection instance is returned.
     * \retval nullptr an error occurred
     */
    LIBBLIGHT_EXPORT Connection* connection();
    /*!
     * \brief Open a connection to the display server
     * \return File descriptor for socket to communicate with the display server.
     * \retval -EAGAIN Failed to connect to DBus, or the display server service does not exist.
     * \sa Blight::Connection::Connection(int)
     * \sa Blight::Connection::handle()
     */
    LIBBLIGHT_EXPORT int open();
    /*!
     * \brief Open a connection to the display server to recieve input events.
     * \return File descriptor for socket that the display server will use to send input events
     * \sa Blight::Connection::input_handle()
     * \sa Blight::Connection::read_event()
     */
    LIBBLIGHT_EXPORT int open_input();
    /*!
     * \brief Get the clipboard
     * \return Clipboard instance
     */
    LIBBLIGHT_EXPORT std::optional<clipboard_t> clipboard();
    /*!
     * \brief Get the current selection
     * \return Current selection instance
     */
    LIBBLIGHT_EXPORT std::optional<clipboard_t> selection();
    /*!
     * \brief Get the secondary selection
     * \return Secondary selection instance
     */
    LIBBLIGHT_EXPORT std::optional<clipboard_t> secondary();
    /*!
     * \brief Set a clipboard to a new value
     * \param clipboard Clipboard instance to set
     * \return If the clipboard was set
     * \retval true The clipboard was set
     * \retval false There was an error setting the clipboard
     */
    LIBBLIGHT_EXPORT bool setClipboard(clipboard_t& clipboard);
    /*!
     * \brief Update a clipboard with the current data
     * \param clipboard Clipboard instance to update
     * \return If the clipboard was updated
     * \retval true The clipboard was updated
     * \retval false There was an error updating the clipboard
     */
    LIBBLIGHT_EXPORT bool updateClipboard(clipboard_t& clipboard);
    /*!
     * \brief createBuffer
     * \param x X coordinate of the buffer
     * \param y Y coordinate of the buffer
     * \param width Width of the buffer
     * \param height Height of the buffer
     * \param stride Bytes per line in the buffer
     * \param format Format of the buffer
     * \return The new buffer
     */
    LIBBLIGHT_EXPORT std::optional<shared_buf_t> createBuffer(
        int x,
        int y,
        int width,
        int height,
        int stride,
        Format format
    );
    /*!
     * \brief Add a new surface to the display server
     * \param fd File descriptor that points to the buffer data for the surface
     * \param x X coordinate of the surface
     * \param y Y coordinate of the surface
     * \param width Width of the surface
     * \param height Height of the surface
     * \param stride Bytes per line in the buffer for the surface
     * \param format Format of the buffer for the surface
     * \return Identifier for the surface
     */
    LIBBLIGHT_EXPORT std::optional<surface_id_t> addSurface(
        int fd,
        int x,
        int y,
        int width,
        int height,
        int stride,
        Format format
    );
    /*!
     * \brief Add a new surface to the display server
     * \param buf Buffer to use for the surface
     */
    LIBBLIGHT_EXPORT inline void addSurface(shared_buf_t buf){
        buf->surface = addSurface(
            buf->fd,
            buf->x,
            buf->y,
            buf->width,
            buf->height,
            buf->stride,
            buf->format
        ).value_or(0);
    }
    /*!
     * \brief Repaint a surface, or all surfaces for a connection
     * \param identifier Identifier of the surface or connection.
     * \return Negative number if there was an error
     */
    LIBBLIGHT_EXPORT int repaint(const std::string& identifier);
    /*!
     * \brief Get the file descriptor of the buffer for a surface
     * \param identifier Surface idnetifier
     * \return Negative number if there was an error. Otherwise the file descriptor of the buffer for the surface
     */
    LIBBLIGHT_EXPORT int getSurface(surface_id_t identifier);
}
/*! @} */
