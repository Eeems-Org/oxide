#include "libblight_global.h"
#include "types.h"
#include <optional>

namespace Blight{
    LIBBLIGHT_EXPORT bool connect(bool use_system = true);
    LIBBLIGHT_EXPORT bool exists();
    LIBBLIGHT_EXPORT int open();
    LIBBLIGHT_EXPORT std::optional<shared_buf_t> createBuffer(
        int x,
        int y,
        int width,
        int height,
        int stride,
        Format format
    );
    LIBBLIGHT_EXPORT std::optional<std::string> addSurface(
        int fd,
        int x,
        int y,
        int width,
        int height,
        int stride,
        Format format
    );
    LIBBLIGHT_EXPORT inline void addSurface(shared_buf_t buf){
        buf->surface = addSurface(
            buf->fd,
            buf->x,
            buf->y,
            buf->width,
            buf->height,
            buf->stride,
            buf->format
        ).value_or("");
    }
    LIBBLIGHT_EXPORT int repaint(std::string identifier);
    LIBBLIGHT_EXPORT int getSurface(std::string identifier);
}
