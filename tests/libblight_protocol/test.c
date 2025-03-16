#include <libblight_protocol.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#define UNUSED(x) (void)(x)
#define TRY(expression, c_expression, f_expression) \
    { \
        jmp_buf __ex_buf__; \
        void __ex_handle__(int s){ \
            UNUSED(s); \
            assert(signal(SIGABRT, SIG_DFL)  != SIG_ERR); \
            longjmp(__ex_buf__, 1); \
        } \
        assert(signal(SIGABRT, __ex_handle__)  != SIG_ERR); \
        if(setjmp(__ex_buf__) == 0){ \
            expression; \
        }else{ \
            c_expression; \
        } \
        f_expression; \
    }
#define THROW longjmp(__ex_buf__, 1)

static int failed_tests = 0;
static int total_tests = 0;
static int skipped_tests = 0;
#define TEST_EXPR(name, expression, predicate) \
    { \
        total_tests++; \
        if(predicate){ \
            int __success__ = 0; \
            TRY(expression, __success__ = 1, { \
                if(__success__ == 0){ \
                    fprintf(stderr, "PASS"); \
                }else{ \
                    fprintf(stderr, "FAIL"); \
                } \
                fprintf(stderr, "   : " #name "()\n"); \
            }); \
            failed_tests += __success__; \
        }else{ \
            skipped_tests++; \
            fprintf(stderr, "SKIP   : " #name "()\n"); \
        } \
    }
#define TEST(method, predicate) \
    TEST_EXPR(method, method(), predicate)

static blight_bus* bus = NULL;

void test_blight_header_from_data(){
    blight_header_t x;
    x.type = 1;
    x.ackid = 1;
    x.size = 1;
    blight_header_t header = blight_header_from_data((blight_data_t)&x);
    assert(header.type == 1);
    assert(header.ackid == 1);
    assert(header.size == 1);
    x.type = 0;
    x.ackid = 0;
    x.size = 0;
    blight_header_t header2 = blight_header_from_data((blight_data_t)&x);
    assert(header2.type == 0);
    assert(header2.ackid == 0);
    assert(header2.size == 0);
}
/**
 * @brief Tests the conversion of raw data into a blight message.
 *
 * This function creates a buffer containing a blight header with preset values and a single
 * character payload. It then calls blight_message_from_data() to construct a blight message and
 * verifies that the header fields and the data content are correctly populated.
 */
void test_blight_message_from_data(){
    blight_header_t header;
    header.type = 1;
    header.ackid = 1;
    header.size = 1;
    blight_data_t data = malloc(sizeof(blight_header_t) + 1);
    assert(data != NULL && "Failed to malloc a header");
    memcpy(data, &header, sizeof(blight_header_t));
    data[sizeof(blight_header_t)] = 'a';
    blight_message_t* message = blight_message_from_data(data);
    assert(message != NULL);
    assert(message->header.type == 1);
    assert(message->header.ackid == 1);
    assert(message->header.size == 1);
    assert(message->data != NULL);
    assert(*message->data == 'a');
    blight_message_deref(message);
    free(data);
}
/**
 * @brief Tests that the blight bus system is successfully connected.
 *
 * This function invokes blight_bus_connect_system() using the address of the global bus variable,
 * then asserts that the connection returns a positive value and that the bus pointer is set to a non-NULL value.
 */
void test_blight_bus_connect_system(){
    assert(blight_bus_connect_system(&bus) > 0);
    assert(bus != NULL);
}
/**
 * @brief Tests if the blight service is available.
 *
 * Uses the global bus to verify that the blight service is active by asserting the result
 * of the blight_service_available function.
 */
void test_blight_service_available(){
    assert(blight_service_available(bus));
}
int test_blight_service_open(){
    int res = blight_service_open(bus);
    assert(res > 0);
    return res;
}
/**
 * @brief Validates that the service input is opened successfully.
 *
 * This test calls `blight_service_input_open` using the global bus and asserts that the
 * returned file descriptor is positive. It then closes the file descriptor to release
 * allocated resources.
 */
void test_blight_service_input_open(){
    int res = blight_service_input_open(bus);
    assert(res > 0);
    close(res);
}
/**
 * @brief Retrieves a ping message from a socket and returns its acknowledgment ID.
 *
 * This function continuously attempts to receive a message from the specified socket file descriptor,
 * retrying if the operation is interrupted (EINTR) or if the data is temporarily unavailable (EAGAIN).
 * Upon a successful read, it asserts that the message is valid and of type Ping, extracts the acknowledgment
 * identifier from the message header, dereferences the message to release resources, and returns the ping ID.
 *
 * @param fd Socket file descriptor used to read the ping message.
 *
 * @return int The acknowledgment ID from the received ping message.
 */
int test_blight_message_from_socket(int fd){
    int res = -EAGAIN;
    blight_message_t* message = NULL;
    while(-res == EAGAIN || -res == EINTR){
        res = blight_message_from_socket(fd, &message);
    }
    assert(res == 0);
    assert(message != NULL);
    assert(message->header.type == Ping);
    int pingid = message->header.ackid;
    blight_message_deref(message);
    return pingid;
}
/**
 * @brief Tests the bidirectional exchange of Ack and Ping messages over a socket.
 *
 * This function verifies the blight protocol's messaging operations by performing a two-step exchange.
 * It first sends an Ack message with the provided ping identifier and waits to receive a Ping message,
 * from which it updates the ping identifier. It then sends a Ping message and confirms that an Ack message
 * with the expected identifier is received. The function employs assertions to ensure that the messaging
 * behavior conforms to the protocol.
 *
 * @param fd The socket file descriptor used for communication.
 * @param pingid The initial ping identifier, which is updated based on the received Ping message.
 *
 * @return int The updated ping identifier extracted from the first exchange.
 */
int test_blight_send_message(int fd, int pingid){
    int res = blight_send_message(fd, Ack, pingid, 0, NULL);
    assert(res == 0);
    res = -EAGAIN;
    blight_message_t* message = NULL;
    while(-res == EAGAIN || -res == EINTR){
        res = blight_message_from_socket(fd, &message);
    }
    assert(res == 0);
    assert(message != NULL);
    assert(message->header.type == Ping);
    pingid = message->header.ackid;
    blight_message_deref(message);
    res = blight_send_message(fd, Ping, 1, 0, NULL);
    assert(res == 0);
    res = -EAGAIN;
    while(-res == EAGAIN || -res == EINTR){
        res = blight_message_from_socket(fd, &message);
    }
    assert(res == 0);
    assert(message != NULL);
    assert(message->header.type == Ack);
    assert(message->header.ackid == 1);
    blight_message_deref(message);
    return pingid;
}
blight_buf_t* test_blight_create_buffer(){
    blight_buf_t* buf = blight_create_buffer(10, 10, 100, 100, 200, Format_RGB16);
    assert(buf != NULL);
    assert(buf->x == 10);
    assert(buf->y == 10);
    assert(buf->width == 100);
    assert(buf->height == 100);
    assert(buf->stride == 200);
    assert(buf->format == Format_RGB16);
    assert(buf->data != NULL);
    return buf;
}
/**
 * @brief Tests adding a surface to the global bus.
 *
 * This function adds a surface using the provided buffer by invoking
 * blight_add_surface on the global bus. It asserts that the returned
 * surface identifier is positive, indicating a successful addition,
 * and then cleans up by dereferencing the buffer.
 *
 * @param buf Pointer to a blight buffer containing the surface data.
 */
void test_blight_add_surface(blight_buf_t* buf){
    blight_surface_id_t identifier = blight_add_surface(bus, buf);
    assert(identifier > 0);
    blight_buffer_deref(buf);
}
/**
 * @brief Tests the conversion of a blight message into a repaint packet.
 *
 * This function verifies that blight_cast_to_repaint_packet handles various error conditions
 * and successfully casts a valid message. It checks that:
 * - Passing a NULL pointer sets errno to EINVAL.
 * - A message with an invalid header type or missing data sets errno appropriately (EINVAL or ENODATA).
 * - A message with a data size that does not match a repaint packet sets errno to EMSGSIZE.
 * - A valid repaint message returns a properly initialized repaint packet.
 */
void test_blight_cast_to_repaint_packet(){
    assert(blight_cast_to_repaint_packet(NULL) == NULL);
    assert(errno == EINVAL);
    blight_message_t message;
    blight_header_t header;
    header.type = Invalid;
    header.ackid = 1;
    header.size = 0;
    message.header = header;
    message.data = NULL;
    assert(blight_cast_to_repaint_packet(&message) == NULL);
    assert(errno == EINVAL);
    message.header.type = Repaint;
    assert(blight_cast_to_repaint_packet(&message) == NULL);
    assert(errno == ENODATA);
    blight_packet_repaint_t data = {0};
    message.data = (blight_data_t)&data;
    assert(blight_cast_to_repaint_packet(&message) == NULL);
    assert(errno == EMSGSIZE);
    message.header.size = sizeof(blight_packet_repaint_t);
    blight_packet_repaint_t* packet = blight_cast_to_repaint_packet(&message);
    assert(errno == 0);
    assert(packet != NULL);
    assert(packet->x == 0);
    assert(packet->y == 0);
    assert(packet->width == 0);
    assert(packet->height == 0);
    assert(packet->waveform == 0);
    assert(packet->marker == 0);
    assert(packet->identifier == 0);
}
/**
 * @brief Tests the conversion of a message to a move packet.
 *
 * This function validates that the blight_cast_to_move_packet function handles various conditions correctly:
 * - Passing a NULL pointer returns NULL and sets errno to EINVAL.
 * - A message with an invalid header type returns NULL and sets errno to EINVAL.
 * - When the header type is Move but the data is NULL or the message size is incorrect, it returns NULL and sets errno to ENODATA or EMSGSIZE respectively.
 * - A properly configured message with a Move header and corresponding data size successfully casts to a move packet with default field values.
 */
void test_blight_cast_to_move_packet(){
    assert(blight_cast_to_move_packet(NULL) == NULL);
    assert(errno == EINVAL);
    blight_message_t message;
    blight_header_t header;
    header.type = Invalid;
    header.ackid = 1;
    header.size = 0;
    message.header = header;
    message.data = NULL;
    assert(blight_cast_to_move_packet(&message) == NULL);
    assert(errno == EINVAL);
    message.header.type = Move;
    assert(blight_cast_to_move_packet(&message) == NULL);
    assert(errno == ENODATA);
    blight_packet_move_t data = {0};
    message.data = (blight_data_t)&data;
    assert(blight_cast_to_move_packet(&message) == NULL);
    assert(errno == EMSGSIZE);
    message.header.size = sizeof(blight_packet_move_t);
    blight_packet_move_t* packet = blight_cast_to_move_packet(&message);
    assert(errno == 0);
    assert(packet != NULL);
    assert(packet->identifier == 0);
    assert(packet->x == 0);
    assert(packet->y == 0);
}
void test_blight_cast_to_surface_info_packet(){
    assert(blight_cast_to_surface_info_packet(NULL) == NULL);
    assert(errno == EINVAL);
    blight_message_t message;
    blight_header_t header;
    header.type = Invalid;
    header.ackid = 1;
    header.size = 0;
    message.header = header;
    message.data = NULL;
    assert(blight_cast_to_surface_info_packet(&message) == NULL);
    assert(errno == EINVAL);
    message.header.type = Info;
    assert(blight_cast_to_surface_info_packet(&message) == NULL);
    assert(errno == ENODATA);
    blight_packet_surface_info_t data = {0};
    message.data = (blight_data_t)&data;
    assert(blight_cast_to_surface_info_packet(&message) == NULL);
    assert(errno == EMSGSIZE);
    message.header.size = sizeof(blight_packet_surface_info_t);
    blight_packet_surface_info_t* packet = blight_cast_to_surface_info_packet(&message);
    assert(errno == 0);
    assert(packet != NULL);
    assert(packet->x == 0);
    assert(packet->y == 0);
    assert(packet->width == 0);
    assert(packet->height == 0);
    assert(packet->stride == 0);
    assert(packet->format == 0);
}
/**
 * @brief Tests the retrieval of a zero-initialized event packet from a non-blocking socket.
 *
 * This function creates a pair of connected Unix domain sockets, sends a zero-initialized
 * `blight_event_packet_t` from the server socket, and then reads the packet using
 * `blight_event_from_socket` from the client socket. It verifies that the packet is received
 * successfully and that its fields (`device`, `event.type`, `event.code`, and `event.value`)
 * are all zero. Allocated memory is freed and the sockets are closed after the test.
 */
void test_blight_event_from_socket(){
    int fds[2];
    assert(
        socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) != -1
        && "Failed to create socket pair"
    );
    int clientFd = fds[0];
    int serverFd = fds[1];
    blight_event_packet_t data = {0};
    assert(send(serverFd, &data, sizeof(blight_event_packet_t), MSG_EOR | MSG_NOSIGNAL) > 0);
    blight_event_packet_t* packet = NULL;
    int res = blight_event_from_socket(clientFd, &packet);
    assert(res >= 0);
    assert(packet != NULL);
    assert(packet->device == 0);
    assert(packet->event.type == 0);
    assert(packet->event.code == 0);
    assert(packet->event.value == 0);
    free(packet);
    close(clientFd);
    close(serverFd);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclobbered"
/**
 * @brief Executes the complete test suite for the libblight_protocol.
 *
 * This function orchestrates a series of tests using predefined macros to validate various
 * components of the blight protocol library. It measures the total execution time, prints a summary 
 * of passed, failed, and skipped tests to stderr, and handles resource cleanup by closing open file 
 * descriptors and dereferencing global resources.
 *
 * @return int The number of tests that failed.
 */
int test_c(){
    fprintf(stderr, "********* Start testing of C tests *********\n");
    struct timeval start;
    gettimeofday(&start, NULL);
    TEST(test_blight_header_from_data, true);
    TEST(test_blight_message_from_data, true);
    TEST(test_blight_bus_connect_system, true);
    TEST(test_blight_service_available, bus != NULL);
    int fd = 0;
    TEST_EXPR(test_blight_service_open, fd = test_blight_service_open(), bus != NULL);
    TEST(test_blight_service_input_open, bus != NULL);
    int pingid = 0;
    TEST_EXPR(
        test_blight_message_from_socket,
        pingid = test_blight_message_from_socket(fd),
        fd > 0
    );
    TEST_EXPR(
        test_blight_send_message,
        pingid = test_blight_send_message(fd, pingid),
        fd > 0
    );
    blight_buf_t* buf = NULL;
    TEST_EXPR(test_blight_create_buffer, buf = test_blight_create_buffer(), true);
    TEST_EXPR(
        test_blight_add_surface,
        test_blight_add_surface(buf),
        bus != NULL && buf != NULL
    );
    TEST(test_blight_cast_to_repaint_packet, true);
    TEST(test_blight_cast_to_move_packet, true);
    TEST(test_blight_cast_to_surface_info_packet, true);
    TEST(test_blight_event_from_socket, true);

    if(fd > 0){
        close(fd);
    }
    if(bus != NULL){
        blight_bus_deref(bus);
    }

    struct timeval end;
    gettimeofday(&end, NULL);
    unsigned int elapsed = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
    fprintf(
        stderr,
        "Totals: %d passed, %d failed, %d skipped, 0 blacklisted, %ums\n",
        total_tests - failed_tests - skipped_tests,
        failed_tests,
        skipped_tests,
        elapsed
    );
    fprintf(stderr, "********* Finished testing of C tests *********\n");
    return failed_tests;
}
#pragma GCC diagnostic pop
