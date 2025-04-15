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
#else
extern "C" {
#endif
#ifndef WITHOUT_JPEG
#include "nanojpeg.h"
#endif
#include "fbgraphics.h"
#ifdef __cplusplus
}
#endif

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
     * \brief BlightUpdateMode Update mode for a repaint
     */
    typedef enum{
        PartialUpdate = 0x0,
        FullUpdate = 0x1
    } BlightUpdateMode;

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
     * \sa blight_header_t
     * \sa blight_message_t
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
        unsigned int width;
        unsigned int height;
        unsigned int stride;
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
        unsigned int width;
        /*!
         * \brief height Height
         */
        unsigned int height;
        /*!
         * \brief waveform Waveform to use
         */
        BlightWaveformMode waveform;
        /*!
         * \brief mode Update mode to use
         */
        BlightUpdateMode mode;
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
         * \brief y Y coordinate
         */
        int y;
        /*!
         * \brief width Width
         */
        unsigned int width;
        /*!
         * \brief height Height
         */
        unsigned int height;
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
#define BlightWaveformMode BlightProtocol::BlightWaveformMode
#define BlightUpdateMode BlightProtocol::BlightUpdateMode
extern "C" {
#endif
    typedef sd_bus blight_bus;
    /*!
     * \brief blight_dbus_connect_system Connect to the system dbus
     * \param bus Will populate with a pointer to a bight_bus instance on success
     * \return If the connection was successful
     * \sa blight_bus_deref
     * \sa blight_service_available
     * \sa blight_service_open
     * \sa blight_bus_connect_user
     * \sa blight_service_input_open
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_bus_connect_system(blight_bus** bus);
    /*!
     * \brief blight_dbus_connect_user Connect to the user dbus
     * \param bus Will populate with a pointer to a bight_bus instance on success
     * \return If the connection was successful
     * \sa blight_bus_deref
     * \sa blight_service_available
     * \sa blight_service_open
     * \sa blight_bus_connect_system
     * \sa blight_service_input_open
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_bus_connect_user(blight_bus** bus);
    /*!
     * \brief blight_bus_deref Disconnect from dbus
     * \param bus dbus connection to disconnect
     * \sa blight_bus_connect_system
     * \sa blight_bus_connect_user
     */
    LIBBLIGHT_PROTOCOL_EXPORT void blight_bus_deref(blight_bus* bus);
    /*!
     * \brief blight_service_available Check to see if the blight service is on dbus
     * \param bus The dbus connection
     * \return If the blight service is available
     * \sa blight_bus_connect_system
     * \sa blight_bus_connect_user
     */
    LIBBLIGHT_PROTOCOL_EXPORT bool blight_service_available(blight_bus* bus);
    /*!
     * \brief blight_service_open Open a socket connection to the blight service
     * \param bus The dbus connection
     * \return The file descriptor for the socket connection
     * \sa blight_bus_connect_system
     * \sa blight_bus_connect_user
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_service_open(blight_bus* bus);
    /*!
     * \brief blight_service_input_open Open a socket connection for input events from the blight service
     * \param bus The dbus connection
     * \return The file descriptor for the socket connection
     * \sa blight_bus_connect_system
     * \sa blight_bus_connect_user
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_service_input_open(blight_bus* bus);
    /*!
     * \brief blight_header_from_data Parse a buffer and return the blight_header_t from it
     * \param data Buffer to parse
     * \return Parsed header
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_header_t blight_header_from_data(blight_data_t data);
    /*!
     * \brief blight_message_from_data Parse a buffer and return the blight_message_t from it
     * \param data Buffer to parse
     * \return Parsed message
     * \sa blight_message_deref
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_message_t* blight_message_from_data(blight_data_t data);
    /*!
     * \brief blight_message_from_socket Read a blight_message_t from a socket
     * \param fd File descriptor for the socket
     * \param message Will be populated with a pointer to the blight_message_t on successful parse
     * \return Returns the size of the data, not including the size of the header. Will be a negative number if the read fails
     * \sa blight_service_open
     * \sa blight_message_deref
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_message_from_socket(
        int fd,
        blight_message_t** message
    );
    /*!
     * \brief blight_message_deref Release the memory for a blight_message_t
     * \param message Pointer to the blight_message_t to free
     * \sa blight_message_from_data
     * \sa blight_message_from_socket
     */
    LIBBLIGHT_PROTOCOL_EXPORT void blight_message_deref(blight_message_t* message);
    /*!
     * \brief blight_send_message Send a message to the display server
     * \param fd File descriptor for the socket
     * \param type Message type
     * \param ackid Unique identifier to be passed back when the server acknowledges when this call has been processed.
     * \param size Size of data
     * \param data Pointer to data buffer
     * \param timeout How many milliseconds to wait for a response. Set to a negative number to disable waiting. Set to 0 to wait forever.
     * \param response Response from server
     * \return Size of the data recieved on success, negative on error. Will always be 0 if no thread has been started
     * \sa blight_service_open
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_send_message(
        int fd,
        BlightMessageType type,
        unsigned int ackid,
        uint32_t size,
        blight_data_t data,
        int timeout,
        blight_data_t* response
    );
    /*!
     * \brief blight_create_buffer Create an image buffer for a display surface
     * \param x X coordinate on the screen
     * \param y Y coordinate on the screen
     * \param width Width of the buffer
     * \param height Height of the buffer
     * \param stride Count of bytes for a single row
     * \param format Image format of the buffer
     * \return Pointer to buffer on success
     * \sa blight_add_surface
     * \sa blight_surface_to_fbg
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_buf_t* blight_create_buffer(
        int x,
        int y,
        unsigned int width,
        unsigned int height,
        unsigned int stride,
        BlightImageFormat format
    );
    /*!
     * \brief blight_buffer_deref Release memory for a image buffer
     * \param buf Buffer to release
     * \sa blight_create_buffer
     */
    LIBBLIGHT_PROTOCOL_EXPORT void blight_buffer_deref(blight_buf_t* buf);
    /*!
     * \brief blight_add_surface Add an image buffer as a surface on the screen
     * \param bus The dbus connection
     * \param buf The image buffer to add as a surface
     * \return 0 on failure, otherwise the surface identifier
     * \sa blight_create_buffer
     * \sa blight_surface_to_fbg
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_surface_id_t blight_add_surface(
        blight_bus* bus,
        blight_buf_t* buf
    );
    /*!
     * \brief blight_cast_to_repaint_packet Cast a blight_message_t to a blight_packet_repaint_t
     * \param message Message to cast
     * \return blight_packet_repaint_t on success
     * \sa blight_message_from_socket
     * \sa blight_message_from_data
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_packet_repaint_t* blight_cast_to_repaint_packet(
        blight_message_t* message
    );
    /*!
     * \brief blight_cast_to_move_packet Cast a blight_message_t to a blight_packet_move_t
     * \param message Message to cast
     * \return blight_move_t instance on success
     * \sa blight_message_from_socket
     * \sa blight_message_from_data
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_packet_move_t* blight_cast_to_move_packet(
        blight_message_t* message
    );
    /*!
     * \brief blight_cast_to_surface_info_packet Cast a blight_message_t to a blight_packet_surface_info_t
     * \param message Message to cast
     * \return blight_packet_surface_info_t instance on success
     * \sa blight_message_from_socket
     * \sa blight_message_from_data
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_packet_surface_info_t* blight_cast_to_surface_info_packet(
        blight_message_t* message
    );
    /*!
     * \brief blight_event_from_socket Read an input event from the input socket
     * \param fd File descriptor of the socket
     * \param packet blight_event_packet_t pointer on success
     * \return 0 on success
     * \sa blight_service_input_open
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_event_from_socket(
        int fd,
        blight_event_packet_t** packet
    );
    /*!
     * \brief blight_surface_to_fbg Create a fbgraphics instance for a surface
     * \param fd File descriptor for the socket
     * \param identifier Surface identifier
     * \param buf Image buffer for the surface
     * \return fbgraphics instance on success
     * \sa blight_add_surface
     * \sa blight_create_buffer
     * \sa blight_service_open
     * \note Currently only supports Format_RGB32 buffers
     * \note calling fbg_close will remove the surface and call blight_buffer_deref on the buffer
     */
    LIBBLIGHT_PROTOCOL_EXPORT struct _fbg* blight_surface_to_fbg(
        int fd,
        blight_surface_id_t identifier,
        blight_buf_t* buf
    );
    /*!
     * \brief blight_move_surface Move a surface to a new location on the screen
     * \param fd File descriptor for the socket
     * \param identifier Surface identifier
     * \param buf Image buffer for the surface
     * \param x New X coordinate
     * \param y New Y coordinate
     * \return negative number if failed
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_move_surface(
        int fd,
        blight_surface_id_t identifier,
        blight_buf_t* buf,
        int x,
        int y
    );
    /*!
     * \brief blight_thread_t Wrapper for background connection thread
     */
    struct blight_thread_t;
    /*!
     * \brief blight_start_connection_thread Start a background thread to handle pings and acks from the display server
     * \param fd File descriptor for the socket
     * \return Pointer to thread, nullptr if failed
     */
    LIBBLIGHT_PROTOCOL_EXPORT struct blight_thread_t* blight_start_connection_thread(int fd);
    /*!
     * \brief blight_join_connection_thread Join a connection thread and wait for it to stop
     * \param thread Thread to join
     * \return 0 on success, negative number on failure
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_join_connection_thread(
        struct blight_thread_t* thread
    );
    /*!
     * \brief blight_detach_connection_thread Detach a connection thread so it will continue if the parent process exits
     * \param thread Thread to detach
     * \return 0 on success, negative number on failure
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_detach_connection_thread(
        struct blight_thread_t* thread
    );
    /*!
     * \brief blight_stop_connection_thread Request that a connection thread stops
     * \param thread Thread to stop
     * \return If the stop request was successful
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_stop_connection_thread(
        struct blight_thread_t* thread
    );
    /*!
     * \brief blight_connection_thread_deref Clean up a thread, will stop it if it's running
     * \param thread Thread to deref
     * \return If the thread was dereferenced properly
     * \note This will join the thread waiting for it to stop
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_connection_thread_deref(
        struct blight_thread_t* thread
    );
    /*!
     * \brief blight_remove_surface Remove a surface from the display server
     * \param fd File descriptor for the socket
     * \param identifier Identifier for the surface we want to remove
     * \return 0 on success, negative number on failure
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_remove_surface(
        int fd,
        blight_surface_id_t identifier
    );
    /*!
     * \brief blight_surface_id_list_t List of surface ids
     */
    struct blight_surface_id_list_t;
    /*!
     * \brief blight_list_surfaces Get the list of surfaces for a connection
     * \param fd File descriptor for the connection socket
     * \param list List of surfaces, will be nullptr if failed
     * \return Negative number on failure, 0 on success
     * \sa blight_surface_id_list_deref
     * \sa blight_surface_id_list_count
     * \sa blight_surface_id_list_data
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_list_surfaces(
        int fd,
        struct blight_surface_id_list_t** list
    );
    /*!
     * \brief blight_surface_id_list_deref Free a surface id list
     * \param list List to free
     * \return 0 on success, negative number on failure
     * \sa blight_list_surfaces
     */
    LIBBLIGHT_PROTOCOL_EXPORT int blight_surface_id_list_deref(
        struct blight_surface_id_list_t* list
    );
    /*!
     * \brief blight_surface_id_list_count Get the count of items in a surface id list
     * \param list List to get count from
     * \return Count of items in the list
     * \sa blight_surface_id_list_data
     */
    LIBBLIGHT_PROTOCOL_EXPORT unsigned int blight_surface_id_list_count(
        struct blight_surface_id_list_t* list
    );
    /*!
     * \brief blight_surface_id_list_data Get the data pointer from a surface id list
     * \param list List to get count from
     * \return Data pointer for the list
     * \sa blight_surface_id_list_count
     */
    LIBBLIGHT_PROTOCOL_EXPORT blight_surface_id_t* blight_surface_id_list_data(
        struct blight_surface_id_list_t* list
    );
    /*!
     * \brief blight_surface_repaint Repaint a surface
     * \param fd File descriptor for the connection socket
     * \param identifier Surface identifier to repaint
     * \param x X cordinate on the surface to repaint
     * \param y Y coordinate on the surface to repaint
     * \param width Width of area on the surface to repaint
     * \param height Height of area on the surface to repaint
     * \param waveform Waveform to use when repainting
     * \param mode Update mode to use when repainting
     * \return 0 on error otherwise the marker used for the repaint call
     */
    LIBBLIGHT_PROTOCOL_EXPORT unsigned int blight_surface_repaint(
        int fd,
        blight_surface_id_t identifier,
        int x,
        int y,
        unsigned int width,
        unsigned int height,
        BlightWaveformMode waveform,
        BlightUpdateMode mode
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
#undef BlightWaveformMode
#undef BlightUpdateMode
#endif
/*! @} */
