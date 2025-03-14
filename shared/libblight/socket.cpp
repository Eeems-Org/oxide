#include "socket.h"
#include <libblight_protocol/socket.h>

namespace Blight{
    std::optional<data_t> recv(
        int fd,
        ssize_t size,
        unsigned int attempts,
        unsigned int timeout
    ){ return BlightProtocol::recv(fd, size, attempts, timeout); }

    std::optional<data_t> recv_blocking(
        int fd,
        ssize_t size
    ){ return BlightProtocol::recv_blocking(fd, size); }

    bool send_blocking(
        int fd,
        const data_t data,
        ssize_t size
    ){ return BlightProtocol::send_blocking(fd, data, size); }

    bool wait_for_send(int fd, int timeout){
        return BlightProtocol::wait_for_send(fd, timeout);
    }

    bool wait_for_read(int fd, int timeout){
        return BlightProtocol::wait_for_read(fd, timeout);
    }
}
