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
  extern bool IS_XOCHITL;
  extern DeviceType deviceType;
  bool isInputEnabled();
  bool isFbEnabled();
  bool isPowerButtonEnabled();
  bool isDumpFb();
  bool isFakeRM1();
  bool isFakeRM1Fb();
  bool isRM2FBAllowed();
  bool isFakeRM2FB();
  bool isForceQt();
}
