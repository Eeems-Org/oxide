#include "socket.h"
#include <libblight_protocol/socket.h>

namespace Blight{
    /**
     * @brief Receives data from a socket.
     *
     * This function attempts to retrieve a specified amount of data from the socket identified by 
     * the given file descriptor. It delegates the operation to BlightProtocol::recv, allowing for a 
     * configurable number of attempts and a timeout period between attempts. If the operation succeeds, 
     * the function returns an optional containing the received data; otherwise, it returns an empty optional.
     *
     * @param fd The socket file descriptor.
     * @param size The number of bytes to receive.
     * @param attempts The number of attempts to try receiving data.
     * @param timeout The maximum time (in milliseconds) to wait for each reception attempt.
     * @return std::optional<data_t> An optional containing the received data on success, or empty if the operation fails.
     */
    std::optional<data_t> recv(
        int fd,
        ssize_t size,
        unsigned int attempts,
        unsigned int timeout
    ){ return BlightProtocol::recv(fd, size, attempts, timeout); }

    /**
     * @brief Receives a specified amount of data from a socket in a blocking manner.
     *
     * This function delegates to BlightProtocol::recv_blocking to attempt reading exactly
     * `size` bytes from the socket identified by `fd`. If the receive operation succeeds,
     * the retrieved data is returned encapsulated in an std::optional; otherwise, an empty
     * optional is returned.
     *
     * @param fd Socket file descriptor.
     * @param size Number of bytes to receive.
     * @return std::optional<data_t> An optional containing the received data if successful, or empty on failure.
     */
    std::optional<data_t> recv_blocking(
        int fd,
        ssize_t size
    ){ return BlightProtocol::recv_blocking(fd, size); }

    /**
     * @brief Sends data over a socket in a blocking manner.
     *
     * This function delegates to BlightProtocol::send_blocking to transmit the specified data buffer
     * of a given size to the socket represented by the file descriptor. It waits until the send
     * operation completes, returning true if the data is successfully sent.
     *
     * @param fd Socket file descriptor.
     * @param data Data to be transmitted.
     * @param size Number of bytes from the data to send.
     * @return bool True if the data was sent successfully, otherwise false.
     */
    bool send_blocking(
        int fd,
        const data_t data,
        ssize_t size
    ){ return BlightProtocol::send_blocking(fd, data, size); }

    /**
     * @brief Waits until the socket is ready for sending data.
     *
     * Delegates to BlightProtocol::wait_for_send, checking if the socket corresponding
     * to the given file descriptor is prepared to send data within the specified timeout.
     *
     * @param fd The file descriptor of the socket.
     * @param timeout The maximum time to wait in milliseconds.
     * @return true if the socket is ready for sending data; false otherwise.
     */
    bool wait_for_send(int fd, int timeout){
        return BlightProtocol::wait_for_send(fd, timeout);
    }

    /**
     * @brief Waits for the specified socket to be ready for reading.
     *
     * This function delegates to BlightProtocol::wait_for_read to determine if the socket
     * identified by the given file descriptor is ready for reading within the specified timeout period.
     *
     * @param fd Socket file descriptor.
     * @param timeout Maximum time to wait for the socket to be ready (in milliseconds).
     * @return bool True if the socket is ready for reading; false otherwise.
     */
    bool wait_for_read(int fd, int timeout){
        return BlightProtocol::wait_for_read(fd, timeout);
    }
}
