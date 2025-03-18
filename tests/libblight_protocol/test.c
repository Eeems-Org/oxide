#include <libblight_protocol.h>
#include <libblight_protocol/socket.h>
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
#include <pthread.h>

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
void test_blight_bus_connect_system(){
    assert(blight_bus_connect_system(&bus) > 0);
    assert(bus != NULL);
}
void test_blight_service_available(){
    assert(blight_service_available(bus));
}
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
struct _test_blight_wait_for_read_thread_args{
    int fd;
    blight_data_t data;
};
void* _test_blight_wait_for_read_thread(void* vargs){
    struct _test_blight_wait_for_read_thread_args* args = vargs;
    write(args->fd, args->data, 10);
    return NULL;
}
void test_blight_wait_for_read(){
    int fds[2];
    assert(
      socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) != -1
      && "Failed to create socket pair"
      );
    int clientFd = fds[0];
    int serverFd = fds[1];
    blight_data_t data = malloc(10);
    assert(data != NULL && "Malloc failed");
    struct _test_blight_wait_for_read_thread_args args;
    args.fd = serverFd;
    args.data = data;
    pthread_t tid;
    assert(
      pthread_create(&tid, NULL, _test_blight_wait_for_read_thread, (void*)&args) == 0
      && "Failed to start background thread"
      );
    int res = blight_wait_for_read(clientFd, 1000);
    assert(res == 0);
    pthread_join(tid, NULL);
    free(data);
    close(clientFd);
    close(serverFd);
}
void test_blight_recv(){
    int fds[2];
    assert(
      socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) != -1
      && "Failed to create socket pair"
      );
    int clientFd = fds[0];
    int serverFd = fds[1];
    blight_data_t data = malloc(10);
    assert(data != NULL && "Malloc failed");
    int res = blight_recv(clientFd, &data, 10);
    assert(-res == EAGAIN);
    write(serverFd, data, 10);
    blight_data_t data2 = NULL;
    res = blight_recv(clientFd, &data2, 10);
    assert(res == 0);
    assert(data2 != NULL);
    assert(memcmp(data, data2, 10) == 0);
    free(data);
    free(data2);
    close(clientFd);
    close(serverFd);
}
struct _test_blight_recv_blocking_thread_args{
    int fd;
    blight_data_t data;
};
void* _test_blight_recv_blocking_thread(void* vargs){
    struct _test_blight_recv_blocking_thread_args* args = vargs;
    write(args->fd, args->data, 10);
    return NULL;
}
void test_blight_recv_blocking(){
    int fds[2];
    assert(
        socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) != -1
        && "Failed to create socket pair"
    );
    int clientFd = fds[0];
    int serverFd = fds[1];
    blight_data_t data = malloc(10);
    assert(data != NULL && "Malloc failed");
    struct _test_blight_recv_blocking_thread_args args;
    args.fd = serverFd;
    args.data = data;
    pthread_t tid;
    assert(
        pthread_create(&tid, NULL, _test_blight_recv_blocking_thread, (void*)&args) == 0
        && "Failed to start background thread"
    );
    blight_data_t data2 = NULL;
    assert(blight_recv(clientFd, &data2, 10) == 0);
    assert(data2 != NULL);
    assert(memcmp(data, data2, 10) == 0);
    pthread_join(tid, NULL);
    free(data);
    free(data2);
    close(clientFd);
    close(serverFd);
}
void test_blight_send_blocking(){
    int fds[2];
    assert(
        socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) != -1
        && "Failed to create socket pair"
    );
    int clientFd = fds[0];
    int serverFd = fds[1];
    blight_data_t data = malloc(10);
    assert(data != NULL && "Malloc failed");
    assert(blight_send_blocking(serverFd, data, 10) == 0);
    blight_data_t data2 = NULL;
    assert(blight_recv(clientFd, &data2, 10) == 0);
    assert(data2 != NULL);
    assert(memcmp(data, data2, 10) == 0);
    free(data);
    free(data2);
    close(clientFd);
    close(serverFd);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclobbered"
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
    TEST(test_blight_recv, true);
    TEST(test_blight_wait_for_read, true);
    TEST(test_blight_recv_blocking, true);
    TEST(test_blight_send_blocking, true);

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
