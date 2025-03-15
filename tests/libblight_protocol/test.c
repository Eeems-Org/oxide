#include <libblight_protocol.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void test_c(){
    fprintf(stderr, "Running C tests...\n");

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

    fprintf(stderr, "Testing blight_message_from_data\n");
    blight_data_t data = malloc(sizeof(blight_header_t) + 1);
    memcpy(data, &header, sizeof(blight_header_t));
    data[sizeof(blight_header_t)] = 'a';
    blight_message_t* message = blight_message_from_data(data);
    assert(message->header.type == 1);
    assert(message->header.ackid == 1);
    assert(message->header.size == 1);
    assert(message->data != NULL);
    assert(*message->data == 'a');
    blight_message_deref(message);
    free(data);

    fprintf(stderr, "Testing blight_bus_connect_system\n");
    blight_bus* bus = NULL;
    assert(blight_bus_connect_system(&bus) > 0);
    assert(bus != NULL);

    fprintf(stderr, "Testing blight_service_available\n");
    assert(blight_service_available(bus));

    fprintf(stderr, "Testing blight_service_open\n");
    int res = blight_service_open(bus);
    assert(res > 0);
    int fd = res;

    fprintf(stderr, "Testing blight_service_input_open\n");
    res = blight_service_input_open(bus);
    assert(res > 0);
    close(res);
    blight_bus_deref(bus);

    fprintf(stderr, "Testing blight_message_from_socket\n");
    res = -EAGAIN;
    while(-res == EAGAIN || -res == EINTR){
        res = blight_message_from_socket(fd, &message);
    }
    assert(res == 0);
    assert(message->header.type == Ping);
    int pingid = message->header.ackid;
    blight_message_deref(message);

    fprintf(stderr, "Testing blight_send_message\n");
    res = blight_send_message(fd, Ack, pingid, 0, NULL);
    assert(res == 0);
    res = -EAGAIN;
    while(-res == EAGAIN || -res == EINTR){
        res = blight_message_from_socket(fd, &message);
    }
    assert(res == 0);
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
    assert(message->header.type == Ack);
    assert(message->header.ackid == 1);
    blight_message_deref(message);

    close(fd);
    fprintf(stderr, "C tests PASS!\n");
}
