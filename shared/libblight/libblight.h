#include "libblight_global.h"
#include "types.h"

namespace Blight{
    bool LIBBLIGHT_EXPORT connect(bool use_system = true);
    bool LIBBLIGHT_EXPORT exists();
    int LIBBLIGHT_EXPORT open();
    buf_t* createBuffer(int x, int y, int width, int height, int stride, Format format);
    std::string LIBBLIGHT_EXPORT addSurface(
        int fd,
        int x,
        int y,
        int width,
        int height,
        int stride,
        Format format
    );
    inline std::string LIBBLIGHT_EXPORT addSurface(buf_t buf){
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
    int repaint(std::string identifier);
}
