#include "libblight_global.h"
#include "types.h"

namespace Blight{
    LIBBLIGHT_EXPORT bool connect(bool use_system = true);
    LIBBLIGHT_EXPORT bool exists();
    LIBBLIGHT_EXPORT int open();
    LIBBLIGHT_EXPORT shared_buf_t createBuffer(
        int x,
        int y,
        int width,
        int height,
        int stride,
        Format format
    );
    LIBBLIGHT_EXPORT std::string LIBBLIGHT_EXPORT addSurface(
        int fd,
        int x,
        int y,
        int width,
        int height,
        int stride,
        Format format
    );
    LIBBLIGHT_EXPORT inline std::string addSurface(buf_t buf){
        return addSurface(
            buf.fd,
            buf.x,
            buf.y,
            buf.width,
            buf.height,
            buf.stride,
            buf.format
        );
    }
    LIBBLIGHT_EXPORT int repaint(std::string identifier);
    LIBBLIGHT_EXPORT int getSurface(std::string identifier);
    LIBBLIGHT_EXPORT std::vector<std::string> surfaces();
}
