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
 * @brief Frees a null-terminated array of C-strings.
 *
 * This function iterates through the provided null-terminated array, releasing each allocated string,
 * and then frees the array itself. If the input pointer is NULL, the function performs no actions.
 *
 * @param v Pointer to a null-terminated array of C-strings allocated on the heap.
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
 * @brief Retrieves a human-readable error message from a bus error.
 *
 * This function checks the provided bus error structure for an error message. If available, it returns the message;
 * otherwise, it returns the system error message corresponding to the negated error code using std::strerror.
 *
 * @param err Bus error structure that may contain an error message.
 * @param return_value Error code (typically negative) used to generate a system error message if no bus error message is set.
 * @return const char* The resulting error message.
 */
inline const char* error_message(const sd_bus_error& err, int return_value){
    return err.message != nullptr ? err.message : std::strerror(-return_value);
}

/**
 * @brief Generates a random version 4 UUID.
 *
 * This function creates a UUID according to the version 4 specification, producing a 36-character string in the format 8-4-4-4-12,
 * where the UUID is built using random hexadecimal digits.
 *
 * @return std::string The generated version 4 UUID.
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
     * Establishes a connection to the system bus and assigns the resulting bus object
     * to the provided pointer. Returns 0 on success or a negative error code on failure.
     *
     * @return int 0 if the connection is successfully established, or a negative error code on error.
     */
    int blight_bus_connect_system(blight_bus** bus){
        return sd_bus_default_system(bus);
    }
    /**
     * @brief Connects to the default user session bus.
     *
     * This function initializes a connection to the user bus by invoking the underlying system API
     * and stores the resulting bus instance in the provided pointer.
     *
     * @param bus Address of the pointer that will receive the connected bus instance.
     * @return int 0 on success, or a negative error code on failure.
     */
    int blight_bus_connect_user(blight_bus** bus){
        return sd_bus_default_user(bus);
    }
    /**
     * @brief Dereferences a Blight bus connection.
     *
     * Decrements the reference count of the provided bus connection and releases its resources when the count reaches zero.
     */
    void blight_bus_deref(blight_bus* bus){
        sd_bus_unref(bus);
    }
    /**
     * @brief Verifies the availability of the Blight service on the given bus.
     *
     * This function queries the bus for both active and activatable services and checks if the
     * service identifier (BLIGHT_SERVICE) is present in either list. On encountering an error
     * while fetching the service lists, it logs a warning and returns false.
     *
     * @return true if the service is found; false otherwise.
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
     * This function checks if the Blight service is available on the given bus and then invokes the "open"
     * method on the service interface to obtain a file descriptor. The retrieved descriptor is duplicated with
     * the close-on-exec flag to ensure proper resource management. If any step fails—whether due to unavailability
     * of the service, an error during the bus method call, or failure in duplicating the file descriptor—the function
     * logs a warning, sets errno accordingly, and returns a negative error code.
     *
     * @return On success, returns a duplicated file descriptor for communication with the Blight service.
     *         On failure, returns a negative error code with errno set to indicate the error.
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
     * @brief Opens an input connection to the Blight service.
     *
     * This function checks whether the Blight service is available on the provided bus
     * before attempting to open an input stream. It sends an "openInput" method call over
     * the bus, retrieves the associated file descriptor from the response, and duplicates
     * it with the CLOEXEC flag set. On success, the duplicated file descriptor is returned.
     * On failure, errno is set and a negative error code is returned.
     *
     * @return int Duplicated file descriptor for the input service on success, or a negative
     * error code on failure.
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
     * @brief Extracts a blight header from raw data.
     *
     * This function copies the first sizeof(blight_header_t) bytes from the input raw data buffer into a blight_header_t structure.
     * The provided data must be valid and contain at least sizeof(blight_header_t) bytes.
     *
     * @param data Raw data buffer containing header information.
     * @return blight_header_t The header constructed from the raw data.
     */
    blight_header_t blight_header_from_data(blight_data_t data){
        blight_header_t header;
        memcpy(&header, data, sizeof(blight_header_t));
        return header;
    }
    /**
     * @brief Creates a Blight message from raw data.
     *
     * This function allocates and initializes a new message structure by extracting its header from the provided
     * raw data buffer using `blight_header_from_data`. If the header indicates a non-zero payload size, the function
     * allocates memory to copy the payload from the buffer; otherwise, it sets the data pointer to null.
     *
     * @param data A raw data buffer containing the message header followed by an optional payload.
     * @return A pointer to the newly allocated `blight_message_t` structure.
     *
     * @note The returned message must be freed using `blight_message_deref` to avoid memory leaks.
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
     * @brief Receives and constructs a Blight message from a socket.
     *
     * This function first reads a fixed-size header from the specified socket file descriptor
     * and allocates a new message structure. The header is copied into the message structure,
     * and if it indicates that additional data is available (i.e., the size is non-zero),
     * the function then reads the corresponding data in a blocking manner. On success, the output
     * parameter is updated with the new message structure and the function returns the size of the
     * received message data (or 0 if no additional data is expected). If an error occurs during
     * header or data reception, a warning is logged and a negative error code corresponding to errno
     * is returned.
     *
     * Note: The caller is responsible for freeing the returned message using blight_message_deref().
     *
     * @param fd The socket file descriptor from which to read the message.
     * @param message Output parameter that will point to the allocated message structure upon success.
     *
     * @return The size of the received message data, or a negative error code on failure.
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
     * @brief Frees a blight message and its associated data buffer.
     *
     * This function deallocates a blight_message_t object along with its internal data buffer,
     * if one exists. It logs a warning when a non-null data pointer is encountered with a header size of zero.
     *
     * @param message Pointer to the blight message to be freed.
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
     * @brief Sends a Blight protocol message over a blocking socket.
     *
     * Constructs and transmits a message header using the specified type, acknowledgment ID,
     * and payload size, followed by the payload data if provided. A valid data pointer is required
     * when the payload size is non-zero.
     *
     * @param fd Socket file descriptor for sending the message.
     * @param type Blight message type.
     * @param ackid Unique acknowledgment identifier for the message.
     * @param size Size in bytes of the payload; must be zero if no payload is sent.
     * @param data Pointer to the message payload.
     * @return On success, returns the number of payload bytes sent; on error, returns a negative error code.
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
     * @brief Creates a shared memory buffer for image data.
     *
     * This function allocates an anonymous, sealable file descriptor and configures a memory-mapped area
     * sized to accommodate a buffer with the specified dimensions and stride. It then initializes a new
     * blight_buf_t structure with the provided geometry and image format. On failure, it cleans up any
     * allocated resources and returns a null pointer.
     *
     * @param x The x-coordinate of the buffer origin.
     * @param y The y-coordinate of the buffer origin.
     * @param width The width of the buffer in pixels.
     * @param height The height of the buffer in pixels.
     * @param stride The number of bytes per row in the buffer.
     * @param format The image format of the buffer.
     * @return blight_buf_t* Pointer to the newly created buffer structure, or nullptr if allocation fails.
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
     * @brief Releases resources associated with a blight buffer.
     *
     * This function unmaps the memory region allocated for the buffer (if any) and then frees the
     * buffer structure. If the buffer pointer is null, no action is taken. A warning is logged if
     * the unmapping operation fails.
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
     * @brief Registers a new surface with the Blight service.
     *
     * This function sends a request to add a surface using the properties defined in the provided buffer.
     * It first verifies the availability of the Blight service, then communicates the buffer's details over
     * the bus. On success, it returns a non-zero surface identifier; on failure, it returns 0 and sets errno
     * with the corresponding error code.
     *
     * @param buf Pointer to the buffer structure containing the surface's file descriptor and its positional
     *            and dimensional properties, including coordinates, width, height, stride, and image format.
     * @return A non-zero surface identifier if successful; otherwise, 0 (with errno set).
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
     * @brief Casts a blight message to a repaint packet.
     *
     * This function checks that the provided message is non-null, has a header type of Repaint,
     * contains non-null data, and that the data size matches the expected size of a repaint packet.
     * If any of these checks fail, it sets errno appropriately (EINVAL for null or incorrect type,
     * ENODATA for missing data, EMSGSIZE for size mismatch) and returns nullptr.
     *
     * @param message Pointer to the blight message to cast.
     * @return Pointer to a valid repaint packet if the message is valid; otherwise, nullptr.
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
     * This function validates the provided message by ensuring it is non-null, that its header indicates a move operation, the data pointer is non-null, and the header size matches the expected size of a move packet. If any of these checks fail, errno is set to an appropriate error code (EINVAL, ENODATA, or EMSGSIZE) and the function returns nullptr.
     *
     * @param message Pointer to the blight message to cast.
     * @return A pointer to a move packet structure on success, or nullptr if validation fails.
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
     * @brief Casts a message to a surface info packet.
     *
     * Validates that the provided message is non-null, has a header type of Info, and contains non-null data
     * with the size matching a blight_packet_surface_info_t. If any check fails, errno is set to:
     * - EINVAL if the message is null or its header type is not Info,
     * - ENODATA if the message data pointer is null,
     * - EMSGSIZE if the data size does not match sizeof(blight_packet_surface_info_t).
     * On success, errno is reset to 0 and the data pointer is cast to a blight_packet_surface_info_t*.
     *
     * @param message Pointer to the blight_message to be cast.
     * @return A pointer to a blight_packet_surface_info_t if the message is valid; otherwise, nullptr.
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
     * This function attempts to read a complete Blight event packet from the specified socket file descriptor.
     * Upon successfully receiving the data, it allocates memory for the event packet, copies the received data,
     * and sets the output parameter to point to the new packet. If the read operation fails, the function returns
     * a negative error code based on errno.
     *
     * @param fd The socket file descriptor to read from.
     * @param packet Pointer to where the address of the newly allocated event packet will be stored.
     * @return 0 if successful, or a negative error code if the reception fails.
     *
     * @note The caller is responsible for freeing the allocated event packet.
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
