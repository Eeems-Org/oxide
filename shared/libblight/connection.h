/*!
 * \addtogroup Blight
 * @{
 * \file
 */
#pragma once
#include "libblight_global.h"
#include "types.h"

#include <linux/input.h>
#include <sys/socket.h>
#include <vector>
#include <thread>
#include <map>
#include <condition_variable>
#include <functional>
#include <atomic>

/**
         * @brief Acknowledgment handle for synchronizing display server responses.
         *
         * This structure encapsulates an identifier and thread-safe synchronization primitives
         * to wait for and signal responses from the display server.
         */
        
        /**
         * @brief Constructs an acknowledgment handle.
         *
         * Initializes the acknowledgment identifier, expected data size, and associated data pointer.
         *
         * @param ackid Optional acknowledgment identifier. Defaults to 0.
         * @param data_size Expected size of the reply data in bytes. Defaults to 0.
         * @param data Pointer to the initial data buffer. Defaults to nullptr.
         */
        
        /**
         * @brief Destroys the acknowledgment handle.
         *
         * Cleans up resources associated with the handle.
         */
        
        /**
         * @brief Waits for the display server's reply.
         *
         * Blocks until a reply is received or the specified timeout elapses.
         *
         * @param timeout Timeout in milliseconds. A value of 0 means wait indefinitely.
         * @return true if the reply was received before the timeout elapsed, false otherwise.
         */
        
        /**
         * @brief Notifies all threads waiting on this acknowledgment handle.
         */
        
        /**
         * @brief Initializes a connection to the display server.
         *
         * Establishes a new connection using the provided socket descriptor.
         *
         * @param fd Socket descriptor for the connection.
         */
        
        /**
         * @brief Destroys the connection instance.
         *
         * Cleans up resources associated with the connection to the display server.
         */
        
        /**
         * @brief Retrieves the connection's socket descriptor.
         *
         * @return The socket descriptor used for communication with the display server.
         */
        
        /**
         * @brief Retrieves the input event socket descriptor.
         *
         * @return The socket descriptor for receiving input events from the display server.
         */
        
        /**
         * @brief Registers a callback to be invoked upon disconnection.
         *
         * The callback is executed with the connection's socket descriptor when the display server disconnects.
         *
         * @param callback Function to be called on disconnection.
         */
        
        /**
         * @brief Reads a message from the display server.
         *
         * Blocks until a message is available from the connection.
         *
         * @return A pointer to the received message.
         */
        
        /**
         * @brief Reads an input event from the display server.
         *
         * Attempts to read an input event from the input event socket.
         *
         * @return An event packet if an input event is available; otherwise, an empty optional.
         */
        
        /**
         * @brief Sends a message to the display server.
         *
         * Transmits a message with the specified type and payload to the display server. If __ackid is zero,
         * a unique acknowledgment identifier is automatically generated.
         *
         * @param type The type of the message.
         * @param data Pointer to the message data.
         * @param size Size of the message data in bytes.
         * @param __ackid Optional unique identifier for acknowledgment. Defaults to 0.
         * @return An acknowledgment pointer to wait for a response, or an empty optional if sending failed.
         */
        
        /**
         * @brief Pings the display server to measure response time.
         *
         * Sends a ping to the display server and returns the duration of the round-trip if a reply is received within
         * the specified timeout.
         *
         * @param timeout Timeout in milliseconds. A value of 0 means wait indefinitely.
         * @return The duration of the ping as a std::chrono::duration if successful; otherwise, an empty optional.
         */
        
        /**
         * @brief Waits for the display server to complete repainting for a specific marker.
         *
         * Blocks until the repaint operation associated with the given marker is finished.
         *
         * @param marker The marker identifier to wait on.
         */
        
        /**
         * @brief Requests a partial repaint of a surface.
         *
         * Specifies the area of the surface to repaint using the given coordinates and dimensions.
         * The optional waveform mode determines the quality of the repaint, and a marker can be specified
         * for synchronization purposes.
         *
         * @param identifier The identifier of the surface to repaint.
         * @param x X-coordinate offset of the repaint region.
         * @param y Y-coordinate offset of the repaint region.
         * @param width Width of the repaint region.
         * @param height Height of the repaint region.
         * @param waveform Waveform mode to use for repainting. Defaults to HighQualityGrayscale.
         * @param marker Optional marker for synchronizing the repaint operation. Defaults to 0.
         * @return An acknowledgment pointer if the repaint command is successfully sent; otherwise, an empty optional.
         */
        
        /**
         * @brief Requests a partial repaint of a surface using buffer information.
         *
         * Utilizes the surface identifier from the provided buffer along with explicit coordinates and dimensions to request a repaint.
         *
         * @param buf Buffer representing the surface.
         * @param x X-coordinate offset of the repaint region.
         * @param y Y-coordinate offset of the repaint region.
         * @param width Width of the repaint region.
         * @param height Height of the repaint region.
         * @param waveform Waveform mode to use for repainting. Defaults to HighQualityGrayscale.
         * @param marker Optional marker for synchronizing the repaint operation. Defaults to 0.
         * @return An acknowledgment pointer if the repaint command is successfully sent; otherwise, an empty optional.
         */
        
        /**
         * @brief Requests a full repaint of a surface using buffer dimensions.
         *
         * Uses the surface and its dimensions specified in the buffer to request a complete repaint.
         *
         * @param buf Buffer representing the surface and its current position and size.
         * @param waveform Waveform mode to use for repainting. Defaults to HighQualityGrayscale.
         * @param marker Optional marker for synchronizing the repaint operation. Defaults to 0.
         * @return An acknowledgment pointer if the repaint command is successfully sent; otherwise, an empty optional.
         */
        namespace Blight {
    class Connection;
    /*!
     * \brief Handle used for waiting for a response from the display server
     */
    typedef struct ackid_t{
        /*!
         * \brief Identifier
         */
        unsigned int ackid;
        bool done;
        /*!
         * \brief Return data size
         */
        unsigned int data_size;
        /*!
         * \brief Return data
         */
        shared_data_t data;
        std::mutex mutex;
        std::condition_variable condition;
        ackid_t(
            unsigned int ackid = 0,
            unsigned int data_size = 0,
            data_t data = nullptr
        );
        ~ackid_t();
        /*!
         * \brief Wait for the reply to return
         * \param timeout Timeout
         */
        bool wait(int timeout = 0);
        void notify_all();
    } ackid_t;
    /*!
     * \brief Shared pointer to ackid_t
     */
    typedef std::shared_ptr<ackid_t> ackid_ptr_t;
    /*!
     * \brief Optional response of ackid_ptr_t
     */
    typedef std::optional<ackid_ptr_t> maybe_ackid_ptr_t;
    /*!
     * \brief Display server connection
     */
    class LIBBLIGHT_EXPORT Connection {
    public:
        /*!
         * \brief Create a new connection instance
         * \param fd Connection socket descriptor
         */
        Connection(int fd);
        ~Connection();
        /*!
         * \brief Connection socket descriptor
         * \return Connection socket descriptor
         */
        int handle();
        /*!
         * \brief input_handle input event socket descriptor
         * \return input event socket descriptor
         */
        int input_handle();
        /*!
         * \brief Run a callback when the socket disconnects from the display server
         * \param callback Callback to run.
         */
        void onDisconnect(std::function<void(int)> callback);
        /*!
         * \brief Read a message from the display server connection
         * \return A message
         */
        message_ptr_t read();
        /*!
         * \brief Read an input event from the display server input events socket
         * \return Input event packet
         */
        std::optional<event_packet_t> read_event();
        /*!
         * \brief Send a message to the display server
         * \param type Type of message
         * \param data Data to send
         * \param size Size of data
         * \param __ackid Unique identifier to use, will automatically generate if set to 0
         * \return ack_ptr_t if the message was sent
         */
        maybe_ackid_ptr_t send(MessageType type, data_t data, size_t size, unsigned int __ackid = 0);
        /*!
         * \brief Ping the server
         * \param timeout Timeout
         * \return Duration of ping
         */
        std::optional<std::chrono::duration<double>> ping(int timeout = 0);
        /*!
         * \brief Wait for a marker to complete repainting on the display server
         * \param marker Maker to wait on
         */
        void waitForMarker(unsigned int marker);
        /*!
         * \brief Repaint a portion of a surface
         * \param identifier Surface identifier
         * \param x X offset on the surface
         * \param y Y offset on the surface
         * \param width Width to repaint
         * \param height Height to repaint
         * \param waveform Waveform to use
         * \param marker Marker
         * \return ack_ptr_t if there was no error
         */
        maybe_ackid_ptr_t repaint(
            surface_id_t identifier,
            int x,
            int y,
            int width,
            int height,
            WaveformMode waveform = WaveformMode::HighQualityGrayscale,
            unsigned int marker = 0
        );
        /*!
         * \brief Repaint a portion of a surface
         * \param buf Buffer representing surface
         * \param x X offset on the surface
         * \param y Y offset on the surface
         * \param width Width to repaint
         * \param height Height to repaint
         * \param waveform Waveform to use
         * \param marker Marker
         * \return ack_ptr_t if there was no error
         */
        inline maybe_ackid_ptr_t repaint(
            shared_buf_t buf,
            int x,
            int y,
            int width,
            int height,
            WaveformMode waveform = WaveformMode::HighQualityGrayscale,
            unsigned int marker = 0
        ){ return repaint(buf->surface, x, y, width, height, waveform, marker); }
        /*!
         * \brief Repaint a surface
         * \param buf Buffer representing surface
         * \param waveform Waveform to use
         * \param marker Marker
         * \return ack_ptr_t if there was no error
         */
        inline maybe_ackid_ptr_t repaint(
            shared_buf_t buf,
            WaveformMode waveform = WaveformMode::HighQualityGrayscale,
            unsigned int marker = 0
        ){ return repaint(buf->surface, buf->x, buf->y, buf->width, buf->height, waveform, marker); }
        /*!
         * \brief Move a surface
         * \param buf Buffer representing surface
         * \param x X coordinate to move to
         * \param y Y coordinate to move to
         */
        void move(shared_buf_t buf, int x, int y);
        /*!
         * \brief move
         * \param identifier Surface identifier
         * \param x X coordinate to move to
         * \param y Y coordinate to move to
         * \return ack_ptr_t if there was no error
         */
        maybe_ackid_ptr_t move(surface_id_t identifier, int x, int y);
        /*!
         * \brief Resize a surface
         * \param buf Buffer representing surface
         * \param width Width of new surface
         * \param height Height of new surface
         * \param stride Bytes per line in the buffer
         * \param new_data New data to use for the buffer
         * \return New shared_buf_t if there was no error
         */
        std::optional<shared_buf_t> resize(
            shared_buf_t buf,
            int width,
            int height,
            int stride,
            data_t new_data
        );
        /*!
         * \brief Raise a surface
         * \param identifier Surface identifier
         * \return ack_ptr_t if there was no error
         */
        maybe_ackid_ptr_t raise(surface_id_t identifier);
        /*!
         * \brief Raise a surface
         * \param buf Buffer representing surface
         * \return ack_ptr_t if there was no error
         */
        maybe_ackid_ptr_t raise(shared_buf_t buf);
        /*!
         * \brief Lower a surface
         * \param identifier Surface identifier
         * \return ack_ptr_t if there was no error
         */
        maybe_ackid_ptr_t lower(surface_id_t identifier);
        /*!
         * \brief Lower a surface
         * \param buf Buffer representing surface
         * \return ack_ptr_t if there was no error
         */
        maybe_ackid_ptr_t lower(shared_buf_t buf);
        /*!
         * \brief Get a buffer
         * \param identifier Surface identifier
         * \return ack_ptr_t if there was no error
         */
        std::optional<shared_buf_t> getBuffer(surface_id_t identifier);
        /*!
         * \brief Get the list of buffers for the connection
         * \return List of buffers for the connection
         */
        std::vector<shared_buf_t> buffers();
        /*!
         * \brief Remove a surface
         * \param buf Buffer representing surface
         * \return ack_ptr_t if there was no error
         */
        maybe_ackid_ptr_t remove(shared_buf_t buf);
        /*!
         * \brief Remove a surface
         * \param identifier Surface identifier
         * \return ack_ptr_t if there was no error
         */
        maybe_ackid_ptr_t remove(surface_id_t identifier);
        /*!
         * \brief Get the list of surfaces for the connection
         * \return List of surfaces identifiers
         */
        std::vector<surface_id_t> surfaces();
        /*!
         * \brief Make the current connection the focused connection, which recieves input events
         */
        void focused();

    private:
        int m_fd;
        int m_inputFd;
        std::atomic<bool> stop_requested;
        std::vector<std::function<void(int)>> disconnectCallbacks;
        std::thread thread;
        static void run(Connection* connection);
    };
}
/*! @} */
