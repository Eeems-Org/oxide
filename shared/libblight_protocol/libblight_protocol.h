/*!
 * \addtogroup BlightProtocol
 * @{
 * \file
 */
#pragma once

#include "libblight_protocol_global.h"

#include <linux/types.h>
#include <systemd/sd-bus.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#include "fbgraphics.h"

#ifdef __cplusplus
namespace BlightProtocol {
#endif
    /*!
     * \brief Name of the DBus service that the display server is available at.
     */
    #define BLIGHT_SERVICE "codes.eeems.blight1"
    /*!
     * \brief Name of the DBus interface that the display server is available at.
     */
    #define BLIGHT_INTERFACE BLIGHT_SERVICE ".Compositor"

    /*!
     * \brief Image format of a buffer
     */
    typedef enum {
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
    } BlightImageFormat;

    /*!
     * \brief Possible waveforms
     */
    typedef enum {
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
    } BlightWaveform;

    /*!
     * \brief Waveform to use for a repaint
     */
    typedef enum{
#ifdef __cplusplus
        Initialize = BlightWaveform::INIT,
        Mono = BlightWaveform::DU,
        Grayscale = BlightWaveform::GL16,
        HighQualityGrayscale = BlightWaveform::GC16,
        Highlight = BlightWaveform::UNKNOWN
#else
        Initialize = 0,
        Mono = 1,
        Grayscale = 3,
        HighQualityGrayscale = 2,
        Highlight = 8
#endif
    } BlightWaveformMode;

    /*!
     * \brief Size type used by the protocol
     */
    typedef uint32_t blight_size_t;
    /*!
     * \brief Surface identifier
     */
    typedef unsigned short blight_surface_id_t;

    /*!
     * \brief Partial input event
     */
    typedef struct{
        /*!
         * \brief Input event type
         */
        __u16 type;
        /*!
         * \brief Input event code
         */
        __u16 code;
        /*!
         * \brief Input event value
         */
        __s32 value;
    } blight_partial_input_event_t;

    /*!
     * \brief Input event packet
     */
    typedef struct {
        /*!
         * \brief Device that this packet is for
         */
        unsigned int device;
        /*!
         * \brief Partial input event
         */
        union{ blight_partial_input_event_t event; };
    } blight_event_packet_t;
    /*!
     * \brief Message type
     */
    typedef enum {
        Invalid,
        Ack,
        Ping,
        Repaint,
        Move,
        Info,
        Delete,
        List,
        Raise,
        Lower,
        Wait,
        Focus
    } BlightMessageType;
    /*!
     * \brief Generic data pointer
     */
    typedef unsigned char* blight_data_t;
    /*!
     * \brief
     */
    typedef struct {
        int fd;
        int x;
        int y;
        int width;
        int height;
        int stride;
        BlightImageFormat format;
        blight_data_t data;
    } blight_buf_t;
    /*!
     * \brief Message header
     */
    typedef struct blight_header_t{
        /*!
         * \brief Message type
         */
        BlightMessageType type;
        /*!
         * \brief Unique identifier for this message
         */
        unsigned int ackid;
        /*!
         * \brief Size of data
         */
        uint32_t size;
    } blight_header_t;
    /*!
     * \brief Message object
     */
    typedef struct blight_message_t{
        /*!
         * \brief Message header
         */
        blight_header_t header;
        /*!
         * \brief Message data
         */
        blight_data_t data;
    } blight_message_t;
    /*!
     * \brief Repaint message data
     */
    typedef struct blight_packet_repaint_t{
        /*!
         * \brief x X offset
         */
        int x;
        /*!
         * \brief y Y offset
         */
        int y;
        /*!
         * \brief width Width
         */
        int width;
        /*!
         * \brief height Height
         */
        int height;
        /*!
         * \brief waveform Waveform to use
         */
        BlightWaveformMode waveform;
        /*!
         * \brief marker Marker to use
         */
        unsigned int marker;
        /*!
         * \brief identifier Surface identifier
         */
        blight_surface_id_t identifier;
    } blight_packet_repaint_t;
    /*!
     * \brief Move message data
     */
    typedef struct blight_packet_move_t{
        /*!
         * \brief identifier Surface identifier
         */
        blight_surface_id_t identifier;
        /*!
         * \brief x X coordinate
         */
        int x;
        /*!
         * \brief y Y coordinate
         */
        int y;
    } blight_packet_move_t;
    /*!
     * \brief Surface information message data
     */
    typedef struct blight_packet_surface_info_t{
        /*!
         * \brief x X coordinate
         */
        int x;
        /*!
         * \brief y Y coordiante
         */
        int y;
        /*!
         * \brief width Width
         */
        int width;
        /*!
         * \brief height Height
         */
        int height;
        /*!
         * \brief stride Bytes per line
         */
        int stride;
        /*!
         * \brief format Image format
         */
        BlightImageFormat format;
    } blight_packet_surface_info_t;
#ifdef __cplusplus
}
#define blight_data_t BlightProtocol::blight_data_t
#define blight_message_t BlightProtocol::blight_message_t
#define blight_header_t BlightProtocol::blight_header_t
#define blight_surface_id_t BlightProtocol::blight_surface_id_t
#define blight_buf_t BlightProtocol::blight_buf_t
#define blight_packet_repaint_t BlightProtocol::blight_packet_repaint_t
#define blight_packet_move_t BlightProtocol::blight_packet_move_t
#define blight_packet_surface_info_t BlightProtocol::blight_packet_surface_info_t
#define blight_event_packet_t BlightProtocol::blight_event_packet_t
#define BlightMessageType BlightProtocol::BlightMessageType
#define BlightImageFormat BlightProtocol::BlightImageFormat
extern "C" {
#endif
    typedef sd_bus blight_bus;
    /*!
     * \brief blight_dbus_connect_system
     * \param bus
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_bus_connect_system(blight_bus** bus);
    /*!
     * \brief blight_dbus_connect_user
     * \param bus
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_bus_connect_user(blight_bus** bus);
    /*!
     * \brief blight_bus_deref
     * \param bus
     */
    LIBBLIGHT_PROTOCOL_EXPORT void blight_bus_deref(blight_bus* bus);
    /*!
     * \brief blight_service_available
     * \param bus
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT bool blight_service_available(blight_bus* bus);
    /*!
     * \brief blight_service_open
     * \param bus
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_service_open(blight_bus* bus);
    /*!
     * \brief blight_service_input_open
     * \param bus
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_service_input_open(blight_bus* bus);
    /*!
     * \brief blight_header_from_data
     * \param data
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_header_t blight_header_from_data(blight_data_t data);
    /*!
     * \brief blight_message_from_data
     * \param data
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_message_t* blight_message_from_data(blight_data_t data);
    /*!
     * \brief blight_message_from_socket
     * \param fd
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_message_from_socket(
        int fd,
        blight_message_t** message
    );
    /*!
     * \brief blight_message_deref
     * \param message
     */
    LIBBLIGHT_PROTOCOL_EXPORT void blight_message_deref(blight_message_t* message);
    /*!
     * \brief blight_send_message
     * \param fd
     * \param type
     * \param size
     * \param data
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_send_message(
        int fd,
        BlightMessageType type,
        unsigned int ackid,
        uint32_t size,
        blight_data_t data
    );
    /*!
     * \brief blight_create_buffer
     * \param x
     * \param y
     * \param width
     * \param height
     * \param stride
     * \param format
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_buf_t* blight_create_buffer(
        int x,
        int y,
        int width,
        int height,
        int stride,
        BlightImageFormat format
    );
    /*!
     * \brief blight_buffer_deref
     * \param buf
     */
    LIBBLIGHT_PROTOCOL_EXPORT void blight_buffer_deref(blight_buf_t* buf);
    /*!
     * \brief blight_add_surface
     * \param bus
     * \param buf
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_surface_id_t blight_add_surface(
        blight_bus* bus,
        blight_buf_t* buf
    );
    /*!
     * \brief blight_cast_to_repaint_packet
     * \param message
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_packet_repaint_t* blight_cast_to_repaint_packet(
        blight_message_t* message
    );
    /*!
     * \brief blight_cast_to_move_packet
     * \param message
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_packet_move_t* blight_cast_to_move_packet(
        blight_message_t* message
    );
    /*!
     * \brief blight_cast_to_surface_info_packet
     * \param message
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_packet_surface_info_t* blight_cast_to_surface_info_packet(
        blight_message_t* message
    );
    /*!
     * \brief blight_event_from_socket
     * \param fd
     * \param packet
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_event_from_socket(
        int fd,
        blight_event_packet_t** packet
    );
    /*!
     * \brief blight_surface_to_fbg
     * \param identifier
     * \param buf
     * \return
     */
    LIBBLIGHT_PROTOCOL_EXPORT struct _fbg* blight_surface_to_fbg(
      blight_surface_id_t identifier,
        blight_buf_t* buf
    );

#ifdef __cplusplus
}
#undef blight_data_t
#undef blight_message_t
#undef blight_header_t
#undef blight_surface_id_t
#undef blight_buf_t
#undef blight_packet_repaint_t
#undef blight_packet_move_t
#undef blight_packet_surface_info_t
#undef blight_event_packet_t
#undef BlightMessageType
#undef BlightImageFormat
#endif
/*! @} */
