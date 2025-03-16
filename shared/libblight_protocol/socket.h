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
 * @brief Receives data from a socket in a non-blocking manner.
 *
 * This function attempts to retrieve the specified number of bytes from the provided socket descriptor.
 * It performs a series of non-blocking read attempts, each with a defined timeout period.
 *
 * @param fd Socket file descriptor.
 * @param size Number of bytes to receive.
 * @param attempts Maximum number of non-blocking attempts.
 * @param timeout Timeout in milliseconds for each attempt.
 * @return An optional containing the received data if successful; otherwise, an empty optional.
 */

/**
 * @brief Receives data from a socket in a blocking manner.
 *
 * Performs a blocking read to obtain the specified number of bytes from the given socket.
 *
 * @param fd Socket file descriptor.
 * @param size Number of bytes to receive.
 * @return An optional containing the received data if successful; otherwise, an empty optional.
 */

/**
 * @brief Sends data to a socket in a blocking manner.
 *
 * Attempts to send the provided data buffer through the specified socket, blocking until the operation completes.
 *
 * @param fd Socket file descriptor.
 * @param data Data buffer to send.
 * @param size Number of bytes to send.
 * @return true if the data was successfully sent; otherwise, false.
 */

/**
 * @brief Waits until a socket is ready for sending data.
 *
 * Blocks until the socket is ready for writing, or until the specified timeout elapses.
 *
 * @param fd Socket file descriptor.
 * @param timeout Timeout in milliseconds to wait for readiness (a negative value indicates an indefinite wait).
 * @return true if the socket is ready for sending; otherwise, false.
 */

/**
 * @brief Waits until a socket is ready for receiving data.
 *
 * Blocks until data is available for reading from the socket, or until the specified timeout elapses.
 *
 * @param fd Socket file descriptor.
 * @param timeout Timeout in milliseconds to wait for readiness (a negative value indicates an indefinite wait).
 * @return true if the socket is ready for reading; otherwise, false.
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
