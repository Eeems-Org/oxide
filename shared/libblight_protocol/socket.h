/*!
 * \addtogroup BlightProtocol
 * @{
 * \file
 */
#pragma once
#include "libblight_protocol.h"
#include <optional>

#ifdef __cplusplus
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
#define blight_message_t BlightProtocol::blight_message_t
#define blight_header_t BlightProtocol::blight_header_t
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
/*! @} */
