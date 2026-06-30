#include "state.h"
#include "fb.h"

#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <libblight/debug.h>
#include <libblight/types.h>
#include <string>
#include <strings.h>
#include <sys/syslog.h>

const std::string
debugLevelString() {
  switch (get_blight_debug_level()) {
    case LOG_DEBUG:
      return "debug";
    case LOG_WARNING:
      return "warning";
    case LOG_INFO:
      return "info";
    case LOG_CRIT:
      return "critical";
    default:
      return "unknown";
  }
}

namespace Client {
  bool INITIALIZED = false;
  bool FAILED_INIT = true;
  bool IS_XOCHITL = false;
  DeviceType deviceType = DeviceType::UNKNOWN;
#define bool_str(value) value ? "true" : "false"
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
    std::string path;
    realpath("/proc/self/exe", path);
    if (path == "/usr/bin/xochitl") {
      IS_XOCHITL = true;
    }
    if (
      !IS_XOCHITL && path != "/usr/bin/evtest" &&
      (!path.starts_with("/opt") || path.starts_with("/opt/sbin")) &&
      (!path.starts_with("/home") ||
       path.starts_with("/home/root/.entware/sbin"))
    ) {
      // We ignore this executable
      set_blight_debug_level(0);
      FAILED_INIT = false;
      return false;
    }
    std::ios_base::Init i;
    std::ifstream deviceIdFile{"/sys/devices/soc0/machine"};
    std::string deviceId;
    std::getline(deviceIdFile, deviceId);
    if (deviceId == "reMarkable 1.0" || deviceId == "reMarkable Prototype 1") {
      deviceType = DeviceType::RM1;
    } else if (deviceId == "reMarkable 2.0") {
      deviceType = DeviceType::RM2;
    } else if (deviceId == "reMarkable Ferrari") {
      deviceType = DeviceType::RMPP;
    } else if (deviceId == "reMarkable Chiappa") {
      deviceType = DeviceType::RMPPM;
    } else if (deviceId == "reMarkable Tatsu") {
      deviceType = DeviceType::RMPPURE;
    } else {
      _WARN("Unknown device: %s", deviceId.c_str());
      deviceType = DeviceType::UNKNOWN;
    }
    _INFO(
      "\nDebug Level: %s"
      "\nDevice: %s\nFB: %s"
      "\nInput: %s"
      "\nPower Button: %s"
      "\nDump FB: %s"
      "\nFake rM1: %s"
      "\nFake rM1 Name: %s"
      "\nFake rM1 FB: %s"
      "\nFake rM1 Input: %s"
      "\nForce RGB16: %s"
      "\nRM2FB Allowed: %s"
      "\nFake RM2FB: %s"
      "\nForce Qt: %s"
      "\nGhost Control: %s"
      "\nScale: %f\n",
      debugLevelString().c_str(),
      deviceTypeName().c_str(),
      bool_str(isFbEnabled()),
      bool_str(isInputEnabled()),
      bool_str(isPowerButtonEnabled()),
      bool_str(isDumpFb()),
      bool_str(isFakeRM1()),
      bool_str(isFakeRM1Name()),
      bool_str(isFakeRM1Fb()),
      bool_str(isFakeRM1Input()),
      bool_str(forceRGB16()),
      bool_str(isRM2FBAllowed()),
      bool_str(isFakeRM2FB()),
      bool_str(forceQt()),
      bool_str(isGhostControlEnabled()),
      FB::deviceScale()
    );
    return true;
  }
#undef bool_str
  void realpath(const char* pathname, std::string& path) {
    char absolutepath[4096];
    if (::realpath(pathname, absolutepath) != nullptr) {
      path = absolutepath;
    }
  }
  bool isInputEnabled() {
    if (forceQt()) {
      return false;
    }
    return getenv("OXIDE_PRELOAD_EXPOSE_INPUT") == nullptr;
  }
  bool isFbEnabled() {
    if (forceQt()) {
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
    if (deviceType == DeviceType::RM1) {
      return false;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM1") != nullptr;
    return enabled;
  }
  bool isFakeRM1Name() {
    if (deviceType == DeviceType::RM1) {
      return false;
    }
    if (isFakeRM1()) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM1_NAME") != nullptr;
    return enabled;
  }
  bool isFakeRM2Name() {
    if (isFakeRM1() || deviceType == DeviceType::RM2) {
      return false;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM2_NAME") != nullptr;
    return enabled;
  }
  bool isFakeRM1Fb() {
    if (deviceType == DeviceType::RM1) {
      return false;
    }
    if (isFakeRM1()) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM1_FB") != nullptr;
    return enabled;
  }
  bool isFakeRM1Input() {
    if (deviceType == DeviceType::RM1) {
      return false;
    }
    if (isFakeRM1()) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RM1_INPUT") != nullptr;
    return enabled;
  }
  bool forceInvertTouchX() {
    static bool enabled =
      getenv("OXIDE_PRELOAD_FORCE_INVERT_TOUCH_X") != nullptr;
    return enabled;
  }
  bool forceRGB16() {
    if (isFakeRM1Fb()) {
      return true;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_RGB16") != nullptr;
    return enabled;
  }
  bool force32bitFscreenInfo() {
    if (deviceType == DeviceType::RM1 || deviceType == DeviceType::RM2) {
      return false;
    }
    static bool enabled =
      getenv("OXIDE_PRELOAD_FORCE_32BIT_FSCREENINFO") != nullptr;
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
    if (Client::deviceType == Client::DeviceType::RM1) {
      return false;
    }
    static bool enabled = getenv("OXIDE_PRELOAD_NO_FAKE_RM2FB") == nullptr;
    return enabled;
  }
  bool forceQt() {
    static bool enabled = getenv("OXIDE_PRELOAD_FORCE_QT") != nullptr;
    return enabled;
  }
  bool isGhostControlEnabled() {
    static bool enabled = getenv("OXIDE_PRELOAD_GHOST_CONTROL") != nullptr;
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
  int forcedWaveform() {
    static char* waveform = getenv("OXIDE_PRELOAD_FORCE_WAVEFORM");
    if (waveform == nullptr) {
      return -1;
    }
    if (strcasecmp(waveform, "UltraFast") == 0) {
      return Blight::WaveformMode::UltraFast;
    }
    if (strcasecmp(waveform, "Fast") == 0) {
      return Blight::WaveformMode::Fast;
    }
    if (strcasecmp(waveform, "Animate") == 0) {
      return Blight::WaveformMode::Animate;
    }
    if (strcasecmp(waveform, "Content") == 0) {
      return Blight::WaveformMode::Content;
    }
    if (strcasecmp(waveform, "UI") == 0) {
      return Blight::WaveformMode::UI;
    }
    if (strcasecmp(waveform, "Full") == 0) {
      return Blight::WaveformMode::Full;
    }
    return -1;
  }
}
