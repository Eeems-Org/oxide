#pragma once
#include "libblight_global.h"
#include <string>

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
    LIBBLIGHT_EXPORT typedef unsigned char* data_t;
    LIBBLIGHT_EXPORT typedef struct buf_t{
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
    enum MessageType{
        Invalid,
        Ack,
        Repaint,
        Move,
    };
    LIBBLIGHT_EXPORT typedef struct header_t{
        MessageType type;
        unsigned int ackid;
        size_t size;
        static header_t from_data(data_t data);
        static header_t from_data(char* data);
        static header_t from_data(void* data);
    } header_t;
    LIBBLIGHT_EXPORT typedef struct message_t{
        header_t header;
        data_t data;
        static message_t from_data(data_t _data);
        static message_t from_data(char* data);
        static message_t from_data(void* data);
        static data_t create_ack(message_t* message);
        static data_t create_ack(const message_t& message);
        static message_t* from_socket(int fd);
    } message_t;
    LIBBLIGHT_EXPORT typedef struct repaint_header_t{
        int x;
        int y;
        int width;
        int height;
        unsigned int marker;
        size_t identifier_len;
        static repaint_header_t from_data(data_t data);
    } repaint_header_t;
    LIBBLIGHT_EXPORT typedef struct repaint_t{
        repaint_header_t header;
        std::string identifier;
        static repaint_t from_message(const message_t* message);
    } repaint_t;
    LIBBLIGHT_EXPORT typedef struct  move_header_t{
        int x;
        int y;
        size_t identifier_len;
        static move_header_t from_data(data_t data);
    } move_header_t;
    LIBBLIGHT_EXPORT typedef struct move_t{
        move_header_t header;
        std::string identifier;
        static move_t from_message(const message_t* message);
    } move_t;
}
