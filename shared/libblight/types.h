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
    LIBBLIGHT_EXPORT typedef unsigned int size_t;
    LIBBLIGHT_EXPORT typedef unsigned short surface_id_t;
    LIBBLIGHT_EXPORT typedef struct{
        __u16 type;
        __u16 code;
        __s32 value;
    } partial_input_event_t;
    LIBBLIGHT_EXPORT typedef struct {
        unsigned int device;
        partial_input_event_t event;
    } event_packet_t;
    LIBBLIGHT_EXPORT typedef unsigned char* data_t;
    LIBBLIGHT_EXPORT typedef std::shared_ptr<unsigned char[]> shared_data_t;
    struct buf_t;
    LIBBLIGHT_EXPORT typedef std::shared_ptr<buf_t> shared_buf_t;
    LIBBLIGHT_EXPORT typedef struct clipboard_t {
        shared_data_t data;
        size_t size;
        std::string name;
        clipboard_t(const std::string name, data_t data = nullptr, size_t size = 0);
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
        surface_id_t surface;
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
        Ping,
        Repaint,
        Move,
        Info,
        Delete,
        List,
        Raise,
        Lower
    };
    LIBBLIGHT_EXPORT typedef struct header_t{
        MessageType type;
        unsigned int ackid;
        unsigned long size;
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
        static header_t create_ack(message_t* message, size_t size = 0);
        static header_t create_ack(const message_t& message, size_t size = 0);
        static message_ptr_t from_socket(int fd);
        static message_ptr_t new_ptr();
    } message_t;
    LIBBLIGHT_EXPORT typedef struct repaint_t{
        int x;
        int y;
        int width;
        int height;
        WaveformMode waveform;
        surface_id_t identifier;
        static repaint_t from_message(const message_t* message);
    } repaint_t;
    LIBBLIGHT_EXPORT typedef struct move_t{
        surface_id_t identifier;
        int x;
        int y;
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
}
