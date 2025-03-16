#include "socket.h"
#include "_debug.h"
#include <sys/socket.h>
#include <sys/poll.h>

/**
 * @brief Pauses execution for a brief duration.
 *
 * This function suspends the calling thread for 5 microseconds using nanosleep.
 */
void short_pause(){
    timespec remaining;
    timespec requested{
        .tv_sec = 0,
        .tv_nsec = 5000
    };
    nanosleep(&requested, &remaining);
}

namespace BlightProtocol{
    /**
     * @brief Receives a fixed amount of data from a socket.
     *
     * This function attempts to read exactly `size` bytes from the socket identified by `fd`.
     * It performs non-blocking reads with a retry mechanism, using up to `attempts` attempts and a specified
     * `timeout` for each wait. In case of a partial read, it switches to a blocking read to retrieve the remainder
     * of the data. If the function fails to obtain the expected number of bytes, it returns an empty optional
     * and sets `errno` appropriately (EAGAIN if too many retries, or EBADMSG if the received byte count is incorrect).
     *
     * @param fd The socket file descriptor to read from.
     * @param size The total number of bytes expected.
     * @param attempts The maximum number of non-blocking read attempts for handling temporary errors.
     * @param timeout The time to wait for the socket to become ready for reading.
     * @return std::optional<blight_data_t> A pointer to the received data if successful; otherwise, an empty optional.
     */
    std::optional<blight_data_t> recv(
      int fd,
      ssize_t size,
      unsigned int attempts,
      unsigned int timeout
    ){
        auto data = new unsigned char[size];
        ssize_t res = -1;
        unsigned int count = 0;
        while(res < 0 && count < attempts){
            if(!wait_for_read(fd, timeout)){
                if(errno == EAGAIN || errno == EINTR){
                    count++;
                    continue;
                }
                delete[] data;
                return {};
            }
            // Recieve the data
            res = ::recv(fd, data, size, MSG_DONTWAIT);
            if(res > -1){
                break;
            }
            // We got an unexpected error
            if(errno != EAGAIN && errno != EINTR){
                delete[] data;
                return {};
            }
            // Temporary error, try again
            count++;
        }
        // We retried too many times
        if(count == attempts){
            delete[] data;
            errno = EAGAIN;
            return {};
        }
        // We started getting data, but didn't get all of it, try getting some more
        if(res && res < size){
            _WARN("Partial read (%d), waiting for the rest of the data (%d)", res, size - res);
            // TODO this should be in the main loop
            auto maybe = recv_blocking(fd, size - res);
            if(maybe.has_value()){
                memcpy(&data[res], maybe.value(), size - res);
                delete[] maybe.value();
                return data;
            }
        }
        // The data we recieved isn't the same size as what we expected
        if(res != size){
            _WARN("recv %d != %d", size, res);
            delete[] data;
            errno = EBADMSG;
            return {};
        }
        return data;
    }

    /**
     * @brief Receives an exact number of bytes from a socket in a blocking manner.
     *
     * This function attempts to read exactly `size` bytes from the socket identified by `fd`. It repeatedly calls
     * the system `recv` with the MSG_WAITALL flag until either the requested amount is received, the connection is closed,
     * or an unexpected error occurs. Temporary errors (EAGAIN, EINTR) are handled by pausing briefly and retrying.
     * If the total number of bytes received does not match the expected size, a warning is logged, the allocated buffer is
     * deallocated, errno is set to EBADMSG, and an empty optional is returned.
     *
     * @param fd The file descriptor of the socket to read from.
     * @param size The exact number of bytes to receive.
     * @return std::optional<blight_data_t> A non-empty optional containing the received data on success, or an empty optional
     *         if the connection was closed or an unrecoverable error occurred.
     */
    std::optional<blight_data_t> recv_blocking(int fd, ssize_t size){
        auto data = new unsigned char[size];
        ssize_t res = -1;
        ssize_t total = 0;
        while(total < size){
            res = ::recv(fd, data, size, MSG_WAITALL);
            // connection was closed
            if(res == 0){
                break;
            }
            // Something was recieved
            if(res > -1){
                total += res;
                continue;
            }
            // We had an unexpected error
            if(errno != EAGAIN && errno != EINTR){
                delete[] data;
                return {};
            }
            // Temporary error, try again
            short_pause();
        }
        // The data we recieved isn't the same size as what we expected
        if(total != size){
            _WARN("recv_blocking %d != %d", size, res);
            delete[] data;
            errno = EBADMSG;
            return {};
        }
        return data;
    }

    /**
     * @brief Sends the entire buffer over a socket in a blocking manner.
     *
     * This function repeatedly calls the underlying send() until the full buffer
     * (of the specified size) has been transmitted or a non-recoverable error occurs.
     * It handles temporary errors (EAGAIN and EINTR) by pausing briefly and retrying.
     * If the total bytes sent do not equal the expected size, a warning is logged,
     * errno is set to EMSGSIZE, and the function returns false.
     *
     * @param fd Socket file descriptor.
     * @param data Pointer to the data buffer to send.
     * @param size Number of bytes to send.
     * @return true if all bytes are sent successfully; false otherwise.
     */
    bool send_blocking(int fd, const blight_data_t data, ssize_t size){
        // TODO explore MSG_ZEROCOPY, this will require owning the buffer instead of allowing
        //      the user to pass in one we wont touch. As we'll need to ensure we don't delete
        //      it until the kernel tells us it's done using it.
        ssize_t total = 0;
        while(total < size){
            ssize_t res = ::send(fd, data + total, size - total, MSG_EOR | MSG_NOSIGNAL);
            // We sent something
            if(res > -1){
                total += res;
                continue;
            }
            // We had an unexpected error
            if(errno != EAGAIN && errno != EINTR){
                return false;
            }
            // Temporary error, try again
            short_pause();
        }
        // The data we sent isn't the same size as what we expected
        if(total != size){
            _WARN("send_blocking %d != %d", size, total);
            errno = EMSGSIZE;
            return false;
        }
        return true;
    }
}
/**
 * @brief Waits for a specified event on a file descriptor within a given timeout.
 *
 * This function uses poll to monitor the provided file descriptor until the desired event 
 * (e.g., POLLIN for reading or POLLOUT for writing) is triggered. It retries on temporary 
 * errors (EAGAIN or EINTR) by recalculating the remaining time. If the timeout expires, 
 * errno is set to EAGAIN. If the socket disconnects (detected by POLLHUP), errno is set to ECONNRESET.
 *
 * @param fd The file descriptor to be monitored.
 * @param timeout Maximum time to wait in milliseconds.
 * @param event The event flag to wait for (e.g., POLLIN or POLLOUT).
 * @return true if the specified event is triggered before the timeout expires; false otherwise.
 */
bool wait_for(int fd, int timeout, int event){
    int remaining = timeout;
    auto start = std::chrono::steady_clock::now();
    while(true){
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = event;
        auto res = poll(&pfd, 1, remaining);
        // We timed out
        if(res == 0){
            errno = EAGAIN;
            return false;
        }
        if(res < 0){
            // Temporary error, try again
            if(errno != EAGAIN && errno != EINTR){
                return false;
            }
            if(timeout > 0){
                remaining = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                ).count();
                // We timed out
                if(remaining <= 0){
                    return false;
                }
            }
            continue;
        }
        // Socket disconnected
        if(pfd.revents & POLLHUP){
            errno = ECONNRESET;
            return false;
        }
        // Event triggered
        if(pfd.revents & event){
            return true;
        }
        // This should never happen, but just in case try again
    }
}
namespace BlightProtocol{
    /**
 * @brief Waits until the socket is ready for sending data.
 *
 * Checks if the socket identified by the given file descriptor is ready for sending by monitoring for the POLLOUT event.
 * The function uses polling to wait for the specified timeout duration (in milliseconds) before returning.
 *
 * @param fd File descriptor of the socket to be monitored.
 * @param timeout Maximum time to wait (in milliseconds) for the socket to become ready.
 * @return true if the socket is ready for sending data before the timeout expires, false otherwise.
 *
 * @note This function is a thin wrapper around the generic wait_for function configured to check for POLLOUT.
 */
bool wait_for_send(int fd, int timeout){ return wait_for(fd, timeout, POLLOUT); }

    /**
 * @brief Checks if the specified socket is ready for reading.
 *
 * This function invokes the underlying wait_for function with the POLLIN event to determine if the
 * socket associated with the given file descriptor is ready for input within the specified timeout period.
 *
 * @param fd The file descriptor of the socket.
 * @param timeout Maximum wait time in milliseconds.
 * @return true if the socket is ready for reading; false otherwise (e.g., on timeout or error).
 */
bool wait_for_read(int fd, int timeout){ return wait_for(fd, timeout, POLLIN); }
}
