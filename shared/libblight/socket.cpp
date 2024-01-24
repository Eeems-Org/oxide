#include "socket.h"
#include "debug.h"

void short_pause(){
    timespec remaining;
    timespec requested{
        .tv_sec = 0,
        .tv_nsec = 5000
    };
    nanosleep(&requested, &remaining);
}

std::optional<Blight::data_t> Blight::recv(
    int fd,
    ssize_t size,
    unsigned int attempts,
    unsigned int timeout
){
    auto data = new unsigned char[size];
    ssize_t res = -1;
    unsigned int count = 0;
    while(res < 0 && count < attempts){
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        res = poll(&pfd, 1, timeout);
        if(res < 0){
            // Temporary error, try again
            if(errno == EAGAIN || errno == EINTR){
                count++;
                continue;
            }
            delete[] data;
            return {};
        }
        // We timed out, try again
        if(res == 0){
            count++;
            continue;
        }
        // Socket disconnected
        if(pfd.revents & POLLHUP){
            errno = ECONNRESET;
            delete[] data;
            return {};
        }
        // We only expect POLLOUT this is odd
        if(!(pfd.revents & POLLOUT)){
            res = 0;
            count++;
            continue;
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
    // The data we recieved isn't the same size as what we expected
    if(res != size){
        _WARN("recv %d != %d", res, size);
        delete[] data;
        errno = EBADMSG;
        return {};
    }
    return data;
}

std::optional<Blight::data_t> Blight::recv_blocking(int fd, ssize_t size){
    auto data = new unsigned char[size];
    ssize_t res = -1;
    while(res < 0){
        res = ::recv(fd, data, size, 0);
        // Something was recieved
        if(res > 0){
            break;
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
    if(res != size){
        _WARN("recv %d != %d", res, size);
        delete[] data;
        errno = EBADMSG;
        return {};
    }
    return data;
}

bool Blight::send_blocking(int fd, const data_t data, ssize_t size){
    int res = -1;
    while(res < 0){
        res = ::send(fd, data, size, MSG_EOR | MSG_NOSIGNAL);
        // We sent something
        if(res > -1){
            break;
        }
        // We had an unexpected error
        if(errno != EAGAIN && errno != EINTR){
            return false;
        }
        // Temporary error, try again
        short_pause();
    }
    // The data we sent isn't the same size as what we expected
    if(res != size){
        errno = EMSGSIZE;
        return false;
    }
    return true;
}
