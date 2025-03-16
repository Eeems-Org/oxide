#include "socket.h"
#include "_debug.h"
#include <sys/socket.h>
#include <sys/poll.h>

/**
 * @brief Pauses execution for 5 microseconds.
 *
 * This function introduces a very short delay using the nanosleep system call. It is useful
 * for scenarios requiring minimal timing gaps between operations.
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
     * @brief Receives an exact amount of data from a socket with retry logic.
     *
     * This function attempts to read exactly @a size bytes of data from the socket identified by @a fd.
     * It performs non-blocking reads while checking for socket readiness with a specified @a timeout, and
     * retries up to @a attempts on temporary errors (EAGAIN or EINTR). In case of a partial read, it calls
     * recv_blocking() to retrieve the remaining data. If the total number of bytes received does not match
     * the expected @a size or an unrecoverable error occurs, allocated resources are cleaned up, errno is set,
     * and an empty optional is returned.
     *
     * @param fd Socket file descriptor.
     * @param size Number of bytes to be received.
     * @param attempts Maximum number of attempts to retry in the event of temporary failures.
     * @param timeout Timeout (in milliseconds) for checking if the socket is ready for reading.
     * @return std::optional<blight_data_t> Pointer to the received data if successful; otherwise, an empty optional.
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
     * @brief Blocks until exactly the specified number of bytes are received from a socket.
     *
     * This function attempts to read `size` bytes from the socket represented by `fd` using a blocking
     * call (with the `MSG_WAITALL` flag). It retries the read in the case of temporary errors (i.e., when
     * errno is set to EAGAIN or EINTR) after a short pause. If the connection closes before the expected
     * amount of data is received or a non-recoverable error occurs, the function cleans up and returns an
     * empty optional. A warning is logged and errno is set (to EBADMSG) if the received data size does not
     * match the requested size.
     *
     * @param fd The socket file descriptor to read from.
     * @param size The exact number of bytes to read.
     * @return std::optional<blight_data_t> A buffer containing the received data if successful, or an empty optional on failure.
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
     * @brief Sends the entire data buffer to a socket in a blocking manner.
     *
     * This function repeatedly invokes the send() system call to transmit all bytes from the provided
     * buffer over the socket specified by the file descriptor. It handles temporary errors (such as EAGAIN
     * and EINTR) by retrying after a brief pause. If an unrecoverable error occurs or if the total bytes
     * sent does not match the expected size, the function sets errno accordingly and returns false.
     *
     * @param fd Socket file descriptor.
     * @param data Pointer to the data buffer to be sent.
     * @param size Number of bytes to send from the buffer.
     * @return true if the entire buffer was sent successfully; false otherwise.
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
 * @brief Waits for a specified event on a file descriptor within a timeout.
 *
 * This function monitors the provided file descriptor for a specific event
 * (e.g., POLLIN or POLLOUT) using the poll system call. It repeatedly checks
 * for the event until it occurs, the timeout period elapses, or a non-recoverable
 * error is encountered.
 *
 * If the poll call times out, errno is set to EAGAIN and the function returns false.
 * In the event of a socket disconnection (POLLHUP), errno is set to ECONNRESET.
 *
 * Temporary errors (EAGAIN or EINTR) are retried until the timeout is reached.
 *
 * @param fd The file descriptor to monitor.
 * @param timeout The maximum time in milliseconds to wait for the event.
 * @param event The event flags to wait for (e.g., POLLIN, POLLOUT).
 * @return true if the event is detected within the timeout; false otherwise.
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
 * This function polls the specified file descriptor until it is ready for sending data or the timeout expires,
 * using the POLLOUT event. It returns true if the socket becomes ready, and false if the timeout is reached or
 * an error occurs.
 *
 * @param fd Socket file descriptor.
 * @param timeout Maximum time to wait (in milliseconds) for the socket to be ready.
 * @return true if the socket is ready for sending; false otherwise.
 */
bool wait_for_send(int fd, int timeout){ return wait_for(fd, timeout, POLLOUT); }

    /**
 * @brief Waits until the specified socket is ready for reading.
 *
 * This function monitors the socket identified by the file descriptor for read readiness
 * within a given timeout period. It is a thin wrapper around the generic wait_for() function,
 * specifying the POLLIN event.
 *
 * @param fd The file descriptor of the socket to monitor.
 * @param timeout Maximum duration to wait (in milliseconds) for the socket to become read-ready.
 * @return true if the socket is ready for reading before the timeout expires, false otherwise.
 */
bool wait_for_read(int fd, int timeout){ return wait_for(fd, timeout, POLLIN); }
}
