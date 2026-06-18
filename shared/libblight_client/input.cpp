#include "input.h"
#include "fb.h"
#include "libc.h"
#include "state.h"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <libblight/debug.h>
#include <libblight/socket.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <map>
#include <mutex>
#include <stdexcept>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <thread>

#include <cstddef>
#include <cstdio>

static constexpr int RM1_PEN_WIDTH = 20967;
static constexpr int RM1_PEN_HEIGHT = 15725;
static constexpr int RM1_PEN_PRESSURE = 4095;
static constexpr int RM1_PEN_DISTANCE = 255;
static constexpr int RM1_MIN_TILT_X = -9000;
static constexpr int RM1_MAX_TILT_X = 9000;
static constexpr int RM1_MIN_TILT_Y = -9000;
static constexpr int RM1_MAX_TILT_Y = 9000;

static constexpr int RM1_TOUCH_WIDTH = 767;
static constexpr int RM1_TOUCH_HEIGHT = 1023;
static constexpr int RM1_TOUCH_PRESSURE = 255;
static constexpr int RM1_TOUCH_DISTANCE = 255;
static constexpr int RM1_MIN_ORIENTATION = -127;
static constexpr int RM1_MAX_ORIENTATION = 127;
static constexpr int RM1_TOUCH_MAJOR = 255;
static constexpr int RM1_TOUCH_MINOR = 255;

static constexpr float PI = 3.14159265358979323846f;

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x) ((x) % BITS_PER_LONG)
#define LONG(x) ((x) / BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

namespace Input {
  std::map<int, DeviceMap> deviceDescriptors{};
  std::map<unsigned short, DeviceInfo> devices{};
  std::map<int, std::map<int, struct epoll_event>> epollMap{};
  std::map<unsigned short, unsigned short> deviceMap{};
  std::mutex mutex{};
  Transform penTransform;
  Transform touchTransform;

  DeviceInfo::~DeviceInfo() {
    if (thread != nullptr && thread->joinable()) {
      thread->detach();
    }
  }

  int getEventFd(int fd) {
    if (!deviceDescriptors.contains(fd)) {
      return -1;
    }
    auto info = deviceDescriptors[fd];
    if (!devices.contains(info.device)) {
      return -1;
    }
    return devices[info.device].eventFd;
  }

  int getInputFdByEventFd(int eventFd) {
    for (auto& [device, devInfo] : devices) {
      if (devInfo.eventFd != eventFd) {
        continue;
      }
      for (auto& [inputFd, info] : deviceDescriptors) {
        if (info.device == device) {
          return inputFd;
        }
      }
    }
    return -1;
  }

  inline bool checkBitSet(int fd, int type, int i) {
    unsigned long bit[NBITS(KEY_MAX)] = {0};
    Libc::ioctl(fd, EVIOCGBIT(type, KEY_MAX), bit);
    return test_bit(i, bit);
  }

  InputType classify(int fd) {
    int version;
    if (Libc::ioctl(fd, EVIOCGVERSION, &version)) {
      _WARN("Failed to classify fd %d: %s", fd, std::strerror(errno));
      return Invalid;
    }
    unsigned long bit[EV_MAX / sizeof(unsigned long) + 1] = {0};
    Libc::ioctl(fd, EVIOCGBIT(0, EV_MAX), bit);
    if (test_bit(EV_KEY, bit)) {
      if (checkBitSet(fd, EV_KEY, BTN_STYLUS) && test_bit(EV_ABS, bit)) {
        return Wacom;
      }
      if (checkBitSet(fd, EV_KEY, KEY_POWER)) {
        return Buttons;
      }
    }
    if (checkBitSet(fd, EV_ABS, ABS_MT_SLOT)) {
      return Touch;
    }
    return Unknown;
  }

  void getAbsInfo(int fd, DeviceInfo& info) {
    switch (info.type) {
      case Wacom: {
        input_absinfo absInfo = {0};
        if (Libc::ioctl(fd, EVIOCGABS(ABS_X), &absInfo) >= 0) {
          info.minimums.x = absInfo.minimum;
          info.maximums.x = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_X range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_Y), &absInfo) >= 0) {
          info.minimums.y = absInfo.minimum;
          info.maximums.y = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_Y range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_PRESSURE), &absInfo) >= 0) {
          info.minimums.pressure = absInfo.minimum;
          info.maximums.pressure = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_PRESSURE range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_DISTANCE), &absInfo) >= 0) {
          info.minimums.distance = absInfo.minimum;
          info.maximums.distance = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_DISTANCE range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_TILT_X), &absInfo) >= 0) {
          info.minimums.tiltX = absInfo.minimum;
          info.maximums.tiltX = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_DISTANCE range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_TILT_Y), &absInfo) >= 0) {
          info.minimums.tiltY = absInfo.minimum;
          info.maximums.tiltY = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_DISTANCE range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        break;
      }
      case Touch: {
        input_absinfo absInfo = {0};
        if (Libc::ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &absInfo) >= 0) {
          info.minimums.x = absInfo.minimum;
          info.maximums.x = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_MT_POSITION_X range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &absInfo) >= 0) {
          info.minimums.y = absInfo.minimum;
          info.maximums.y = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_MT_POSITION_Y range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_MT_ORIENTATION), &absInfo) >= 0) {
          info.minimums.orientation = absInfo.minimum;
          info.maximums.orientation = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_MT_POSITION_Y range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_MT_TOUCH_MAJOR), &absInfo) >= 0) {
          info.minimums.major = absInfo.minimum;
          info.maximums.major = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_MT_POSITION_Y range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        if (Libc::ioctl(fd, EVIOCGABS(ABS_MT_TOUCH_MINOR), &absInfo) >= 0) {
          info.minimums.minor = absInfo.minimum;
          info.maximums.minor = absInfo.maximum;
        } else {
          _WARN(
            "Unable to get ABS_MT_POSITION_Y range for event%d: %s",
            info.device,
            std::strerror(errno)
          );
        }
        break;
      }
      default:
        break;
    }
  }
  const std::string typeName(InputType type) {
    switch (type) {
      case Unknown:
        return "Unknown";
      case Touch:
        return "Touch";
      case Wacom:
        return "Wacom";
      case Buttons:
        return "Buttons";
      default:
        return "Invalid";
    }
  }

  bool init() {
    _DEBUG("Initializing input");
    if (!Client::isFakeRM1Input()) {
      _DEBUG("Not faking rM1 input, continuing");
      return true;
    }
    penTransform = {.invertX = false, .invertY = false, .rotation = 0.0f};
    switch (Client::deviceType) {
      case Client::DeviceType::RM1:
        touchTransform = {.invertX = false, .invertY = false, .rotation = 0.0f};
        break;
      case Client::DeviceType::RM2:
        touchTransform = {.invertX = true, .invertY = false, .rotation = 0.0f};
        break;
      case Client::DeviceType::RMPP:
        touchTransform = {
          .invertX = false, .invertY = false, .rotation = 180.0f
        };
        break;
      case Client::DeviceType::RMPPM:
        touchTransform = {
          .invertX = false, .invertY = false, .rotation = 180.0f
        };
        break;
      case Client::DeviceType::RMPPURE:
        touchTransform = {
          .invertX = false, .invertY = false, .rotation = 180.0f
        };
        break;
      default:
        _WARN("Unknown device type, unable to force rM1 input")
        return false;
    }
    _DEBUG(
      "pen: invertX=%s, invertY=%s, rotation=%f",
      penTransform.invertX ? "true" : "false",
      penTransform.invertY ? "true" : "false",
      penTransform.rotation
    );
    _DEBUG(
      "touch: invertX=%s, invertY=%s, rotation=%f",
      touchTransform.invertX ? "true" : "false",
      touchTransform.invertY ? "true" : "false",
      touchTransform.rotation
    );
    _DEBUG("Remapping input devices");
    DIR* dir = opendir("/dev/input");
    if (dir == nullptr) {
      _WARN("Unable to open /dev/input: %s", std::strerror(errno));
      return false;
    }
    dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      std::string name(entry->d_name);
      if (name.rfind("event", 0) != 0) {
        continue;
      }
      std::string path = "/dev/input/" + name;
      int fd = Libc::open(path.c_str(), O_RDONLY);
      if (fd < 0) {
        _WARN("Failed to open %s: %s", name.c_str(), std::strerror(errno));
        continue;
      }
      InputType type = classify(fd);
      unsigned short device;
      switch (type) {
        case Wacom:
          device = 0;
          break;
        case Buttons:
          device = 1;
          break;
        case Touch:
          device = 2;
          break;
        default:
          _WARN(
            "Incorrect type for %s: %s", name.c_str(), typeName(type).c_str()
          );
          Libc::close(fd);
          continue;
      }
      if (devices.contains(device)) {
        Libc::close(fd);
        _WARN("Already mapped: %s", name.c_str());
        continue;
      }
      unsigned short actualDevice;
      try {
        actualDevice = static_cast<unsigned short>(std::stoi(name.substr(5)));
      } catch (std::invalid_argument&) {
        _WARN("Resolved event device name invalid: %s", name.c_str());
        continue;
      } catch (std::out_of_range&) {
        _WARN("Resolved event device number out of range: %s", name.c_str());
        continue;
      }
      deviceMap[actualDevice] = device;
      _DEBUG(
        "event%d -> event%d: %s", actualDevice, device, typeName(type).c_str()
      );
      devices[device] = {
        .device = device,
        .fd = fd,
        .eventFd = -1,
        .type = type,
        .state = {},
        .minimums = {},
        .maximums = {},
        .ringBuffer = nullptr,
        .remapBuffer = nullptr,
        .thread = nullptr
      };
      auto& info = devices[device];
      getAbsInfo(fd, info);
      if (info.type == Wacom || info.type == Touch) {
        _DEBUG("x:  min=%d, max=%d", info.minimums.x, info.maximums.x);
        _DEBUG("y:  min=%d, max=%d", info.minimums.y, info.maximums.y);
        _DEBUG(
          "pressure:  min=%d, max=%d",
          info.minimums.pressure,
          info.maximums.pressure
        );
      }
      if (info.type == Wacom) {
        _DEBUG(
          "distance:  min=%d, max=%d",
          info.minimums.distance,
          info.maximums.distance
        );
      }
    }
    closedir(dir);
    return true;
  }

  void readEvents() {
    _INFO("%s", "InputWorker starting");
    prctl(PR_SET_NAME, "InputWorker\0", 0, 0, 0);
    nice(-10);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    auto fd = FB::connection->input_handle();
    bool allow_power_button = Client::isPowerButtonEnabled();
    while (fd > 0 && FB::connection != nullptr) {
      if (!Client::isInputEnabled()) {
        _INFO("Input handling disabled");
        break;
      }
      auto maybe = FB::connection->read_event();
      if (!maybe.has_value()) {
        if (errno != EAGAIN && errno != EINTR) {
          _WARN(
            "[InputWorker] Failed to read message %s", std::strerror(errno)
          );
          break;
        }
        if (!Blight::wait_for_read(fd, -1) && errno != EAGAIN) {
          _WARN(
            "[InputWorker] Failed to wait for next input event %s",
            std::strerror(errno)
          );
          break;
        }
        continue;
      }
      unsigned int rawDevice = maybe.value().device;
      unsigned int device = rawDevice;
      auto& partial = maybe.value().event;
      input_event ev{};
      ev.type = partial.type;
      ev.code = partial.code;
      ev.value = (!allow_power_button && partial.type == EV_KEY &&
                  partial.code == KEY_POWER)
                   ? 0
                   : partial.value;
      std::shared_ptr<Blight::EvdevRingBuffer> ringBuffer;
      {
        std::lock_guard lock{mutex};
        if (Client::isFakeRM1Input()) {
          auto it = deviceMap.find(static_cast<unsigned short>(rawDevice));
          if (it != deviceMap.end()) {
            device = it->second;
          }
        }
        if (!devices.contains(device)) {
          _INFO(
            "Ignoring event for unopened device: raw=%d mapped=%d",
            rawDevice,
            device
          );
          continue;
        }
        ringBuffer = Client::isFakeRM1Input() ? devices[device].remapBuffer
                                              : devices[device].ringBuffer;
      }
      if (ringBuffer != nullptr) {
        ringBuffer->insert(ev);
      }
      if (Client::isFakeRM1Input()) {
        continue;
      }
      {
        std::lock_guard lock{mutex};
        auto it = devices.find(device);
        if (it != devices.end()) {
          uint64_t val = 1;
          Libc::write(it->second.eventFd, &val, sizeof(val));
        }
      }
    }
    _INFO("%s", "InputWorker quitting");
  }

  inline int range(int max, int min) {
    int range = max - min;
    if (range <= 0) {
      return 1;
    }
    return range;
  }

  void
  applyTransform(const DeviceInfo& info, input_event* outX, input_event* outY) {
    int targetWidth;
    int targetHeight;
    Transform transform;
    switch (info.type) {
      case Wacom:
        targetWidth = RM1_PEN_WIDTH;
        targetHeight = RM1_PEN_HEIGHT;
        transform = penTransform;
        break;
      case Touch:
        targetWidth = RM1_TOUCH_WIDTH;
        targetHeight = RM1_TOUCH_HEIGHT;
        transform = touchTransform;
        break;
      default:
        return;
    }
    int widthRange = range(info.maximums.x, info.minimums.x);
    int heightRange = range(info.maximums.y, info.minimums.y);
    float xNorm = static_cast<float>(info.state.x - info.minimums.x) /
                  static_cast<float>(widthRange);
    float yNorm = static_cast<float>(info.state.y - info.minimums.y) /
                  static_cast<float>(heightRange);
    if (FB::visibleYRatio < 1.0f) {
      if (yNorm > FB::visibleYRatio) {
        if (outX != nullptr) {
          outX->value = -1;
        }
        if (outY != nullptr) {
          outY->value = -1;
        }
        return;
      }
      yNorm /= FB::visibleYRatio;
    }
    if (FB::visibleXRatio < 1.0f) {
      if (xNorm > FB::visibleXRatio) {
        if (outX != nullptr) {
          outX->value = -1;
        }
        if (outY != nullptr) {
          outY->value = -1;
        }
        return;
      }
      xNorm /= FB::visibleXRatio;
    }
    switch (static_cast<int>(transform.rotation + 0.5f) % 360) {
      case 0:
        if (transform.invertX) {
          xNorm = 1.0f - xNorm;
        }
        if (transform.invertY) {
          yNorm = 1.0f - yNorm;
        }
        if (outX != nullptr) {
          outX->value = static_cast<int>(xNorm * targetWidth + 0.5f);
        }
        if (outY != nullptr) {
          outY->value = static_cast<int>(yNorm * targetHeight + 0.5f);
        }
        break;
      case 90: {
        float origX = xNorm;
        xNorm = 1.0f - yNorm;
        yNorm = origX;
        if (transform.invertX) {
          xNorm = 1.0f - xNorm;
        }
        if (transform.invertY) {
          yNorm = 1.0f - yNorm;
        }
        if (outX != nullptr) {
          outX->value = static_cast<int>(xNorm * targetWidth + 0.5f);
        }
        if (outY != nullptr) {
          outY->value = static_cast<int>(yNorm * targetHeight + 0.5f);
        }
        break;
      }
      case 180:
        xNorm = 1.0f - xNorm;
        yNorm = 1.0f - yNorm;
        if (transform.invertX) {
          xNorm = 1.0f - xNorm;
        }
        if (transform.invertY) {
          yNorm = 1.0f - yNorm;
        }
        if (outX != nullptr) {
          outX->value = static_cast<int>(xNorm * targetWidth + 0.5f);
        }
        if (outY != nullptr) {
          outY->value = static_cast<int>(yNorm * targetHeight + 0.5f);
        }
        break;
      case 270: {
        float origX = xNorm;
        xNorm = yNorm;
        yNorm = 1.0f - origX;
        if (transform.invertX) {
          xNorm = 1.0f - xNorm;
        }
        if (transform.invertY) {
          yNorm = 1.0f - yNorm;
        }
        if (outX != nullptr) {
          outX->value = static_cast<int>(xNorm * targetWidth + 0.5f);
        }
        if (outY != nullptr) {
          outY->value = static_cast<int>(yNorm * targetHeight + 0.5f);
        }
        break;
      }
      default: {
        float angleRadius = transform.rotation * PI / 180.0f;
        float xScaled = xNorm * static_cast<float>(targetWidth);
        float yScaled = yNorm * static_cast<float>(targetHeight);
        if (outX != nullptr) {
          outX->value = static_cast<int>(
            xScaled * cosf(angleRadius) - yScaled * sinf(angleRadius) + 0.5f
          );
          if (transform.invertX) {
            outX->value = targetWidth - outX->value;
          }
        }
        if (outY != nullptr) {
          outY->value = static_cast<int>(
            xScaled * sinf(angleRadius) + yScaled * cosf(angleRadius) + 0.5f
          );
          if (transform.invertY) {
            outY->value = targetHeight - outY->value;
          }
        }
        break;
      }
    }
  }
  inline int scaleValue(int value, int minimum, int maximum) {
    return (value - minimum) * maximum / range(maximum, minimum);
  }

  void remapEvents(unsigned short device) {
    char name[16];
    snprintf(name, sizeof(name), "Remap[%u]", device);
    _INFO("%s starting", name);
    prctl(PR_SET_NAME, name, 0, 0, 0);
    auto& info = devices[device];
    std::vector<input_event> pending;
    while (FB::connection != nullptr) {
      if (info.remapBuffer->overflowed()) {
        info.remapBuffer->take();
        input_event syn{};
        syn.type = EV_SYN;
        syn.code = SYN_DROPPED;
        syn.value = 1;
        info.ringBuffer->insert(syn);
        uint64_t val = 1;
        Libc::write(info.eventFd, &val, sizeof(val));
        pending.clear();
      }
      input_event event = info.remapBuffer->wait_for_values();
      if (info.type != Wacom && info.type != Touch) {
        info.ringBuffer->insert(event);
        uint64_t val = 1;
        Libc::write(info.eventFd, &val, sizeof(val));
        continue;
      }
      if (event.type == EV_SYN && event.code == SYN_DROPPED) {
        pending.clear();
        continue;
      }
      pending.push_back(event);
      if (!(event.type == EV_SYN && event.code == SYN_REPORT)) {
        continue;
      }
      switch (info.type) {
        case Wacom: {
          input_event* xEvent = nullptr;
          input_event* yEvent = nullptr;
          for (auto& previousEvent : pending) {
            if (previousEvent.type == EV_ABS) {
              switch (previousEvent.code) {
                case ABS_PRESSURE:
                  info.state.pressure = previousEvent.value;
                  previousEvent.value = scaleValue(
                    previousEvent.value,
                    info.minimums.pressure,
                    info.maximums.pressure
                  );
                  break;
                case ABS_DISTANCE:
                  info.state.distance = previousEvent.value;
                  previousEvent.value = scaleValue(
                    previousEvent.value,
                    info.minimums.distance,
                    info.maximums.distance
                  );
                  break;
                case ABS_TILT_X:
                  info.state.tiltX = previousEvent.value;
                  previousEvent.value = scaleValue(
                    previousEvent.value,
                    info.minimums.tiltX,
                    info.maximums.tiltX
                  );
                  break;
                case ABS_TILT_Y:
                  info.state.tiltY = previousEvent.value;
                  previousEvent.value = scaleValue(
                    previousEvent.value,
                    info.minimums.tiltY,
                    info.maximums.tiltY
                  );
                  break;
                case ABS_X:
                  info.state.x = previousEvent.value;
                  xEvent = &previousEvent;
                  break;
                case ABS_Y:
                  info.state.y = previousEvent.value;
                  yEvent = &previousEvent;
                  break;
                default:
                  break;
              }
            }
          }
          if (xEvent == nullptr && yEvent == nullptr) {
            break;
          }
          applyTransform(info, xEvent, yEvent);
          _DEBUG(
            "pen (%d,  %d) -> (%d, %d)",
            info.state.x,
            info.state.y,
            xEvent == nullptr ? info.state.x : xEvent->value,
            yEvent == nullptr ? info.state.y : yEvent->value
          );
          if (xEvent != nullptr) {
            info.state.x = xEvent->value;
          }
          if (yEvent != nullptr) {
            info.state.y = yEvent->value;
          }
          break;
        }
        case Touch: {
          input_event* xEvent = nullptr;
          input_event* yEvent = nullptr;
          for (auto& previousEvent : pending) {
            if (previousEvent.type != EV_ABS) {
              continue;
            }
            switch (previousEvent.code) {
              case ABS_MT_PRESSURE:
                info.state.pressure = previousEvent.value;
                previousEvent.value = scaleValue(
                  previousEvent.value,
                  info.minimums.pressure,
                  info.maximums.pressure
                );
                break;
              case ABS_MT_DISTANCE:
                info.state.distance = previousEvent.value;
                previousEvent.value = scaleValue(
                  previousEvent.value,
                  info.minimums.distance,
                  info.maximums.distance
                );
                break;
              case ABS_MT_ORIENTATION:
                info.state.orientation = previousEvent.value;
                previousEvent.value = scaleValue(
                  previousEvent.value,
                  info.minimums.orientation,
                  info.maximums.orientation
                );
                break;
              case ABS_MT_TOUCH_MAJOR:
                info.state.major = previousEvent.value;
                previousEvent.value = scaleValue(
                  previousEvent.value, info.minimums.major, info.maximums.major
                );
                break;
              case ABS_MT_TOUCH_MINOR:
                info.state.minor = previousEvent.value;
                previousEvent.value = scaleValue(
                  previousEvent.value, info.minimums.minor, info.maximums.minor
                );
                break;
              case ABS_MT_POSITION_X:
                info.state.x = previousEvent.value;
                xEvent = &previousEvent;
                break;
              case ABS_MT_POSITION_Y:
                info.state.y = previousEvent.value;
                yEvent = &previousEvent;
                break;
            }
          }
          if (xEvent == nullptr && yEvent == nullptr) {
            break;
          }
          applyTransform(info, xEvent, yEvent);
          _DEBUG(
            "touch (%d,  %d) -> (%d, %d)",
            info.state.x,
            info.state.y,
            xEvent == nullptr ? info.state.x : xEvent->value,
            yEvent == nullptr ? info.state.y : yEvent->value
          );
          if (xEvent != nullptr) {
            info.state.x = xEvent->value;
          }
          if (yEvent != nullptr) {
            info.state.y = yEvent->value;
          }
          break;
        }
        default:
          break;
      }
      uint64_t val = 1;
      for (auto& pendingEvent : pending) {
        if (
          pendingEvent.type == EV_ABS &&
          (pendingEvent.code == ABS_X || pendingEvent.code == ABS_Y ||
           pendingEvent.code == ABS_MT_POSITION_X ||
           pendingEvent.code == ABS_MT_POSITION_Y) &&
          pendingEvent.value == -1
        ) {
          continue;
        }
        info.ringBuffer->insert(pendingEvent);
        Libc::write(info.eventFd, &val, sizeof(val));
      }
      pending.clear();
    }
  }

  bool isInputFd(int fd) {
    std::scoped_lock lock(mutex);
    return deviceDescriptors.contains(fd);
  }

  int open(const std::string& path, int flags) {
    static std::atomic<bool> started = false;
    if (!started) {
      static std::mutex mutex{};
      if (mutex.try_lock()) {
        FB::connection->input_handle();
        _DEBUG("Starting readEvents thread");
        std::thread(readEvents).detach();
        started = true;
        mutex.unlock();
      } else {
        mutex.lock();
        mutex.unlock();
      }
    }
    if (FB::connection->input_handle() < 0) {
      _WARN("%s", "Could not open connection input stream");
      errno = EIO;
      return -1;
    }
    std::scoped_lock lock{mutex};
    std::string basePath(basename(path.c_str()));
    unsigned short device;
    try {
      device = static_cast<unsigned short>(stoi(basePath.substr(5)));
    } catch (std::invalid_argument&) {
      _WARN("Resolved event device name invalid: %s", basePath.c_str());
      errno = EIO;
      return -1;
    } catch (std::out_of_range&) {
      _WARN("Resolved event device number out of range: %s", basePath.c_str());
      errno = EIO;
      return -1;
    }
    int fd = Libc::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
      return -1;
    }
    if (!devices.contains(device)) {
      InputType type = classify(fd);
      devices[device] = {
        .device = device,
        .fd = fd,
        .eventFd = -1,
        .type = type,
        .state = {},
        .minimums = {},
        .maximums = {},
        .ringBuffer = nullptr,
        .remapBuffer = nullptr,
        .thread = nullptr
      };
      auto& info = devices[device];
      getAbsInfo(fd, info);
      _DEBUG("event%d: %s", device, typeName(info.type).c_str());
      _DEBUG("x:  min=%d, max=%d", info.minimums.x, info.maximums.x);
      _DEBUG("y:  min=%d, max=%d", info.minimums.y, info.maximums.y);
      _DEBUG(
        "pressure:  min=%d, max=%d",
        info.minimums.pressure,
        info.maximums.pressure
      );
      _DEBUG(
        "distance:  min=%d, max=%d",
        info.minimums.distance,
        info.maximums.distance
      );
    }
    if (devices[device].ringBuffer == nullptr) {
      devices[device].ringBuffer =
        std::make_shared<Blight::EvdevRingBuffer>(true);
    }
    if (Client::isFakeRM1Input() && devices[device].remapBuffer == nullptr) {
      devices[device].remapBuffer =
        std::make_shared<Blight::EvdevRingBuffer>(true);
    }
    if (Client::isFakeRM1Input() && devices[device].thread == nullptr) {
      devices[device].thread =
        std::make_shared<std::thread>(remapEvents, device);
    }
    if (devices[device].eventFd == -1) {
      int eventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
      if (eventFd < 0) {
        _WARN("Failed to create eventfd: %s", std::strerror(errno));
        return -1;
      }
      devices[device].eventFd = eventFd;
    }
    deviceDescriptors[fd] = {device, flags};
    return fd;
  }

  int close(int fd) {
    _DEBUG("Closing input fd %d", fd);
    {
      std::scoped_lock lock(mutex);
      if (!deviceDescriptors.contains(fd)) {
        return Libc::close(fd);
      }
      unsigned int device = deviceDescriptors[fd].device;
      if (devices.contains(device)) {
        int eventFd = devices[device].eventFd;
        for (auto& [epfd, track] : epollMap) {
          Libc::epoll_ctl(epfd, EPOLL_CTL_DEL, eventFd, nullptr);
          track.erase(eventFd);
        }
      }
      deviceDescriptors.erase(fd);
    }
    return Libc::close(fd);
  }

  inline int
  rm1AbsInfo(int fd, unsigned long request, char* ptr, int min, int max) {
    auto abs = reinterpret_cast<struct input_absinfo*>(ptr);
    memset(abs, 0, sizeof(*abs));
    Libc::ioctl(fd, request, ptr);
    abs->minimum = min;
    abs->maximum = max;
    abs->value = scaleValue(abs->value, min, max);
    return 0;
  }

  int ioctlv(int fd, unsigned long request, char* ptr) {
    _DEBUG("input ioctl %d %lu", fd, request);
    if (request == EVIOCGRAB || request == EVIOCREVOKE) {
      return 0;
    }
    if (!Client::isFakeRM1Input()) {
      return Libc::ioctl(fd, request, ptr);
    }
    InputType type = Unknown;
    unsigned short device;
    {
      std::scoped_lock lock{mutex};
      if (!deviceDescriptors.contains(fd)) {
        errno = EBADF;
        return -1;
      }
      device = deviceDescriptors[fd].device;
      if (!devices.contains(device)) {
        errno = EBADF;
        return -1;
      }
      type = devices[device].type;
      fd = devices[device].fd;
    }
    if (EVIOCGNAME(0) == (request & ~(0x3FFFUL << 16))) {
      size_t bufSize = (request >> 16) & 0x3FFF;
      const char* name = nullptr;
      switch (device) {
        case 0:
          name = "Wacom I2C Digitizer";
          break;
        case 1:
          name = "gpio-keys";
          break;
        case 2:
          name = "cyttsp5_mt";
          break;
        default:
          return Libc::ioctl(fd, request, ptr);
      }
      size_t nameLen = strlen(name);
      size_t copyLen = nameLen;
      if (bufSize > 0 && copyLen >= bufSize) {
        copyLen = bufSize - 1;
      }
      memcpy(ptr, name, copyLen);
      ptr[copyLen] = '\0';
      return 0;
    }
    switch (device) {
      case 0:
        if (EVIOCGBIT(0, 0) == (request & ~(0x3FFFUL << 16))) {
          size_t len = (request >> 16) & 0x3FFF;
          memset(ptr, 0, len);
          auto bits = reinterpret_cast<unsigned long*>(ptr);
          bits[LONG(EV_ABS)] |= (1UL << OFF(EV_ABS));
          bits[LONG(EV_KEY)] |= (1UL << OFF(EV_KEY));
          bits[LONG(EV_SYN)] |= (1UL << OFF(EV_SYN));
          return 0;
        }
        if (EVIOCGBIT(EV_ABS, 0) == (request & ~(0x3FFFUL << 16))) {
          size_t len = (request >> 16) & 0x3FFF;
          memset(ptr, 0, len);
          auto bits = reinterpret_cast<unsigned long*>(ptr);
          bits[LONG(ABS_X)] |= (1UL << OFF(ABS_X));
          bits[LONG(ABS_Y)] |= (1UL << OFF(ABS_Y));
          bits[LONG(ABS_PRESSURE)] |= (1UL << OFF(ABS_PRESSURE));
          bits[LONG(ABS_DISTANCE)] |= (1UL << OFF(ABS_DISTANCE));
          bits[LONG(ABS_TILT_X)] |= (1UL << OFF(ABS_TILT_X));
          bits[LONG(ABS_TILT_Y)] |= (1UL << OFF(ABS_TILT_Y));
          return 0;
        }
        if (EVIOCGBIT(EV_KEY, 0) == (request & ~(0x3FFFUL << 16))) {
          size_t len = (request >> 16) & 0x3FFF;
          memset(ptr, 0, len);
          auto bits = reinterpret_cast<unsigned long*>(ptr);
          bits[LONG(BTN_TOOL_PEN)] |= (1UL << OFF(BTN_TOOL_PEN));
          bits[LONG(BTN_TOOL_RUBBER)] |= (1UL << OFF(BTN_TOOL_RUBBER));
          bits[LONG(BTN_TOUCH)] |= (1UL << OFF(BTN_TOUCH));
          bits[LONG(BTN_STYLUS)] |= (1UL << OFF(BTN_STYLUS));
          bits[LONG(BTN_STYLUS2)] |= (1UL << OFF(BTN_STYLUS2));
          return 0;
        }
        switch (request) {
          case EVIOCGABS(ABS_X):
            return rm1AbsInfo(fd, request, ptr, 0, RM1_PEN_WIDTH);
          case EVIOCGABS(ABS_Y):
            return rm1AbsInfo(fd, request, ptr, 0, RM1_PEN_HEIGHT);
          case EVIOCGABS(ABS_PRESSURE):
            return rm1AbsInfo(fd, request, ptr, 0, RM1_PEN_PRESSURE);
          case EVIOCGABS(ABS_DISTANCE):
            return rm1AbsInfo(fd, request, ptr, 0, RM1_PEN_DISTANCE);
          case EVIOCGABS(ABS_TILT_X):
            return rm1AbsInfo(fd, request, ptr, RM1_MIN_TILT_X, RM1_MAX_TILT_X);
          case EVIOCGABS(ABS_TILT_Y):
            return rm1AbsInfo(fd, request, ptr, RM1_MIN_TILT_Y, RM1_MAX_TILT_Y);
          default:
            break;
        }
        break;
      case 1:
        if (EVIOCGBIT(0, 0) == (request & ~(0x3FFFUL << 16))) {
          size_t len = (request >> 16) & 0x3FFF;
          memset(ptr, 0, len);
          auto bits = reinterpret_cast<unsigned long*>(ptr);
          bits[LONG(EV_KEY)] |= (1UL << OFF(EV_KEY));
          bits[LONG(EV_SYN)] |= (1UL << OFF(EV_SYN));
          return 0;
        }
        if (EVIOCGBIT(EV_KEY, 0) == (request & ~(0x3FFFUL << 16))) {
          size_t len = (request >> 16) & 0x3FFF;
          memset(ptr, 0, len);
          auto bits = reinterpret_cast<unsigned long*>(ptr);
          bits[LONG(KEY_POWER)] |= (1UL << OFF(KEY_POWER));
          bits[LONG(KEY_LEFT)] |= (1UL << OFF(KEY_LEFT));
          bits[LONG(KEY_RIGHT)] |= (1UL << OFF(KEY_RIGHT));
          bits[LONG(KEY_HOME)] |= (1UL << OFF(KEY_HOME));
          bits[LONG(KEY_WAKEUP)] |= (1UL << OFF(KEY_WAKEUP));
          return 0;
        }
        break;
      case 2:
        if (EVIOCGBIT(0, 0) == (request & ~(0x3FFFUL << 16))) {
          size_t len = (request >> 16) & 0x3FFF;
          memset(ptr, 0, len);
          auto bits = reinterpret_cast<unsigned long*>(ptr);
          bits[LONG(EV_ABS)] |= (1UL << OFF(EV_ABS));
          bits[LONG(EV_REL)] |= (1UL << OFF(EV_REL));
          bits[LONG(EV_KEY)] |= (1UL << OFF(EV_KEY));
          bits[LONG(EV_SYN)] |= (1UL << OFF(EV_SYN));
          return 0;
        }
        if (EVIOCGBIT(EV_ABS, 0) == (request & ~(0x3FFFUL << 16))) {
          size_t len = (request >> 16) & 0x3FFF;
          memset(ptr, 0, len);
          auto bits = reinterpret_cast<unsigned long*>(ptr);
          bits[LONG(ABS_MT_DISTANCE)] |= (1UL << OFF(ABS_MT_DISTANCE));
          bits[LONG(ABS_MT_SLOT)] |= (1UL << OFF(ABS_MT_SLOT));
          bits[LONG(ABS_MT_TOUCH_MAJOR)] |= (1UL << OFF(ABS_MT_TOUCH_MAJOR));
          bits[LONG(ABS_MT_TOUCH_MINOR)] |= (1UL << OFF(ABS_MT_TOUCH_MINOR));
          bits[LONG(ABS_MT_ORIENTATION)] |= (1UL << OFF(ABS_MT_ORIENTATION));
          bits[LONG(ABS_MT_POSITION_X)] |= (1UL << OFF(ABS_MT_POSITION_X));
          bits[LONG(ABS_MT_POSITION_Y)] |= (1UL << OFF(ABS_MT_POSITION_Y));
          bits[LONG(ABS_MT_TOOL_TYPE)] |= (1UL << OFF(ABS_MT_TOOL_TYPE));
          bits[LONG(ABS_MT_TRACKING_ID)] |= (1UL << OFF(ABS_MT_TRACKING_ID));
          bits[LONG(ABS_MT_PRESSURE)] |= (1UL << OFF(ABS_MT_PRESSURE));
          return 0;
        }
        if (
          EVIOCGBIT(EV_KEY, 0) == (request & ~(0x3FFFUL << 16)) ||
          EVIOCGBIT(EV_REL, 0) == (request & ~(0x3FFFUL << 16)) ||
          EVIOCGBIT(EV_SYN, 0) == (request & ~(0x3FFFUL << 16))
        ) {
          size_t len = (request >> 16) & 0x3FFF;
          memset(ptr, 0, len);
          auto bits = reinterpret_cast<unsigned long*>(ptr);
          return 0;
        }
        switch (request) {
          case EVIOCGABS(ABS_MT_POSITION_X):
            return rm1AbsInfo(fd, request, ptr, 0, RM1_TOUCH_WIDTH);
          case EVIOCGABS(ABS_MT_POSITION_Y):
            return rm1AbsInfo(fd, request, ptr, 0, RM1_TOUCH_HEIGHT);
          case EVIOCGABS(ABS_MT_PRESSURE):
            return rm1AbsInfo(fd, request, ptr, 0, RM1_TOUCH_PRESSURE);
          case EVIOCGABS(ABS_MT_DISTANCE):
            return rm1AbsInfo(fd, request, ptr, 0, RM1_TOUCH_DISTANCE);
          case EVIOCGABS(ABS_MT_ORIENTATION):
            return rm1AbsInfo(
              fd, request, ptr, RM1_MIN_ORIENTATION, RM1_MAX_ORIENTATION
            );
          default:
            break;
        }
        break;
    }
    return Libc::ioctl(fd, request, ptr);
  }

  ssize_t read(int fd, void* buf, size_t size) {
    _DEBUG("read %d %d", fd, size);
    if (size < sizeof(input_event)) {
      _WARN("Trying to read too small of an amount from an input fd: %d", size)
      errno = EINVAL;
      return -1;
    }
    int device;
    int flags;
    {
      std::scoped_lock lock(mutex);
      auto& info = deviceDescriptors[fd];
      device = info.device;
      flags = info.flags;
    }
    auto* events = static_cast<input_event*>(buf);
    size_t count = 0;
    auto& ringBuffer = devices[device].ringBuffer;
    if (ringBuffer->overflowed()) {
      ringBuffer->take();
      events[count] = {};
      events[count].type = EV_SYN;
      events[count].code = SYN_DROPPED;
      events[count].value = 1;
      count++;
    }
    while (count < size / sizeof(input_event)) {
      if (flags & O_NONBLOCK) {
        auto maybe = ringBuffer->take();
        if (!maybe.has_value()) {
          break;
        }
        events[count] = *maybe;
      } else {
        events[count] = ringBuffer->wait_for_values();
      }
      uint64_t val;
      Libc::read(devices[device].eventFd, &val, sizeof(val));
      count++;
      if (ringBuffer->empty()) {
        break;
      }
    }
    if (count == 0) {
      errno = EAGAIN;
      return -1;
    }
    return count * sizeof(input_event);
  }

  int fcntl(int fd, int cmd, void* ptr) {
    std::scoped_lock lock(mutex);
    auto& info = deviceDescriptors[fd];
    switch (cmd) {
      case F_GETFD:
        return 0;
      case F_SETFD:
        return 0;
      case F_GETFL:
        return info.flags;
      case F_SETFL: {
        info.flags = static_cast<int>(reinterpret_cast<intptr_t>(ptr));
        return 0;
      }
      default:
        errno = EINVAL;
        return -1;
    }
  }

  struct pollfd* translatePollfds(struct pollfd* fds, nfds_t nfds) {
    std::scoped_lock lock(mutex);
    auto size = sizeof(pollfd) * nfds;
    auto* backup = (pollfd*)malloc(size);
    memcpy(backup, fds, size);
    for (nfds_t i = 0; i < nfds; i++) {
      int fd = getEventFd(fds[i].fd);
      if (fd > 0) {
        fds[i].fd = fd;
      }
    }
    return backup;
  }

  void restorePollfds(struct pollfd* fds, nfds_t nfds, struct pollfd* backup) {
    std::scoped_lock lock(mutex);
    for (nfds_t i = 0; i < nfds; i++) {
      fds[i].fd = backup[i].fd;
    }
    free(backup);
  }

  int translateSelectFds(
    int& nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds
  ) {
    std::scoped_lock lock(mutex);
    int orig_nfds = nfds;
    for (int fd = 0; fd < orig_nfds; fd++) {
      if (readfds && FD_ISSET(fd, readfds) && deviceDescriptors.contains(fd)) {
        FD_CLR(fd, readfds);
        int eventFd = getEventFd(fd);
        FD_SET(eventFd, readfds);
        if (eventFd >= nfds) {
          nfds = eventFd + 1;
        }
      }
      if (
        writefds && FD_ISSET(fd, writefds) && deviceDescriptors.contains(fd)
      ) {
        FD_CLR(fd, writefds);
        int eventFd = getEventFd(fd);
        FD_SET(eventFd, writefds);
        if (eventFd >= nfds) {
          nfds = eventFd + 1;
        }
      }
      if (
        exceptfds && FD_ISSET(fd, exceptfds) && deviceDescriptors.contains(fd)
      ) {
        FD_CLR(fd, exceptfds);
        int eventFd = getEventFd(fd);
        FD_SET(eventFd, exceptfds);
        if (eventFd >= nfds) {
          nfds = eventFd + 1;
        }
      }
    }
    return orig_nfds;
  }

  void restoreSelectFds(
    int nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds,
    int backup
  ) {
    std::scoped_lock lock(mutex);
    for (int fd = 0; fd < nfds; fd++) {
      if (readfds && FD_ISSET(fd, readfds)) {
        int _fd = getInputFdByEventFd(fd);
        if (_fd > 0) {
          FD_CLR(fd, readfds);
          FD_SET(_fd, readfds);
        }
      }
      if (writefds && FD_ISSET(fd, writefds)) {
        int _fd = getInputFdByEventFd(fd);
        if (_fd > 0) {
          FD_CLR(fd, writefds);
          FD_SET(_fd, writefds);
        }
      }
      if (exceptfds && FD_ISSET(fd, exceptfds)) {
        int _fd = getInputFdByEventFd(fd);
        if (_fd > 0) {
          FD_CLR(fd, exceptfds);
          FD_SET(_fd, exceptfds);
        }
      }
    }
    for (int fd = backup; fd < nfds; fd++) {
      if (readfds) {
        FD_CLR(fd, readfds);
      }
      if (writefds) {
        FD_CLR(fd, writefds);
      }
      if (exceptfds) {
        FD_CLR(fd, exceptfds);
      }
    }
  }

  int epoll_ctl(int epfd, int op, int fd, struct epoll_event* ev) {
    int eventFd;
    {
      std::scoped_lock lock(mutex);
      if (!deviceDescriptors.contains(fd)) {
        return Libc::epoll_ctl(epfd, op, fd, ev);
      }
      eventFd = getEventFd(fd);
      if (eventFd < 0) {
        errno = EBADF;
        return -1;
      }
    }
    if (op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) {
      struct epoll_event modified_ev;
      memset(&modified_ev, 0, sizeof(modified_ev));
      modified_ev.events = ev->events;
      modified_ev.data.fd = eventFd;
      {
        std::scoped_lock lock(mutex);
        epollMap[epfd][eventFd] = *ev;
      }
      int res = Libc::epoll_ctl(epfd, op, eventFd, &modified_ev);
      if (res < 0 && op == EPOLL_CTL_ADD && errno == EEXIST) {
        errno = 0;
        return 0;
      }
      return res;
    } else if (op == EPOLL_CTL_DEL) {
      std::scoped_lock lock(mutex);
      epollMap[epfd].erase(eventFd);
      return Libc::epoll_ctl(epfd, op, eventFd, nullptr);
    }
    return Libc::epoll_ctl(epfd, op, fd, ev);
  }

  int restoreEpollfds(int epfd, struct epoll_event* events, int res) {
    std::scoped_lock lock(mutex);
    auto epIt = epollMap.find(epfd);
    if (epIt == epollMap.end()) {
      return res;
    }
    auto& trackByEventFd = epIt->second;
    for (int i = 0; i < res; i++) {
      int eventFd = events[i].data.fd;
      auto it = trackByEventFd.find(eventFd);
      if (it != trackByEventFd.end()) {
        events[i].data = it->second.data;
      }
    }
    return res;
  }
}
