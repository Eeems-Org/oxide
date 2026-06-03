#include "input.h"
#include "fb.h"
#include "libc.h"
#include "state.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <libblight/debug.h>
#include <libblight/socket.h>
#include <linux/input.h>
#include <stdexcept>
#include <sys/select.h>
#include <thread>

namespace Input {
  std::map<int, Blight::EvdevRingBuffer> ringBuffers{};
  std::map<int, std::pair<int, int>> deviceDescriptors{};
  std::mutex mutex{};

  int open(const std::string& path, int flags) {
    if (FB::connection->input_handle() < 0) {
      _WARN("%s", "Could not open connection input stream");
      errno = EIO;
      return -1;
    }
    std::scoped_lock lock{mutex};
    if (ringBuffers.empty() && Client::HANDLE_INPUT) {
      new std::thread(readEvents);
    }
    std::string basePath(basename(path.c_str()));
    try {
      unsigned int device = stol(basePath.substr(5));
      // If device already opened, return existing fd
      if (ringBuffers.contains(device)) {
        int fd = Libc::open(path.c_str(), flags, 0);
        if (fd > 0) {
          deviceDescriptors[fd] = {device, flags};
        }
        return fd;
      }
      // Create ring buffer for this device
      ringBuffers.try_emplace(device);
      int fd = Libc::open(path.c_str(), flags, 0);
      if (fd < 0) {
        ringBuffers.erase(device);
        return -1;
      }
      deviceDescriptors[fd] = {device, flags};
      _DEBUG("Opened input device %d on fd %d", device, fd);
      return fd;
    } catch (std::invalid_argument&) {
      _WARN("Resolved event device name invalid: %s", basePath.c_str());
      errno = EIO;
      return -1;
    } catch (std::out_of_range&) {
      _WARN("Resolved event device number out of range: %s", basePath.c_str());
      errno = EIO;
      return -1;
    }
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
    bool allow_power_button =
      getenv("OXIDE_PRELOAD_ALLOW_POWER_BUTTON") != nullptr;
    while (fd > 0 && FB::connection != nullptr) {
      if (getenv("OXIDE_PRELOAD_EXPOSE_INPUT") != nullptr) {
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
      unsigned int device = maybe.value().device;
      auto& partial = maybe.value().event;
      input_event ev = {
        .type = partial.type,
        .code = partial.code,
        .value = (!allow_power_button && partial.type == EV_KEY &&
                  partial.code == KEY_POWER)
                   ? 0
                   : partial.value
      };
      mutex.lock();
      auto it = ringBuffers.find(device);
      if (it == ringBuffers.end()) {
        _INFO("Ignoring event for unopened device: %d", device);
        mutex.unlock();
        continue;
      }
      it->second.insert(ev);
      mutex.unlock();
    }
    _INFO("%s", "InputWorker quitting");
  }

  int close(int fd) {
    _DEBUG("Closing input fd %d", fd);
    int device = deviceDescriptors[fd].first;
    ringBuffers.erase(device);
    deviceDescriptors.erase(fd);
    return Libc::close(fd);
  }

  int ioctlv(int fd, unsigned long request, char* ptr) {
    _DEBUG("input ioctl %d %lu", fd, request);
    switch (request) {
      case EVIOCGRAB:
        return 0;
      case EVIOCREVOKE:
        return 0;
      default:
        return Libc::ioctl(fd, request, ptr);
    }
  }

  ssize_t read(int fd, void* buf, size_t size) {
    auto& entry = deviceDescriptors[fd];
    auto& ringBuf = ringBuffers[entry.first];
    auto* events = static_cast<input_event*>(buf);
    size_t max_events = size / sizeof(input_event);
    size_t count = 0;
    bool nonblocking = entry.second & O_NONBLOCK;
    if (ringBuf.overflowed()) {
      if (nonblocking) {
        auto maybe = ringBuf.take();
        if (!maybe.has_value()) {
          errno = EAGAIN;
          return -1;
        }
      } else {
        ringBuf.wait_for_values();
      }
      if (count < max_events) {
        events[count] = {.type = EV_SYN, .code = SYN_DROPPED, .value = 1};
        count++;
      }
    }
    if (nonblocking) {
      while (count < max_events) {
        auto maybe = ringBuf.take();
        if (!maybe.has_value()) {
          break;
        }
        events[count] = *maybe;
        count++;
      }
      if (count == 0) {
        errno = EAGAIN;
        return -1;
      }
      return count * sizeof(input_event);
    }
    if (count == 0) {
      events[count] = ringBuf.wait_for_values();
      count++;
    }
    while (count < max_events) {
      auto maybe = ringBuf.take();
      if (!maybe.has_value()) {
        break;
      }
      events[count] = *maybe;
      count++;
    }
    return count * sizeof(input_event);
  }

  int fcntl(int fd, int cmd, void* ptr) {
    switch (cmd) {
      case F_GETFD:
        return 0;
      case F_SETFD:
        return 0;
      case F_GETFL:
        return deviceDescriptors[fd].second;
      case F_SETFL: {
        deviceDescriptors[fd].second =
          static_cast<int>(reinterpret_cast<intptr_t>(ptr));
        return 0;
      }
      default:
        errno = EINVAL;
        return -1;
    }
  }
}
