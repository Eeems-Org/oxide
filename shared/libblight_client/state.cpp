#include "state.h"

#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <libblight/debug.h>
#include <string>

namespace Client {
  bool INITIALIZED = false;
  bool FAILED_INIT = true;
  bool IS_XOCHITL = false;
  DeviceType deviceType = DeviceType::UNKNOWN;
  bool init() {
    auto debugLevel = getenv("OXIDE_PRELOAD_DEBUG");
    if (debugLevel != nullptr) {
      try {
        set_blight_debug_level(std::stoi(debugLevel));
      } catch (std::invalid_argument&) {
      } catch (std::out_of_range&) {
        _WARN("OXIDE_PRELOAD_DEBUG invalid value");
      }
    }
    std::ios_base::Init i;
    std::ifstream deviceIdFile{"/sys/devices/soc0/machine"};
    std::string deviceId;
    std::getline(deviceIdFile, deviceId);
    if (deviceId == "reMarkable 1.0" || deviceId == "reMarkable Prototype 1") {
      Client::deviceType = Client::DeviceType::RM1;
    } else if (deviceId == "reMarkable 2.0") {
      Client::deviceType = Client::DeviceType::RM2;
    } else if (deviceId == "reMarkable Ferrari") {
      Client::deviceType = Client::DeviceType::RMPP;
    } else if (deviceId == "reMarkable Chiappa") {
      Client::deviceType = Client::DeviceType::RMPPM;
    } else if (deviceId == "reMarkable Tatsu") {
      Client::deviceType = Client::DeviceType::RMPPURE;
    } else {
      _WARN("Unknown device: %s", deviceId.c_str());
      Client::deviceType = Client::DeviceType::UNKNOWN;
    }
    std::string path;
    realpath("/proc/self/exe", path);
    if (path == "/usr/bin/xochitl") {
      Client::IS_XOCHITL = true;
    }
    if (
      !Client::IS_XOCHITL &&
      (!path.starts_with("/opt") || path.starts_with("/opt/sbin")) &&
      (!path.starts_with("/home") ||
       path.starts_with("/home/root/.entware/sbin"))
    ) {
      // We ignore this executable
      set_blight_debug_level(0);
    }
    return true;
  }
  void realpath(const char* pathname, std::string& path) {
    char absolutepath[4096];
    if (::realpath(pathname, absolutepath) != nullptr) {
      path = absolutepath;
    }
  }
  bool isInputEnabled() {
    if (isForceQt()) {
      return false;
    }
    return getenv("OXIDE_PRELOAD_EXPOSE_INPUT") == nullptr;
  }
  bool isFbEnabled() {
    if (isForceQt()) {
      return true;
    }
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
  bool isFakeRM1Name() {
    if (isFakeRM1()) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM1_NAME") != nullptr;
    return enabled;
  }
  bool isFakeRM1Fb() {
    if (isFakeRM1()) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM1_FB") != nullptr;
    return enabled;
  }
  bool forceRGB16() {
    if (isFakeRM1Fb()) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RGB16") != nullptr;
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
  const std::string deviceTypeName() {
    switch (deviceType) {
      case DeviceType::RM1:
        return "reMarkable 1";
      case DeviceType::RM2:
        return "reMarkable 2";
      case Client::DeviceType::RMPP:
        return "reMarkable Paper Pro";
      case Client::DeviceType::RMPPM:
        return "reMarkable Paper Pro Move";
      case Client::DeviceType::RMPPURE:
        return "reMarkable Paper Pure";
      default:
        return "Unknown Device";
    }
  }
}
