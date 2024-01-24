#include "socket.h"
#include "debug.h"

std::optional<Blight::data_t> Blight::recv(
    int fd,
    ssize_t size,
    int attempts,
    int timeout
){
    auto data = new unsigned char[size];
    ssize_t res = -1;
    int count = 0;
    if(attempts < 0){
        count = attempts - 1;
    }
    while(res < 0 && count < attempts){
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        res = poll(&pfd, 1, timeout);
        if(res < 0){
            if(errno == EAGAIN || errno == EINTR){
                if(attempts > 0){
                    count++;
                }
                continue;
            }
            break;
        }
        if(res == 0){
            if(attempts > 0){
                count++;
            }
            continue;
        }
        if(pfd.revents & POLLHUP){
            errno = ECONNRESET;
            delete[] data;
            return {};
        }
        if(!(pfd.revents & POLLOUT)){
            res = 0;
            if(attempts > 0){
                count++;
            }
            continue;
        }
        res = ::recv(fd, data, size, 0);
        if(res > 0){
            break;
        }
        if(errno == EAGAIN || errno == EINTR){
            if(attempts > 0){
                count++;
            }
            continue;
        }
        return {};
    }
    if(attempts > 0 && count == attempts){
        delete[] data;
        errno = EAGAIN;
        return {};
    }
    if(res < 0){
        delete[] data;
        return {};
    }
    if(res != size){
        _WARN("recv %d != %d", res, size);
        delete[] data;
        errno = EBADMSG;
        return {};
    }
    return data;
}

bool Blight::send(
    int fd,
    data_t data,
    ssize_t size,
    int attempts,
    int timeout
){
    int res = -1;
    int count = 0;
    if(attempts < 0){
        count = attempts - 1;
    }
    while(res < 1 && count < attempts){
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        res = poll(&pfd, 1, timeout);
        if(res < 0){
            if(errno == EAGAIN || errno == EINTR){
                if(attempts > 0){
                    count++;
                }
                continue;
            }
            break;
        }
        if(res == 0){
            count++;
            continue;
        }
        if(pfd.revents & POLLHUP){
            errno = ECONNRESET;
            res = -1;
            break;
        }
        if(!(pfd.revents & POLLOUT)){
            res = 0;
            if(attempts > 0){
                count++;
            }
            continue;
        }
        res = ::send(fd, &data, size, MSG_EOR);
        if(res >= 0){
            break;
        }
        if(errno == EAGAIN || errno == EINTR){
            if(attempts > 0){
                count++;
            }
            continue;
        }
        break;
    }
    if(attempts > 0 && count == attempts){
        errno = EAGAIN;
        return false;
    }
    if(res < 0){
        return false;
    }
    if(res != size){
        _WARN("send %d != %d", res, size);
        errno = EBADMSG;
        return false;
    }
    return true;
}

bool Blight::send_blocking(int fd, data_t data, ssize_t size){
    int res = -1;
    while(res < 0){
        res = ::send(fd, data, size, MSG_EOR);
        if(res > -1){
            break;
        }
        if(errno == EAGAIN || errno == EINTR){
            timespec remaining;
            timespec requested{
                .tv_sec = 0,
                .tv_nsec = 5000
            };
            nanosleep(&requested, &remaining);
            continue;
        }
        break;
    }
    if(res < 0){
        return {};
    }
    if(res != size){
        errno = EMSGSIZE;
        return false;
    }
    return true;
}
