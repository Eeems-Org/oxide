#pragma once

#include <libblight_protocol.h>

#ifdef __cplusplus
extern "C" {
BlightProtocol::blight_input_buffer_t*
#else
blight_input_buffer_t*
#endif
create_test_input_buffer();
int
test_c();
#ifdef __cplusplus
}
#endif
