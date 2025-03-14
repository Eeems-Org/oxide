#include "socket.h"
#include "_debug.h"
#include <sys/socket.h>
#include <sys/poll.h>

void short_pause(){
    timespec remaining;
    timespec requested{
        .tv_sec = 0,
        .tv_nsec = 5000
    };
    nanosleep(&requested, &remaining);
}

namespace BlightProtocol{
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
            res = ::recv(fd, data, size, MSG_WAITALL | MSG_DONTWAIT);
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

    std::optional<blight_data_t> recv_blocking(int fd, ssize_t size){
        auto data = new unsigned char[size];
        ssize_t res = -1;
        while(res < 0){
            res = ::recv(fd, data, size, MSG_WAITALL);
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
            _WARN("recv_blocking %d != %d", size, res);
            delete[] data;
            errno = EBADMSG;
            return {};
        }
        return data;
    }

    bool send_blocking(int fd, const blight_data_t data, ssize_t size){
        // TODO explore MSG_ZEROCOPY, this will require owning the buffer instead of allowing
        //      the user to pass in one we wont touch. As we'll need to ensure we don't delete
        //      it until the kernel tells us it's done using it.
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
            _WARN("send_blocking %d != %d", size, res);
            errno = EMSGSIZE;
            return false;
        }
        return true;
    }
}
bool wait_for(int fd, int timeout, int event){
    while(true){
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = event;
        auto res = poll(&pfd, 1, timeout);
        if(res < 0){
            // Temporary error, try again
            // TODO keep track of timeout and try again with only remaining time
            if(errno == EAGAIN || errno == EINTR){
                continue;
            }
            return false;
        }
        // We timed out
        if(res == 0){
            errno = EAGAIN;
            return false;
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
    bool wait_for_send(int fd, int timeout){ return wait_for(fd, timeout, POLLOUT); }

    bool wait_for_read(int fd, int timeout){ return wait_for(fd, timeout, POLLIN); }
}
