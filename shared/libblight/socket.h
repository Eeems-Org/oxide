#pragma once
#include "libblight_global.h"

#include "types.h"


namespace Blight{
    LIBBLIGHT_EXPORT std::optional<data_t> recv(
        int fd,
        ssize_t size,
        unsigned int attempts = 5,
        unsigned int timeout = 50
    );
    LIBBLIGHT_EXPORT std::optional<data_t> recv_blocking(
        int fd,
        ssize_t size
    );
    LIBBLIGHT_EXPORT bool send(
        int fd,
        const data_t data,
        ssize_t size,
        unsigned int attempts = 5,
        unsigned int timeout = 50
    );
    LIBBLIGHT_EXPORT bool send_blocking(
        int fd,
        const data_t data,
        ssize_t size
    );
}
