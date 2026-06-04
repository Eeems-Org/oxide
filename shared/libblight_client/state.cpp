#include "state.h"
#include <cstdlib>

namespace Client {
  bool INITIALIZED = false;
  bool FAILED_INIT = true;
  bool IS_XOCHITL = false;
  DeviceType deviceType = DeviceType::UNKNOWN;
  bool isInputEnabled() {
    return getenv("OXIDE_PRELOAD_EXPOSE_INPUT") == nullptr;
  }
  bool isFbEnabled() {
    static bool enabled = getenv("OXIDE_PRELOAD_EXPOSE_FB") == nullptr;
    return enabled;
  }
  bool isPowerButtonEnabled() {
    return getenv("OXIDE_PRELOAD_ALLOW_POWER_BUTTON") != nullptr;
  }
  bool isDumpFb() {
    return getenv("OXIDE_PRELOAD_DUMP_FB") != nullptr;
  }
  bool isFakeRM1() {
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM1") != nullptr;
    return enabled;
  }
  bool isFakeRM1Fb() {
    if (isFakeRM1()) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM1_FB") != nullptr;
    return enabled;
  }
  bool isRM2FBAllowed() {
    if (IS_XOCHITL) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_ALLOW_RM2FB") != nullptr;
    return enabled;
  }
  bool isFakeRM2FB() {
    if (Client::deviceType != Client::DeviceType::RM1) {
      return false;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_NO_FAKE_RM2FB") == nullptr;
    return enabled;
  }
  bool isForceQt() {
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_QT") != nullptr;
    return enabled;
  }
}
