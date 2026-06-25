#pragma once
#include <string>

namespace Client {
  enum DeviceType {
    INVALID,
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
  bool init();
  void realpath(const char* pathname, std::string& path);
  bool isInputEnabled();
  bool isFbEnabled();
  bool isPowerButtonEnabled();
  bool isDumpFb();
  bool isFakeRM1();
  bool isFakeRM1Name();
  bool isFakeRM2Name();
  bool isFakeRM1Fb();
  bool isFakeRM1Input();
  bool forceInvertTouchX();
  bool forceRGB16();
  bool force32bitFscreenInfo();
  bool isRM2FBAllowed();
  bool isFakeRM2FB();
  bool forceQt();
  bool isGhostControlEnabled();
  const std::string deviceTypeName();
  int forcedWaveform();
}
