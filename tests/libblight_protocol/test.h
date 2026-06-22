#pragma once

#include <libblight_protocol.h>

extern "C" {
#ifdef __cplusplus
BlightProtocol::blight_input_buffer_t*
#else
blight_input_buffer_t*
#endif
create_test_input_buffer();
int
test_c();
}
