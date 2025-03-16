#include "socket.h"
#include <libblight_protocol/socket.h>

namespace Blight{
    /**
     * @brief Receives a block of data from a socket.
     *
     * Delegates to BlightProtocol::recv to attempt receiving the specified number of bytes from the
     * provided socket file descriptor. The operation will be retried up to the given number of attempts,
     * with each attempt waiting for the specified timeout duration. If the data is received successfully,
     * it is returned wrapped in an std::optional; otherwise, an empty optional is returned.
     *
     * @param fd Socket file descriptor.
     * @param size Expected number of bytes to receive.
     * @param attempts Maximum number of receive attempts.
     * @param timeout Duration to wait for each attempt.
     * @return std::optional<data_t> The received data if successful; an empty optional otherwise.
     */
    std::optional<data_t> recv(
        int fd,
        ssize_t size,
        unsigned int attempts,
        unsigned int timeout
    ){ return BlightProtocol::recv(fd, size, attempts, timeout); }

    /**
     * @brief Receives data from a socket in a blocking manner.
     *
     * This function delegates to BlightProtocol::recv_blocking to read a specified number of bytes from the given socket file descriptor. The call blocks until the data is received or an error occurs.
     *
     * @param fd Socket file descriptor.
     * @param size Number of bytes to receive.
     * @return std::optional<data_t> Received data on success; otherwise, an empty optional.
     */
    std::optional<data_t> recv_blocking(
        int fd,
        ssize_t size
    ){ return BlightProtocol::recv_blocking(fd, size); }

    /**
     * @brief Sends data to a socket in a blocking manner.
     *
     * This function attempts to send a specified number of bytes from the provided data buffer through the given socket file descriptor.
     * It blocks until all the data is sent or an error occurs, delegating the operation to BlightProtocol::send_blocking.
     *
     * @param fd Socket file descriptor to which the data will be sent.
     * @param data Data buffer containing the bytes to send.
     * @param size Number of bytes from the data buffer to send.
     * @return true if the data is sent successfully, false otherwise.
     */
    bool send_blocking(
        int fd,
        const data_t data,
        ssize_t size
    ){ return BlightProtocol::send_blocking(fd, data, size); }

    /**
     * @brief Waits for the specified socket to become ready for sending data.
     *
     * This function delegates to BlightProtocol::wait_for_send to check if the socket associated with
     * the given file descriptor is ready to send data within the specified timeout.
     *
     * @param fd The file descriptor of the socket.
     * @param timeout Maximum time in milliseconds to wait for readiness.
     * @return bool True if the socket is ready for sending, false otherwise.
     */
    bool wait_for_send(int fd, int timeout){
        return BlightProtocol::wait_for_send(fd, timeout);
    }

    /**
     * @brief Waits for a socket to become ready for reading.
     *
     * This function checks whether the socket identified by the given file descriptor is ready for reading within the
     * specified timeout. It delegates the operation to BlightProtocol::wait_for_read.
     *
     * @param fd The socket file descriptor.
     * @param timeout The maximum time to wait for the socket to become ready (timeout units as defined by BlightProtocol).
     * @return true if the socket is ready for reading; false otherwise.
     */
    bool wait_for_read(int fd, int timeout){
        return BlightProtocol::wait_for_read(fd, timeout);
    }
}
