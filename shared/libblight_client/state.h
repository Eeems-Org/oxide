#pragma once

namespace Client {
  enum DeviceType {
    UNKNOWN,
    RM1,
    RM2,
    RMPP,
    RMPPM,
    RMPPURE,
  };
  extern bool INITIALIZED;
  extern bool FAILED_INIT;
  extern bool HANDLE_FB;
  extern bool DUMP_FB;
  extern bool FAKE_RM1;
  extern bool RM1_FB;
  extern DeviceType deviceType;
  extern bool IS_XOCHITL;
}
