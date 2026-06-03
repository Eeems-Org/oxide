#include "state.h"

namespace Client {
  bool INITIALIZED = false;
  bool FAILED_INIT = true;
  bool HANDLE_FB = true;
  bool DUMP_FB = false;
  bool FAKE_RM1 = false;
  bool RM1_FB = false;
  DeviceType deviceType = DeviceType::UNKNOWN;
  bool IS_XOCHITL = false;
  bool HANDLE_INPUT = true;
}
