/*!
 * \addtogroup BlightProtocol
 * @{
 * \file
 */
#pragma once
#include "libblight_protocol.h"

#ifdef __cplusplus
#include <optional>

namespace BlightProtocol{
    /*!
     * \brief Non-blocking recieve data from a socket.
     * \param fd The socket.
     * \param size Size of the data to recieve.
     * \param attempts Number of attempts to recieve data.
     * \param timeout Timeout to wait for data per attempt.
     * \return The data if it was recieved.
     */
    LIBBLIGHT_PROTOCOL_EXPORT std::optional<blight_data_t> recv(
        int fd,
        ssize_t size,
        unsigned int attempts = 5,
        unsigned int timeout = 50
    );
    /*!
     * \brief Recieve data from a socket
     * \param fd The socket.
     * \param size Size of the data to recieve
     * \return The data if it was recieved without error.
     */
    LIBBLIGHT_PROTOCOL_EXPORT std::optional<blight_data_t> recv_blocking(
        int fd,
        ssize_t size
    );
    /*!
     * \brief Send data to a socket.
     * \param fd The socket
     * \param data The data to send.
     * \param size The size of the data to send.
     * \return If the data was sent without error.
     */
    LIBBLIGHT_PROTOCOL_EXPORT bool send_blocking(
        int fd,
        const blight_data_t data,
        ssize_t size
    );
    /*!
     * \brief Wait until a socket is ready to send data.
     * \param fd The socket
     * \param timeout Timeout
     * \return If the socket is ready to send data.
     */
    LIBBLIGHT_PROTOCOL_EXPORT bool wait_for_send(int fd, int timeout = -1);
    /*!
     * \brief Wait until a socket is ready for us to recieve data.
     * \param fd The socket
     * \param timeout Timeout
     * \return If the socket is ready for us to recieve data.
     */
    LIBBLIGHT_PROTOCOL_EXPORT bool wait_for_read(int fd, int timeout = -1);
}
#define blight_data_t BlightProtocol::blight_data_t
extern "C" {
#endif

    /*!
     * \brief Recieve data from a socket
     * \param fd The socket.
     * \param size Size of the data to recieve
     * \param data The data if it was recieved without error.
     * \return 0 if data was recieved without error
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_recv(
        int fd,
        blight_data_t* data,
        ssize_t size
    );
    /*!
     * \brief Recieve data from a socket, blocking until data is recieved.
     * \param fd The socket.
     * \param size Size of the data to recieve
     * \param data The data if it was recieved without error.
     * \return 0 if data was recieved without error
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_recv_blocking(
        int fd,
        blight_data_t* data,
        ssize_t size
    );
    /*!
     * \brief Send data to a socket.
     * \param fd The socket
     * \param data The data to send.
     * \param size The size of the data to send.
     * \return 0 if the data was sent without error.
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_send_blocking(
        int fd,
        const blight_data_t data,
        ssize_t size
    );
    /*!
     * \brief Wait until a socket is ready to send data.
     * \param fd The socket
     * \param timeout Timeout. -1 will wait forever
     * \return If the socket is ready for us to send data it returns 0.
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_wait_for_send(int fd, int timeout);
    /*!
     * \brief Wait until a socket is ready for us to recieve data.
     * \param fd The socket
     * \param timeout Timeout. -1 will wait forever
     * \return If the socket is ready for us to recieve data it returns 0.
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_wait_for_read(int fd, int timeout);

#ifdef __cplusplus
}
#undef blight_data_t
#endif
/*! @} */
