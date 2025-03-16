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
 * @brief Tests conversion of raw data into a blight_message_t structure.
 *
 * This function creates a blight header with specific values and allocates a data buffer
 * that contains the header data followed by a payload byte. It then invokes blight_message_from_data
 * to parse the raw data into a blight_message_t object and uses assertions to verify that both
 * the header fields and the payload are correctly extracted. Allocated resources are cleaned up after validation.
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
 * @brief Validates a successful connection to the blight bus system.
 *
 * This test function asserts that establishing a connection to the bus returns a positive value
 * and sets the bus pointer correctly to a non-NULL value.
 */
void test_blight_bus_connect_system(){
    assert(blight_bus_connect_system(&bus) > 0);
    assert(bus != NULL);
}
/**
 * @brief Tests that the blight service is available on the bus.
 *
 * This function asserts that the blight service is accessible by invoking
 * blight_service_available() on the global bus. If the service is not available,
 * the assertion will fail, indicating an error in the bus or service configuration.
 */
void test_blight_service_available(){
    assert(blight_service_available(bus));
}
/**
 * @brief Tests opening a service connection.
 *
 * This function calls blight_service_open() using a global bus and asserts that the returned
 * service handle is valid (i.e., greater than 0). It returns the service identifier on success.
 *
 * @return int A positive service identifier if the service is opened successfully.
 */
int test_blight_service_open(){
    int res = blight_service_open(bus);
    assert(res > 0);
    return res;
}
void test_blight_service_input_open(){
    int res = blight_service_input_open(bus);
    assert(res > 0);
    close(res);
}
/**
 * @brief Receives a ping message from a socket and returns its ping identifier.
 *
 * This function repeatedly attempts to read a message from the given socket file descriptor,
 * retrying if the operation would block or is interrupted (i.e., on EAGAIN or EINTR errors).
 * Upon a successful read, it asserts that the message is valid and of type Ping, then extracts
 * and returns the ping identifier from the message header after releasing the allocated memory.
 *
 * @param fd The socket file descriptor from which to receive the ping message.
 * @return int The ping identifier extracted from the received message.
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
 * @brief Tests the message send/receive protocol for blight.
 *
 * This function validates the correct exchange of messages over a blight connection. It first sends an ACK message with the provided ping identifier, then waits to receive a corresponding PING message. The ping identifier is updated based on the received message. Next, it sends a PING message with an identifier of 1 and verifies that an ACK message with an ack identifier of 1 is received. Assertion failures indicate deviations from the expected protocol behavior.
 *
 * @param fd The file descriptor for communication with the blight bus.
 * @param pingid The initial ping identifier; updated from the received message's acknowledgment id.
 *
 * @return int The updated ping identifier extracted from the received PING message.
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
/**
 * @brief Tests the creation of a buffer object.
 *
 * This function creates a new buffer using fixed parameters for position (10, 10),
 * dimensions (100x100), stride (200), and format (Format_RGB16). It verifies that the
 * buffer's attributes, including its data pointer, are correctly initialized.
 *
 * @return blight_buf_t* A pointer to the created and validated buffer.
 */
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
 * @brief Tests adding a surface to the bus.
 *
 * This function attempts to add a surface to the bus using the provided
 * buffer and then asserts that a valid (positive) surface identifier is returned.
 * It concludes by dereferencing the buffer to release its reference.
 *
 * @param buf Pointer to the buffer containing surface data.
 */
void test_blight_add_surface(blight_buf_t* buf){
    blight_surface_id_t identifier = blight_add_surface(bus, buf);
    assert(identifier > 0);
    blight_buffer_deref(buf);
}
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
 * @brief Tests the conversion of a blight message to a move packet.
 *
 * This function validates that the `blight_cast_to_move_packet` function correctly handles
 * various input scenarios by returning NULL and setting the appropriate errno values:
 * - When provided a NULL pointer or a message with an invalid header, it should return NULL and set errno to EINVAL.
 * - When the header type is correct (Move) but the message lacks data or has an incorrect size, it should return NULL with errno set to ENODATA or EMSGSIZE.
 * - When the message is properly formed with a Move header and correct size, it should return a valid move packet with all fields initialized.
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
/**
 * @brief Tests the conversion of a blight_message_t to a surface info packet.
 *
 * This function verifies that blight_cast_to_surface_info_packet correctly handles
 * various error conditions and returns a valid packet when provided with a well-formed
 * message. It asserts that:
 * - A NULL message results in a NULL return with errno set to EINVAL.
 * - A message with an invalid header type results in a NULL return with errno set to EINVAL.
 * - A message with a valid header type but NULL data results in a NULL return with errno set to ENODATA.
 * - A message with insufficient size returns NULL with errno set to EMSGSIZE.
 * - A valid message returns a properly initialized surface info packet with all members set to zero.
 */
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
 * @brief Runs all tests for the blight protocol library.
 *
 * This function orchestrates a comprehensive suite of tests that validate various aspects
 * of the blight protocol library, including header and message creation, service connectivity,
 * socket communication, buffer operations, and event processing. It measures the execution time,
 * logs test outcomes (passed, failed, and skipped) to standard error, and cleans up resources
 * such as file descriptors and bus connections.
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
