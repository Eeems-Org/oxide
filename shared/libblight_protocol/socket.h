/*!
 * \addtogroup BlightProtocol
 * @{
 * \file
 */
#pragma once
#include "libblight_protocol.h"
#include <optional>

#ifdef __cplusplus
/**
 * @brief Attempts to receive data from a socket in a non-blocking manner.
 *
 * This function makes several attempts to receive the requested number of bytes from the specified socket,
 * waiting for a defined timeout between each try. If data is successfully received during any attempt, it is returned;
 * otherwise, an empty optional is returned.
 *
 * @param fd The file descriptor of the socket.
 * @param size The number of bytes to receive.
 * @param attempts The maximum number of attempts to receive data.
 * @param timeout The timeout in milliseconds for each attempt.
 * @return An optional containing the received data, or empty if the operation failed.
 */

/**
 * @brief Receives data from a socket in a blocking manner.
 *
 * This function performs a blocking read to retrieve the specified number of bytes from the given socket.
 * If the operation completes successfully, the received data is returned; otherwise, an empty optional is returned.
 *
 * @param fd The file descriptor of the socket.
 * @param size The number of bytes to receive.
 * @return An optional containing the received data, or empty if an error occurred.
 */

/**
 * @brief Sends data to a socket using a blocking operation.
 *
 * This function performs a blocking send to transmit the specified number of bytes to the given socket.
 * It returns a boolean value indicating whether the data was sent successfully.
 *
 * @param fd The file descriptor of the socket.
 * @param data The data to be sent.
 * @param size The number of bytes to send.
 * @return True if the data was sent successfully; false otherwise.
 */

/**
 * @brief Waits until the socket is ready to send data.
 *
 * This function blocks until the socket is ready for sending data or until the specified timeout expires.
 * A negative timeout value indicates that the function should wait indefinitely.
 *
 * @param fd The file descriptor of the socket.
 * @param timeout The timeout in milliseconds (negative for an indefinite wait).
 * @return True if the socket is ready to send data; false if the timeout expires first.
 */

/**
 * @brief Waits until the socket is ready to receive data.
 *
 * This function blocks until the socket is ready for receiving data or until the specified timeout expires.
 * A negative timeout value indicates that the function should wait indefinitely.
 *
 * @param fd The file descriptor of the socket.
 * @param timeout The timeout in milliseconds (negative for an indefinite wait).
 * @return True if the socket is ready to receive data; false if the timeout expires first.
 */
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
