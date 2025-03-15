#include <libblight_protocol.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
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
#define TEST_EXPR(name, expression) \
    { \
        total_tests++; \
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
    }
#define TEST(method) \
    TEST_EXPR(method, { \
        method(); \
    })

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclobbered"
int test_c(){
    fprintf(stderr, "********* Start testing of C tests *********\n");
    int fd = 0;
    TEST(test_blight_header_from_data);
    TEST(test_blight_message_from_data);
    TEST(test_blight_bus_connect_system);
    TEST(test_blight_service_available);
    TEST_EXPR("test_blight_service_open", fd = test_blight_service_open());
    TEST(test_blight_service_input_open);
    int pingid = 0;
    TEST_EXPR(test_blight_message_from_socket, pingid = test_blight_message_from_socket(fd));
    TEST_EXPR(test_blight_send_message, pingid = test_blight_send_message(fd, pingid));
    blight_buf_t* buf = NULL;
    TEST_EXPR(test_blight_create_buffer, buf = test_blight_create_buffer());
    TEST_EXPR(test_blight_add_surface, test_blight_add_surface(buf));
    fprintf(
        stderr,
        "Totals: %d passed, %d failed, 0 skipped, 0 blacklisted\n",
        total_tests - failed_tests,
        failed_tests
    );
    fprintf(stderr, "********* Finished testing of C tests *********\n");
    return failed_tests;
}
#pragma GCC diagnostic pop
