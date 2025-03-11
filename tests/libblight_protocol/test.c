#include <libblight_protocol.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

void test_c(){
    fprintf(stderr, "Running C tests...\n");
    blight_bus* bus = NULL;
    fprintf(stderr, "Testing blight_bus_connect_system\n");
    assert(blight_bus_connect_system(&bus) > 0);
    assert(bus != NULL);
    fprintf(stderr, "Testing blight_service_available\n");
    assert(blight_service_available(bus));
    fprintf(stderr, "Testing blight_service_open\n");
    int res = blight_service_open(bus);
    assert(res > 0);
    close(res);
    fprintf(stderr, "Testing blight_service_input_open\n");
    res = blight_service_input_open(bus);
    assert(res > 0);
    close(res);
    blight_bus_deref(bus);
    fprintf(stderr, "Testing blight_header_from_data\n");
    blight_header_t x;
    x.type = 1;
    x.ackid = 1;
    x.size = 1;
    blight_header_t header = blight_header_from_data((blight_data_t)&x);
    assert(header.type == 1);
    assert(header.ackid == 1);
    assert(header.size == 1);
}
