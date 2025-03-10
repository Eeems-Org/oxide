/*!
 * \addtogroup Blight
 * @{
 * \file
 */
#pragma once
#include "libblight_global.h"
#include <linux/input.h>
#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <libblight_protocol.h>

namespace Blight{
    /*!
     * \brief Image format of a buffer
     */
    using Format = BlightProtocol::BlightImageFormat;
    /*!
     * \brief Possible waveforms
     */
    using Waveform = BlightProtocol::BlightWaveform;
    /*!
     * \brief Waveform to use for a repaint
     */
    using WaveformMode = BlightProtocol::BlightWaveformMode;
    /*!
     * \brief Size type
     */
    typedef BlightProtocol::blight_size_t size_t;
    /*!
     * \brief Surface identifier
     */
    typedef BlightProtocol::blight_surface_id_t surface_id_t;
    /*!
     * \brief Partial input event
     */
    typedef BlightProtocol::blight_partial_input_event_t partial_input_event_t;
    /*!
     * \brief Input event packet
     */
    typedef BlightProtocol::blight_event_packet_t event_packet_t;
    /*!
     * \brief Generic data pointer
     */
    typedef unsigned char* data_t;
    /*!
     * \brief Shared pointer to generic data
     */
    typedef std::shared_ptr<unsigned char[]> shared_data_t;
    struct buf_t;
    /*!
     * \brief Shared pointer to buffer
     */
    typedef std::shared_ptr<buf_t> shared_buf_t;
    /*!
     * \brief Clipboard instance
     */
    typedef struct clipboard_t {
        /*!
         * \brief Data
         */
        shared_data_t data;
        /*!
         * \brief Size of data
         */
        size_t size;
        /*!
         * \brief Name of clipboard
         */
        std::string name;
        /*!
         * \brief Create a new clipboard instance
         * \param name Name of clipboard
         * \param data Data
         * \param size Data size
         */
        clipboard_t(const std::string name, data_t data = nullptr, size_t size = 0);
        /*!
         * \brief Convert the data to a string
         * \return The clipboard data as a string
         */
        const std::string to_string();
        /*!
         * \brief Get the latest data for the clipboard
         * \return If the clipboard was able to update
         * \retval true The clipboard was updated
         * \retval false An error occurred
         */
        bool update();
        /*!
         * \brief Set the data for the clipboard
         * \param data Data
         * \param size Size of data
         * \return If the clipboard was able to be set
         * \retval true The clipboard data was set
         * \retval false An error occurred
         */
        bool set(shared_data_t data, size_t size);
        /*!
         * \brief Set the data for the clipboard
         * \param data Data
         * \param size Size of data
         * \return If the clipboard was able to be set
         * \retval true The clipboard data was set
         * \retval false An error occurred
         */
        inline bool set(const char* data, size_t size){
            auto buf = new unsigned char[size];
            memcpy(buf, data, size);
            return set(shared_data_t(buf), size);
        }
        /*!
         * \brief Set the data for the clipboard
         * \param data Data
         * \param size Size of data
         * \return If the clipboard was able to be set
         * \retval true The clipboard data was set
         * \retval false An error occurred
         */
        inline bool set(const data_t data, size_t size){ return set((char*)data, size); }
        /*!
         * \brief Set the data for the clipboard to a string
         * \param data String to set the data to
         * \return If the clipboard was able to be set
         * \retval true The clipboard data was set
         * \retval false An error occurred
         */
        inline bool set(const std::string& data){ return set(data.data(), data.size()); }
    } clipboard_t;
    /*!
     * \brief A buffer used to represent a surface
     */
    typedef struct buf_t{
        /*!
         * \brief File descriptor for the buffer
         */
        int fd;
        /*!
         * \brief X coordinate of the buffer
         */
        int x;
        /*!
         * \brief Y coordinate of the buffer
         */
        int y;
        /*!
         * \brief Width of the buffer
         */
        int width;
        /*!
         * \brief Height of the buffer
         */
        int height;
        /*!
         * \brief Bytes per line in the buffer
         */
        int stride;
        /*!
         * \brief Image format of the buffer
         */
        Format format;
        /*!
         * \brief Memory mapped data of the buffer
         */
        data_t data;
        /*!
         * \brief UUID of the buffer
         */
        std::string uuid;
        /*!
         * \brief Surface identifier
         */
        surface_id_t surface;
        /*!
         * \brief Size of the buffer
         * \return
         */
        size_t size();
        /*!
         * \brief Close the buffer
         * \return Negative number if there was an error
         * \retval 0 No error while closing the buffer
         */
        int close();
        ~buf_t();
        /*!
         * \brief Clone a buffer
         * \return Cloned buffer if there was no error
         */
        std::optional<shared_buf_t> clone();
        static shared_buf_t new_ptr();
        static std::string new_uuid();
    } buf_t;
    /*!
     * \brief Message type
     */
    using MessageType = BlightProtocol::BlightMessageType;
    /*!
     * \brief Message header
     */
    typedef struct header_t{
        /*!
         * \brief Message type
         */
        MessageType type;
        /*!
         * \brief Unique identifier for this message
         */
        unsigned int ackid;
        /*!
         * \brief Size of data
         */
        unsigned long size;
        /*!
         * \brief Get a header from data
         * \param data Data
         * \return Header
         */
        static header_t from_data(data_t data);
        /*!
         * \brief Get a header from data
         * \param data Data
         * \return Header
         */
        static header_t from_data(char* data);
        /*!
         * \brief Get a header from data
         * \param data Data
         * \return header
         */
        static header_t from_data(void* data);
        /*!
         * \brief Create a new invalid header
         * \return Invalid header object
         */
        static header_t new_invalid();
    } header_t;
    struct message_t;
    /*!
     * \brief Shared pointer to a message
     */
    typedef std::shared_ptr<message_t> message_ptr_t;
    /*!
     * \brief Message object
     */
    typedef struct message_t{
        /*!
         * \brief Message header
         */
        header_t header;
        /*!
         * \brief Message data
         */
        shared_data_t data;
        /*!
         * \brief Get a message from data
         * \param _data Data
         * \return
         */
        static message_t from_data(data_t _data);
        /*!
         * \brief Get a message from data
         * \param data Data
         * \return
         */
        static message_t from_data(char* data);
        /*!
         * \brief Get a message from data
         * \param data Data
         * \return
         */
        static message_t from_data(void* data);
        /*!
         * \brief Create an Ack header from a message
         * \param message Message to create ack from
         * \param size Size of response data
         * \return Ack header
         */
        static header_t create_ack(message_t* message, size_t size = 0);
        /*!
         * \brief Create an Ack header from a message
         * \param message Message to create ack from
         * \param size Size of response data
         * \return Ack header
         */
        static header_t create_ack(const message_t& message, size_t size = 0);
        /*!
         * \brief Read a message from a socket
         * \param fd Socket descriptor
         * \return Message
         */
        static message_ptr_t from_socket(int fd);
        /*!
         * \brief Create a new invalid and empty message
         * \return Invalid and empty message
         */
        static message_ptr_t new_ptr();
    } message_t;
    /*!
     * \brief Repaint message data
     */
    typedef struct repaint_t{
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
        WaveformMode waveform;
        /*!
         * \brief marker Marker to use
         */
        unsigned int marker;
        /*!
         * \brief identifier Surface identifier
         */
        surface_id_t identifier;
        /*!
         * \brief Get the repaint message data from a message
         * \param message Message
         * \return Repaint message data
         */
        static repaint_t from_message(const message_t* message);
    } repaint_t;
    /*!
     * \brief Move message data
     */
    typedef struct move_t{
        /*!
         * \brief identifier Surface identifier
         */
        surface_id_t identifier;
        /*!
         * \brief x X coordinate
         */
        int x;
        /*!
         * \brief y Y coordinate
         */
        int y;
        /*!
         * \brief Get the move message data from a message
         * \param message Message
         * \return Move message data
         */
        static move_t from_message(const message_t* message);
    } move_t;
    /*!
     * \brief Surface information message data
     */
    typedef struct surface_info_t{
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
        Format format;
        /*!
         * \brief Get the surface information message data from data
         * \param data Data
         * \return Surface message data
         */
        static surface_info_t from_data(data_t data);
    } surface_info_t;
}
/*! @} */
