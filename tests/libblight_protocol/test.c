#include <libblight_protocol.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static blight_bus* bus = NULL;

void test_blight_header_from_data(){
    fprintf(stderr, "Testing blight_header_from_data\n");
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
    fprintf(stderr, "Testing blight_message_from_data\n");
    blight_header_t header;
    header.type = 1;
    header.ackid = 1;
    header.size = 1;
    blight_data_t data = malloc(sizeof(blight_header_t) + 1);
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
    fprintf(stderr, "Testing blight_bus_connect_system\n");
    assert(blight_bus_connect_system(&bus) > 0);
    assert(bus != NULL);
}
void test_blight_service_available(){
    fprintf(stderr, "Testing blight_service_available\n");
    assert(blight_service_available(bus));
}
int test_blight_service_open(){
    fprintf(stderr, "Testing blight_service_open\n");
    int res = blight_service_open(bus);
    assert(res > 0);
    return res;
}
void test_blight_service_input_open(){
    fprintf(stderr, "Testing blight_service_input_open\n");
    int res = blight_service_input_open(bus);
    assert(res > 0);
    close(res);
}
int test_blight_message_from_socket(int fd){
    fprintf(stderr, "Testing blight_message_from_socket\n");
    int res = -EAGAIN;
    blight_message_t* message;
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
    fprintf(stderr, "Testing blight_send_message\n");
    int res = blight_send_message(fd, Ack, pingid, 0, NULL);
    assert(res == 0);
    res = -EAGAIN;
    blight_message_t* message;
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
    fprintf(stderr, "Testing blight_create_buffer\n");
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
    fprintf(stderr, "Testing blight_add_surface\n");
    blight_surface_id_t identifier = blight_add_surface(bus, buf);
    assert(identifier > 0);
    blight_buffer_deref(buf);
}

void test_c(){
    fprintf(stderr, "Running C tests...\n");

    test_blight_header_from_data();
    test_blight_message_from_data();
    test_blight_bus_connect_system();
    test_blight_service_available();
    int fd = test_blight_service_open();
    test_blight_service_input_open();
    int pingid = test_blight_message_from_socket(fd);
    pingid = test_blight_send_message(fd, pingid);
    blight_buf_t* buf = test_blight_create_buffer();
    test_blight_add_surface(buf);

    close(fd);
    blight_bus_deref(bus);
    fprintf(stderr, "PASS - C tests\n");
}
