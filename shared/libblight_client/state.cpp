#include "state.h"

namespace Client {
    bool INITIALIZED = false;
    bool FAILED_INIT = true;
    bool HANDLE_FB = true;
    bool DUMP_FB = true;
    bool FAKE_RM1 = false;
    DeviceType deviceType = DeviceType::UNKNOWN;
    bool IS_XOCHITL = false;
}
