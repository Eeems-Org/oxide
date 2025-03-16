#include "libblight_protocol.h"
#include "socket.h"
#include "_debug.h"

#include <fcntl.h>
#include <cstring>
#include <linux/socket.h>
#include <unistd.h>
#include <sys/mman.h>
#include <random>
#include <sstream>
#include <ios>

#define XCONCATENATE(x, y) x ## y
#define CONCATENATE(x, y) XCONCATENATE(x, y)
#define UNIQ_T(x, uniq) CONCATENATE(__unique_prefix_, CONCATENATE(x, uniq))
#define UNIQ __COUNTER__
#define _STRV_FOREACH(s, l, i) for(typeof(*(l)) *s, *i = (l); (s = i) && *i; i++)
#define STRV_FOREACH(s, l) _STRV_FOREACH(s, l, UNIQ_T(i, UNIQ))

/**
 * @brief Frees a null-terminated array of dynamically allocated C-style strings.
 *
 * This function iterates through the provided array, freeing each individual string,
 * and then frees the array itself. If the input pointer is NULL, no action is taken.
 *
 * @param v Pointer to a null-terminated array of strings. May be NULL.
 * @return Always returns NULL.
 */
char** strv_free(char** v) {
    if(!v){
        return NULL;
    }
    for(char** i = v; *i; i++){
        free(*i);
    }
    free(v);
    return NULL;
}
/**
 * @brief Retrieves a human-readable error message.
 *
 * This function returns the error message from the provided bus error structure if available.
 * Otherwise, it returns the system error message corresponding to the negated error code.
 *
 * @param err The bus error structure containing an optional error message.
 * @param return_value The error code to convert into a system error message if no bus error message exists.
 * @return const char* A pointer to the error message string.
 */
inline const char* error_message(const sd_bus_error& err, int return_value){
    return err.message != nullptr ? err.message : std::strerror(-return_value);
}

/**
 * @brief Generates a version 4 universally unique identifier (UUID).
 *
 * Creates a UUID following the standard format "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx", where each
 * "x" is a random hexadecimal digit and "y" is one of 8, 9, A, or B. Internally, it utilizes thread-local
 * random number generators for efficient and thread-safe random number production.
 *
 * @return A std::string representing the generated UUID.
 */
std::string generate_uuid_v4(){
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_int_distribution<> dis(0, 15);
    thread_local std::uniform_int_distribution<> dis2(8, 11);
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };
    return ss.str();
}

using namespace BlightProtocol;
extern "C" {
    /**
     * @brief Connects to the system bus.
     *
     * Establishes a connection to the system bus for the Blight protocol by invoking
     * the underlying sd_bus_default_system function. The resulting connection handle
     * is stored in the provided bus pointer.
     *
     * @return int A non-negative value on success, or a negative error code on failure.
     */
    int blight_bus_connect_system(blight_bus** bus){
        return sd_bus_default_system(bus);
    }
    /**
     * @brief Connects to the user bus.
     *
     * Establishes a connection to the user bus using the default configuration.
     *
     * @return int 0 on success, or a negative error code on failure.
     */
    int blight_bus_connect_user(blight_bus** bus){
        return sd_bus_default_user(bus);
    }
    /**
     * @brief Releases a Blight bus connection.
     *
     * Decrements the reference count of the provided bus connection, potentially freeing
     * its associated resources when no other references remain.
     */
    void blight_bus_deref(blight_bus* bus){
        sd_bus_unref(bus);
    }
    /**
     * @brief Checks if the Blight service is available on the provided bus.
     *
     * This function queries the bus for both registered and activatable service names,
     * then returns true if the Blight service (BLIGHT_SERVICE) is found in either list.
     * If an error occurs during the query, a warning is logged and the function returns false.
     *
     * @return true if the Blight service is available; false otherwise.
     */
    bool blight_service_available(blight_bus* bus){
        char** names = NULL;
        char** activatable = NULL;
        int res = sd_bus_list_names(bus, &names, &activatable);
        if(res < 0){
            _WARN(
                "[blight_service_available::sd_bus_list_names] Error: %s",
                std::strerror(-res)
            );
            strv_free(names);
            strv_free(activatable);
            return false;
        }
        STRV_FOREACH(i, names){
            if(std::string(*i) == BLIGHT_SERVICE){
                strv_free(names);
                strv_free(activatable);
                return true;
            }
        }
        STRV_FOREACH(i, activatable){
            if(std::string(*i) == BLIGHT_SERVICE){
                strv_free(names);
                strv_free(activatable);
                return true;
            }
        }
        strv_free(names);
        strv_free(activatable);
        return false;
    }
    /**
     * @brief Opens a connection to the Blight service.
     *
     * This function first checks if the Blight service is available on the given bus.
     * If available, it sends a method call to open the service and reads a file descriptor
     * from the response. The file descriptor is then duplicated with the CLOEXEC flag
     * to ensure safe usage in forked processes. On failure, the function logs a warning,
     * cleans up resources, sets errno accordingly, and returns a negative error code.
     *
     * @return The duplicated file descriptor on success, or a negative error code on failure.
     */
    int blight_service_open(blight_bus* bus){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return -EAGAIN;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "open",
            &error,
            &message,
            ""
        );
        if(res < 0){
            _WARN(
                "[blight_service_open::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int fd;
        res = sd_bus_message_read(message, "h", &fd);
        if(res < 0){
            _WARN(
                "[blight_service_open::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        int e = errno;
        if(message != nullptr){
            sd_bus_message_unref(message);
        }
        sd_bus_error_free(&error);
        if(dfd < 0){
            _WARN(
                "[blight_service_open::fcntl(...)] Error: %s",
                std::strerror(e)
            );
            errno = e;
            return -errno;
        }
        return dfd;
    }
    /**
     * @brief Opens the Blight service input stream.
     *
     * This function verifies that the Blight service is available on the provided bus,
     * and if so, invokes the "openInput" method to retrieve a file descriptor for input.
     * The returned descriptor is duplicated with a close-on-exec flag added. In case of any
     * failure during service availability check, method call, message reading, or duplication,
     * the function logs a warning, cleans up resources, sets errno appropriately, and returns
     * a negative error code.
     *
     * @return A duplicated file descriptor on success, or a negative error code on failure.
     */
    int blight_service_input_open(blight_bus* bus){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return -EAGAIN;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "openInput",
            &error,
            &message,
            ""
        );
        if(res < 0){
            _WARN(
                "[blight_service_input_open::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int fd;
        res = sd_bus_message_read(message, "h", &fd);
        if(res < 0){
            _WARN(
                "[blight_service_input_open::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        int e = errno;
        if(message != nullptr){
            sd_bus_message_unref(message);
        }
        sd_bus_error_free(&error);
        if(dfd < 0){
            _WARN(
                "[blight_service_input_open::fcntl(...)] Error: %s",
                std::strerror(e)
            );
            errno = e;
            return -errno;
        }
        return dfd;
    }
    /**
     * @brief Extracts a Blight header from raw data.
     *
     * Copies the content from the provided data buffer into a blight header
     * structure. The caller must ensure that the memory pointed to by @p data
     * is valid and contains at least sizeof(blight_header_t) bytes.
     *
     * @param data Pointer to the memory block holding the header data.
     * @return blight_header_t The header extracted from the given data.
     */
    blight_header_t blight_header_from_data(blight_data_t data){
        blight_header_t header;
        memcpy(&header, data, sizeof(blight_header_t));
        return header;
    }
    /**
     * @brief Constructs a blight message from raw data.
     *
     * Parses the provided byte array to create a new blight_message_t instance. The header is extracted
     * using blight_header_from_data(). If the header indicates a non-zero payload size, memory is allocated
     * for the payload and the corresponding bytes are copied from the input buffer immediately following the header.
     * Otherwise, the data pointer is set to nullptr.
     *
     * @param data Raw byte buffer containing the message header and, optionally, its payload.
     * @return blight_message_t* Pointer to the newly allocated message. The caller is responsible for freeing it
     *         using blight_message_deref().
     */
    blight_message_t* blight_message_from_data(blight_data_t data){
        auto message = new blight_message_t;
        message->header = blight_header_from_data(data);
        if(message->header.size){
            message->data = new unsigned char[message->header.size];
            memcpy(message->data, &data[sizeof(message->header)], message->header.size);
        }else{
            message->data = nullptr;
        }
        return message;
    }
    /**
     * @brief Reads a Blight message from a socket.
     *
     * This function reads a message header from the specified socket file descriptor. If the header
     * indicates a non-zero payload size, the function performs a blocking read to acquire the payload.
     * A new message structure is then allocated and populated with the header and, if applicable, the data.
     *
     * A return value of 0 signifies that the message contains no payload, while a positive return value
     * corresponds to the size of the received payload. In case of an error, a negative error code (based on
     * errno) is returned.
     *
     * The caller is responsible for freeing the allocated message using `blight_message_deref`.
     *
     * @param fd Socket file descriptor from which the message is read.
     * @param message Output parameter that will point to the allocated message structure on success.
     *
     * @return int Size of the received payload if data is present, 0 if there is no payload, or a negative
     *         error code if an error occurs.
     */
    int blight_message_from_socket(int fd, blight_message_t** message){
        auto maybe = recv(fd, sizeof(blight_header_t));
        if(!maybe.has_value()){
            if(errno != EAGAIN && errno != EINTR){
                _WARN(
                    "Failed to read connection message header: %s socket=%d",
                    std::strerror(errno),
                    fd
                );
            }
            return -errno;
        }
        auto m = new blight_message_t{
            .header = blight_header_t{
              .type = BlightMessageType::Invalid,
              .ackid = 0,
              .size = 0
            },
            .data = nullptr
        };
        memcpy(&m->header, maybe.value(), sizeof(blight_header_t));
        delete[] maybe.value();
        if(!m->header.size){
            *message = m;
            return 0;
        }
        maybe = recv_blocking(fd, m->header.size);
        if(!maybe.has_value()){
            _WARN(
                "Failed to read connection message data: %s "
                "socket=%d, "
                "ackid=%u, "
                "type=%d, "
                "size=%ld",
                std::strerror(errno),
                fd,
                m->header.ackid,
                m->header.type,
                (long int)m->header.size
            );
            delete m;
            return -errno;
        }
        m->data = maybe.value();
        *message = m;
        return m->header.size;
    }
    /**
     * @brief Frees a blight_message_t instance and its associated data.
     *
     * Deallocates memory for the given blight_message_t and its accompanying data buffer (if present).
     * If the data pointer is non-null but the header size is zero, a warning is logged.
     *
     * @param message Pointer to the blight_message_t instance to be freed.
     */
    void blight_message_deref(blight_message_t* message){
        if(message->data != nullptr){
            if(!message->header.size){
                _WARN("Freeing blight_message_t with a data pointer but a size of 0");
            }
            delete[] message->data;
        }
        delete message;
    }
    /**
     * @brief Sends a message over a file descriptor.
     *
     * Constructs a message header with the specified type, acknowledgment ID, and data size, then sends it
     * over the provided file descriptor. If a non-zero size is specified, the function immediately follows up by
     * sending the additional data. On success, it returns the number of data bytes sent; on failure, it returns
     * a negative error code.
     *
     * @param fd The file descriptor to send the message over.
     * @param type The type of the message.
     * @param ackid The acknowledgment identifier for the message.
     * @param size The number of bytes in the additional data payload.
     * @param data Pointer to the additional message data; must be non-null if size is non-zero.
     *
     * @return The size of the data sent on success, or a negative error code on failure.
     */
    int blight_send_message(
        int fd,
        BlightMessageType type,
        unsigned int ackid,
        uint32_t size,
        blight_data_t data
    ){
        if(size && data == nullptr){
            _WARN("Data missing when size is not zero");
            return -EINVAL;
        }
        blight_header_t header{
            .type = type,
            .ackid = ackid,
            .size = size
        };
        if(!send_blocking(fd, (blight_data_t)&header, sizeof(blight_header_t))){
            _WARN("Failed to write connection message header: %s", std::strerror(errno));
            return -errno;
        }
        if(size && !send_blocking(fd, data, size)){
            _WARN("Failed to write connection message data: %s", std::strerror(errno));
            return -errno;
        }
        return size;
    }
    /**
     * @brief Creates a memory-mapped image buffer.
     *
     * This function allocates and initializes a buffer structure for image data using the specified
     * position, dimensions, stride, and image format. It creates an in-memory file descriptor with a
     * unique name, adjusts its size based on the provided stride and height, and maps the memory region
     * for read and write access. On failure at any step, it cleans up resources, sets errno accordingly,
     * and returns nullptr.
     *
     * @param x The x-coordinate of the buffer's origin.
     * @param y The y-coordinate of the buffer's origin.
     * @param width The width of the buffer in pixels.
     * @param height The height of the buffer in pixels.
     * @param stride The number of bytes per row in the buffer.
     * @param format The image format of the buffer.
     * @return blight_buf_t* A pointer to the new buffer on success, or nullptr on failure.
     */
    blight_buf_t* blight_create_buffer(
        int x,
        int y,
        int width,
        int height,
        int stride,
        BlightImageFormat format
    ){
        int fd = memfd_create(generate_uuid_v4().c_str(), MFD_ALLOW_SEALING);
        if(fd < 0){
            return nullptr;
        }
        ssize_t size = stride * height;
        if(ftruncate(fd, size)){
            int e = errno;
            close(fd);
            errno = e;
            return nullptr;
        }
        void* data = mmap(
            NULL,
            size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED_VALIDATE,
            fd,
            0
        );
        if(data == MAP_FAILED || data == nullptr){
            int e = errno;
            close(fd);
            errno = e;
            return nullptr;
        }
        return new blight_buf_t{
            .fd = fd,
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .stride = stride,
            .format = format,
            .data = (blight_data_t)data
        };
    }
    /**
     * @brief Releases a blight buffer by unmapping its memory and deallocating its resources.
     *
     * If the buffer pointer is valid, the function attempts to unmap the memory region
     * associated with the buffer's data. Regardless of the unmap operation's success,
     * the buffer is then deallocated. If the input pointer is null, the function returns immediately.
     * A warning is logged if the memory unmapping fails.
     *
     * @param buf Pointer to the blight buffer to be released.
     */
    void blight_buffer_deref(blight_buf_t* buf){
        if(buf == nullptr){
            return;
        }
        if(buf->data != nullptr){
            ssize_t size = buf->stride * buf->height;
            if (munmap(buf->data, size) != 0) {
                _WARN("Failed to unmap buffer: %s", std::strerror(errno));
            }
        }
        delete buf;
    }
    /**
     * @brief Adds a new surface to the Blight service.
     *
     * Sends an "addSurface" method call over the provided bus using
     * the attributes from the specified buffer. On success, a unique
     * surface identifier is returned. If the Blight service is unavailable
     * or if the method call fails, the function sets errno accordingly and returns 0.
     *
     * @param buf The buffer containing surface attributes, including the file descriptor,
     *            position (x, y), dimensions (width, height), stride, and image format.
     * @return blight_surface_id_t The identifier of the newly added surface, or 0 on error.
     */
    blight_surface_id_t blight_add_surface(blight_bus* bus, blight_buf_t* buf){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return 0;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "addSurface",
            &error,
            &message,
            "hiiiiii",
            buf->fd,
            buf->x,
            buf->y,
            buf->width,
            buf->height,
            buf->stride,
            buf->format
        );
        if(res < 0){
            _WARN(
                "[blight_add_surface::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return 0;
        }
        blight_surface_id_t identifier;
        res = sd_bus_message_read(message, "q", &identifier);
        if(res < 0){
            _WARN(
                "[blight_add_surface::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return 0;
        }
        return identifier;
    }
    /**
     * @brief Casts a message to a repaint packet.
     *
     * This function verifies that the provided message is valid for a repaint packet by ensuring:
     * - The message pointer is not null and its header type is Repaint (otherwise, errno is set to EINVAL).
     * - The message contains non-null data (otherwise, errno is set to ENODATA).
     * - The header size matches the size of a repaint packet (otherwise, errno is set to EMSGSIZE).
     *
     * @param message Pointer to the message to be cast.
     * @return Pointer to the repaint packet if the message is valid; nullptr otherwise.
     */
    blight_packet_repaint_t* blight_cast_to_repaint_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Repaint){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_repaint_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_repaint_t*)message->data;
    }
    /**
     * @brief Casts a blight message to a move packet.
     *
     * This function validates that the provided message is not null, has a header type of Move,
     * contains non-null data, and that the header size matches the size of a move packet.
     * If any validation fails, errno is set (to EINVAL, ENODATA, or EMSGSIZE) and the function returns nullptr.
     *
     * @param message Pointer to the blight message to cast.
     * @return Pointer to the move packet if the message is valid; otherwise, nullptr.
     */
    blight_packet_move_t* blight_cast_to_move_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Move){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_move_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_move_t*)message->data;
    }
    /**
     * @brief Casts a generic Blight message to a surface info packet.
     *
     * This function validates that the provided message:
     * - Is non-null and has a header type of Info.
     * - Contains non-null data.
     * - Has a header size matching the expected size of a surface info packet.
     *
     * If any check fails, it sets errno to EINVAL, ENODATA, or EMSGSIZE respectively and returns nullptr.
     * On success, errno is cleared and the function returns the message data cast to a blight_packet_surface_info_t pointer.
     *
     * @param message The Blight message expected to contain a surface info packet.
     * @return blight_packet_surface_info_t* Pointer to the surface info packet, or nullptr if validation fails.
     */
    blight_packet_surface_info_t* blight_cast_to_surface_info_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Info){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_surface_info_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_surface_info_t*)message->data;
    }
    /**
     * @brief Receives a Blight event packet from a socket.
     *
     * This function attempts to read a fixed-size event packet from the specified socket. If successful,
     * it allocates memory for the packet, copies the received data into it, and stores the pointer in the provided output parameter.
     * On failure, a negative error code is returned.
     *
     * @param fd The file descriptor of the socket to read from.
     * @param packet A pointer to a pointer where the newly allocated event packet will be stored.
     * @return 0 on success, or a negative error code if the packet could not be received.
     */
    int blight_event_from_socket(int fd, blight_event_packet_t** packet){
        auto maybe = recv(fd, sizeof(blight_event_packet_t));
        if(!maybe.has_value()){
            return -errno;
        }
        auto p = new blight_event_packet_t;
        memcpy(p, maybe.value(), sizeof(blight_event_packet_t));
        delete[] maybe.value();
        *packet = p;
        return 0;
    }
}
