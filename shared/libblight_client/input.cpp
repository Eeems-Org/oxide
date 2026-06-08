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
#include <mutex>
#include <stdexcept>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <thread>

#include <cstddef>
#include <cstdio>

namespace Input {
  std::map<int, Blight::EvdevRingBuffer> ringBuffers{};
  std::map<int, DeviceInfo> deviceDescriptors{};
  std::map<unsigned int, int> deviceEventFds{};
  std::map<int, std::map<int, struct epoll_event>> epollMap{};
  std::mutex mutex{};

  static void drainEventFd(int eventFd) {
    uint64_t val;
    Libc::read(eventFd, &val, sizeof(val));
  }

  int getEventFd(int fd) {
    if (!deviceDescriptors.contains(fd)) {
      return -1;
    }
    auto info = deviceDescriptors[fd];
    if (!deviceEventFds.contains(info.device)) {
      return -1;
    }
    return deviceEventFds[info.device];
  }

  int getInputFdByEventFd(int eventFd) {
    for (auto& [device, _eventFd] : deviceEventFds) {
      if (_eventFd != eventFd) {
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
      Blight::EvdevRingBuffer* ringBuf = nullptr;
      {
        std::lock_guard lock{mutex};
        if (!ringBuffers.contains(device)) {
          _INFO("Ignoring event for unopened device: %d", device);
          continue;
        }
        ringBuf = &ringBuffers[device];
      }
      ringBuf->insert(ev);
      {
        std::lock_guard lock{mutex};
        auto it = deviceEventFds.find(static_cast<int>(device));
        if (it != deviceEventFds.end()) {
          uint64_t val = 1;
          Libc::write(it->second, &val, sizeof(val));
        }
      }
    }
    _INFO("%s", "InputWorker quitting");
  }

  bool isInputFd(int fd) {
    std::scoped_lock lock(mutex);
    return deviceDescriptors.contains(fd);
  }

  int open(const std::string& path, int flags) {
    if (FB::connection->input_handle() < 0) {
      _WARN("%s", "Could not open connection input stream");
      errno = EIO;
      return -1;
    }
    std::scoped_lock lock{mutex};
    if (ringBuffers.empty() && Client::isInputEnabled()) {
      _DEBUG("Starting readEvents thread");
      std::thread(readEvents).detach();
    }
    std::string basePath(basename(path.c_str()));
    try {
      unsigned int device = stol(basePath.substr(5));
      if (!deviceEventFds.contains(device)) {
        int eventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (eventFd < 0) {
          _WARN("Failed to create eventfd: %s", std::strerror(errno));
          return -1;
        }
        deviceEventFds[device] = eventFd;
        ringBuffers.try_emplace(device, true);
      }
      int fd = Libc::open(path.c_str(), flags, 0);
      if (fd < 0) {
        return -1;
      }
      deviceDescriptors[fd] = {device, flags};
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

  int close(int fd) {
    _DEBUG("Closing input fd %d", fd);
    {
      std::scoped_lock lock(mutex);
      if (!deviceDescriptors.contains(fd)) {
        return Libc::close(fd);
      }
      unsigned int device = deviceDescriptors[fd].device;
      if (deviceEventFds.contains(device)) {
        int eventFd = deviceEventFds[device];
        for (auto& [epfd, track] : epollMap) {
          Libc::epoll_ctl(epfd, EPOLL_CTL_DEL, eventFd, nullptr);
          track.erase(eventFd);
        }
      }
      deviceDescriptors.erase(fd);
    }
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
    auto& ringBuffer = ringBuffers[device];
    if (ringBuffer.overflowed()) {
      events[count] = {.type = EV_SYN, .code = SYN_DROPPED, .value = 1};
      count++;
    }
    while (count < size / sizeof(input_event)) {
      if (flags & O_NONBLOCK) {
        auto maybe = ringBuffer.take();
        if (!maybe.has_value()) {
          break;
        }
        events[count] = *maybe;
      } else {
        events[count] = ringBuffer.wait_for_values();
      }
      count++;
    }
    if (ringBuffer.empty()) {
      std::scoped_lock lock(mutex);
      if (deviceEventFds.contains(device)) {
        drainEventFd(deviceEventFds[device]);
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
