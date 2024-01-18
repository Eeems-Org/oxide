#pragma once

namespace Blight{
    enum Format{
        Format_Invalid,
        Format_Mono,
        Format_MonoLSB,
        Format_Indexed8,
        Format_RGB32,
        Format_ARGB32,
        Format_ARGB32_Premultiplied,
        Format_RGB16,
        Format_ARGB8565_Premultiplied,
        Format_RGB666,
        Format_ARGB6666_Premultiplied,
        Format_RGB555,
        Format_ARGB8555_Premultiplied,
        Format_RGB888,
        Format_RGB444,
        Format_ARGB4444_Premultiplied,
        Format_RGBX8888,
        Format_RGBA8888,
        Format_RGBA8888_Premultiplied,
        Format_BGR30,
        Format_A2BGR30_Premultiplied,
        Format_RGB30,
        Format_A2RGB30_Premultiplied,
        Format_Alpha8,
        Format_Grayscale8,
        Format_RGBX64,
        Format_RGBA64,
        Format_RGBA64_Premultiplied,
        Format_Grayscale16,
        Format_BGR888,
    };
    typedef unsigned char* data_t;
    typedef struct buf_t{
        int fd;
        int x;
        int y;
        int width;
        int height;
        int stride;
        Format format;
        data_t data;
        std::string uuid;
        size_t size();
        int close();
        ~buf_t();
    } buf_t;
    typedef struct repaint_t{
        int x;
        int y;
        int width;
        int height;
    } repaint_t;
}
