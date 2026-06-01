#include "system.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "debug.h"
#include "libblight.h"
#include "meta.h"

namespace Blight {
  DBus* dbus = nullptr;

  int frameBuffer() {
    if (!exists()) {
      errno = EAGAIN;
      return -1;
    }
    _DEBUG("[Blight::frameBuffer()]");
    auto reply =
      dbus->call_method(BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "frameBuffer");
    if (reply->isError()) {
      _WARN(
        "[Blight::frameBuffer()::call_method(...)] Error: %s",
        reply->error_message().c_str()
      );
      return reply->return_value;
    }
    auto fd = reply->read_value<int>("h");
    if (!fd.has_value()) {
      _WARN(
        "[Blight::frameBuffer()::read_value(\"h\")] Error: %s",
        reply->error_message().c_str()
      );
      return reply->return_value;
    }
    int dfd = dup(fd.value());
    if (dfd == -1) {
      _WARN(
        "[Blight::frameBuffer()::dup(%d)] Error: %s",
        fd.value(),
        reply->error_message().c_str()
      );
      return -errno;
    }
    return dfd;
  }
  std::tuple<int, int, int> frameBufferInfo() {
    if (!exists()) {
      errno = EAGAIN;
      return {-1, -1, -1};
    }
    _DEBUG("[Blight::frameBufferInfo()]");
    auto reply = dbus->call_method(
      BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "frameBufferInfo"
    );
    if (reply->isError()) {
      _WARN(
        "[Blight::frameBufferInfo()::call_method(...)] Error: %s",
        reply->error_message().c_str()
      );
      errno = reply->return_value;
      return {-1, -1, -1};
    }
    int width, height, stride;
    reply->return_value =
      sd_bus_message_read(reply->message, "(iii)", &width, &height, &stride);
    if (reply->isError()) {
      _WARN(
        "[Blight::frameBufferInfo()::read_value(\"(iii)\")] Error: %s",
        reply->error_message().c_str()
      );
      errno = reply->return_value;
      return {-1, -1, -1};
    }
    return {width, height, stride};
  }
  bool waitForNoRepaints() {
    if (!exists()) {
      errno = EAGAIN;
      return false;
    }
    _DEBUG("[Blight::waitForNoRepaints()]");
    auto reply = dbus->call_method(
      BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "waitForNoRepaints"
    );
    if (reply->isError()) {
      _WARN(
        "[Blight::waitForNoRepaints()::call_method(...)] Error: %s",
        reply->error_message().c_str()
      );
      errno = reply->return_value;
      return false;
    }
    return true;
  }
  bool enterExclusiveMode() {
    if (!exists()) {
      errno = EAGAIN;
      return false;
    }
    _DEBUG("[Blight::enterExclusiveMode()]");
    auto reply = dbus->call_method(
      BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "enterExclusiveMode"
    );
    if (reply->isError()) {
      _WARN(
        "[Blight::enterExclusiveMode()::call_method(...)] Error: %s",
        reply->error_message().c_str()
      );
      return false;
    }
    return true;
  }
  bool exitExclusiveMode() {
    if (!exists()) {
      errno = EAGAIN;
      return false;
    }
    _DEBUG("[Blight::exitExclusiveMode()]");
    auto reply = dbus->call_method(
      BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "exitExclusiveMode"
    );
    if (reply->isError()) {
      _WARN(
        "[Blight::exitExclusiveMode()::call_method(...)] Error: %s",
        reply->error_message().c_str()
      );
      return false;
    }
    return true;
  }
  bool exclusiveModeRepaintFull() {
    if (!exists()) {
      errno = EAGAIN;
      return false;
    }
    _DEBUG("[Blight::exclusiveModeRepaintFull()]");
    auto reply = dbus->call_method(
      BLIGHT_SERVICE, "/", BLIGHT_INTERFACE, "exclusiveModeRepaintFull"
    );
    if (reply->isError()) {
      _WARN(
        "[Blight::exclusiveModeRepaintFull()::call_method(...)] Error: "
        "%s",
        reply->error_message().c_str()
      );
      return false;
    }
    return true;
  }
  bool exclusiveModeRepaint(
    int x,
    int y,
    int width,
    int height,
    WaveformMode waveform,
    UpdateMode updateMode
  ) {
    if (!exists()) {
      errno = EAGAIN;
      return false;
    }
    _DEBUG(
      "[Blight::exclusiveModeRepaint(%d, %d, %d, %d, %d, %d)]",
      x,
      y,
      width,
      height,
      waveform,
      updateMode
    );
    auto reply = dbus->call_method(
      BLIGHT_SERVICE,
      "/",
      BLIGHT_INTERFACE,
      "exclusiveModeRepaint",
      "iiiiii",
      x,
      y,
      width,
      height,
      waveform,
      updateMode
    );
    if (reply->isError()) {
      _WARN(
        "[Blight::exclusiveModeRepaint()::call_method(...)] Error: %s",
        reply->error_message().c_str()
      );
      return false;
    }
    return true;
  }
  bool setFlags(std::string identifier, std::vector<std::string> flags) {
    if (!exists()) {
      errno = EAGAIN;
      return false;
    }
    if (identifier.empty()) {
      errno = EINVAL;
      return false;
    }
    _DEBUG("[Blight::setFlags(%s, ...)]", identifier.c_str());
    sd_bus_message* message = nullptr;
    auto res = sd_bus_message_new_method_call(
      dbus->bus(), // <-- fix 1: bus() accessor
      &message,
      BLIGHT_SERVICE,
      "/",
      BLIGHT_INTERFACE,
      "setFlags"
    );
    if (res < 0) {
      _WARN(
        "[Blight::setFlags()::new_method_call()] Error: %s", std::strerror(-res)
      );
      return false;
    }
    // Append string: identifier
    res = sd_bus_message_append(message, "s", identifier.c_str());
    if (res < 0) {
      _WARN("[Blight::setFlags()::append(s)] Error: %s", std::strerror(-res));
      sd_bus_message_unref(message);
      return false;
    }
    // Open container for array of strings
    res = sd_bus_message_open_container(message, 'a', "s");
    if (res < 0) {
      _WARN(
        "[Blight::setFlags()::open_container()] Error: %s", std::strerror(-res)
      );
      sd_bus_message_unref(message);
      return false;
    }
    for (auto& flag : flags) {
      res = sd_bus_message_append(message, "s", flag.c_str());
      if (res < 0) {
        _WARN("[Blight::setFlags()::append(s)] Error: %s", std::strerror(-res));
        sd_bus_message_unref(message);
        return false;
      }
    }
    res = sd_bus_message_close_container(message);
    if (res < 0) {
      _WARN(
        "[Blight::setFlags()::close_container()] Error: %s", std::strerror(-res)
      );
      sd_bus_message_unref(message);
      return false;
    }
    sd_bus_error error = SD_BUS_ERROR_NULL;
    res = sd_bus_call(dbus->bus(), message, 0, &error, NULL);
    sd_bus_message_unref(message);
    if (res < 0) {
      _WARN(
        "[Blight::setFlags()::call()] Error: %s",
        error.message ? error.message : std::strerror(-res)
      );
      sd_bus_error_free(&error);
      return false;
    }
    sd_bus_error_free(&error);
    return true;
  }

  bool connectionExists(std::string identifier) {
    if (!exists()) {
      errno = EAGAIN;
      return -1;
    }
    _DEBUG("[Blight::connectionExists(%s)]", identifier.c_str());
    auto reply = dbus->call_method(
      BLIGHT_SERVICE,
      "/",
      BLIGHT_INTERFACE,
      "connectionExists",
      "s",
      identifier.c_str()
    );
    if (reply->isError()) {
      _WARN(
        "[Blight::connectionExists(%s)::call_method(...)] Error: %s",
        identifier.c_str(),
        reply->error_message().c_str()
      );
      return reply->return_value;
    }
    auto res = reply->read_value<int>("b");
    if (!res.has_value()) {
      _WARN(
        "[Blight::connectionExists(%s)::read_value(\"h\")] Error: %s",
        identifier.c_str(),
        reply->error_message().c_str()
      );
      return false;
    }
    return res.value();
  }
} // namespace Blight
