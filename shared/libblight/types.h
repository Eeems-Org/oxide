#pragma once
#include "libblight_global.h"
#include <linux/input.h>
#include <string>
#include <memory>
#include <vector>
#include <optional>

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
    enum Waveform {
        INIT = 0,
        DU = 1,
        GC16 = 2,
        GL16 = 3,
        GLR16 = 4,
        GLD16 = 5,
        A2 = 6,
        DU4 = 7,
        UNKNOWN = 8,
        INIT2 = 9
    };
    enum WaveformMode{
        Initialize = Waveform::INIT,
        Mono = Waveform::DU,
        Grayscale = Waveform::GL16,
        HighQualityGrayscale = Waveform::GC16,
        Highlight = Waveform::UNKNOWN
    };

    LIBBLIGHT_EXPORT typedef struct {
        unsigned int device;
        input_event event;
    } event_packet_t;
    LIBBLIGHT_EXPORT typedef unsigned char* data_t;
    LIBBLIGHT_EXPORT typedef std::shared_ptr<unsigned char[]> shared_data_t;
    struct buf_t;
    LIBBLIGHT_EXPORT typedef std::shared_ptr<buf_t> shared_buf_t;
    LIBBLIGHT_EXPORT typedef struct clipboard_t {
        shared_data_t data;
        size_t size;
        const std::string name;
        const std::string to_string();
        bool update();
        bool set(shared_data_t data, size_t size);
        inline bool set(const char* data, size_t size){
            auto buf = new unsigned char[size];
            memcpy(buf, data, size);
            return set(shared_data_t(buf), size);
        }
        inline bool set(const data_t data, size_t size){ return set((char*)data, size); }
        inline bool set(const std::string& data){ return set(data.data(), data.size()); }
    } clipboard_t;
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
        std::string surface;
        size_t size();
        int close();
        ~buf_t();
        std::optional<shared_buf_t> clone();
        static shared_buf_t new_ptr();
        static std::string new_uuid();
    } buf_t;
    enum MessageType{
        Invalid,
        Ack,
        Repaint,
        Move,
        Info,
        Delete,
        List
    };
    LIBBLIGHT_EXPORT typedef struct header_t{
        MessageType type;
        unsigned int ackid;
        size_t size;
        static header_t from_data(data_t data);
        static header_t from_data(char* data);
        static header_t from_data(void* data);
        static header_t new_invalid();
    } header_t;
    struct message_t;
    LIBBLIGHT_EXPORT typedef std::shared_ptr<message_t> message_ptr_t;
    LIBBLIGHT_EXPORT typedef struct message_t{
        header_t header;
        shared_data_t data;
        static message_t from_data(data_t _data);
        static message_t from_data(char* data);
        static message_t from_data(void* data);
        static data_t create_ack(message_t* message, size_t size = 0);
        static data_t create_ack(const message_t& message, size_t size = 0);
        static message_ptr_t from_socket(int fd);
        static message_ptr_t new_ptr();
    } message_t;
    LIBBLIGHT_EXPORT typedef struct repaint_header_t{
        int x;
        int y;
        int width;
        int height;
        WaveformMode waveform;
        size_t identifier_len;
        static repaint_header_t from_data(data_t data);
    } repaint_header_t;
    LIBBLIGHT_EXPORT typedef struct repaint_t{
        repaint_header_t header;
        std::string identifier;
        static repaint_t from_message(const message_t* message);
    } repaint_t;
    LIBBLIGHT_EXPORT typedef struct move_header_t{
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
    LIBBLIGHT_EXPORT typedef struct surface_info_t{
        int x;
        int y;
        int width;
        int height;
        int stride;
        Format format;
        static surface_info_t from_data(data_t data);
    } surface_info_t;
    LIBBLIGHT_EXPORT typedef struct list_header_t{
        unsigned int count;
        static list_header_t from_data(data_t data);
    } list_header_t;
    LIBBLIGHT_EXPORT typedef struct list_item_t{
        size_t identifier_len;
        std::string identifier;
        static list_item_t from_data(data_t data);
    } list_item_t;
    LIBBLIGHT_EXPORT typedef struct list_t{
        list_header_t header;
        std::vector<list_item_t> items;
        std::vector<std::string> identifiers();
        static list_t from_data(data_t data);
    } list_t;
}
