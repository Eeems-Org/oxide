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
  std::map<int, std::map<int, EpollTrackEntry>> epollMap{};
  std::mutex mutex{};

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
      std::map<int, Blight::EvdevRingBuffer>::iterator it;
      {
        std::lock_guard lock{mutex};
        it = ringBuffers.find(device);
        if (it == ringBuffers.end()) {
          _INFO("Ignoring event for unopened device: %d", device);
          continue;
        }
      }
      it->second.insert(ev);
      {
        std::lock_guard lock{mutex};
        // Signal all eventfds for this device
        for (auto& [inputFd, info] : deviceDescriptors) {
          if (info.device == static_cast<int>(device)) {
            uint64_t val = 1;
            // Non-blocking write; ignore EAGAIN (eventfd already signaled)
            Libc::write(info.event_fd, &val, sizeof(val));
          }
        }
      }
    }
    _INFO("%s", "InputWorker quitting");
  }

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
      // Create eventfd for signaling data availability
      int event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
      if (event_fd < 0) {
        _WARN("Failed to create eventfd: %s", std::strerror(errno));
        return -1;
      }
      // If device already opened, return existing fd
      if (ringBuffers.contains(device)) {
        int fd = Libc::open(path.c_str(), flags, 0);
        if (fd > 0) {
          deviceDescriptors[fd] = {device, flags, event_fd};
        } else {
          Libc::close(event_fd);
        }
        return fd;
      }
      // Create ring buffer for this device
      ringBuffers.try_emplace(device);
      int fd = Libc::open(path.c_str(), flags, 0);
      if (fd < 0) {
        ringBuffers.erase(device);
        Libc::close(event_fd);
        return -1;
      }
      deviceDescriptors[fd] = {device, flags, event_fd};
      _DEBUG(
        "Opened input device %d on fd %d (event_fd %d)", device, fd, event_fd
      );
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
    auto& info = deviceDescriptors[fd];
    int event_fd = info.event_fd;
    int device = info.device;
    {
      std::scoped_lock lock(mutex);
      // Clean up epoll tracking for this fd's eventfd
      for (auto& [epfd, track] : epollMap) {
        track.erase(event_fd);
      }
      // Remove from shared maps BEFORE closing fds, so InputWorker can't
      // race on these entries while we're tearing down.
      deviceDescriptors.erase(fd);
      ringBuffers.erase(device);
    }
    Libc::close(event_fd);
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

  // Drain eventfd after reading from ringbuffer to prevent spurious wakeups
  // on the next epoll_wait/poll/select.  Safe to call even if eventfd has
  // no pending data (returns EAGAIN which is ignored).
  static void drainEventFd(int event_fd) {
    uint64_t val;
    Libc::read(event_fd, &val, sizeof(val));
  }

  ssize_t read(int fd, void* buf, size_t size) {
    auto& info = deviceDescriptors[fd];
    auto& ringBuf = ringBuffers[info.device];
    auto* events = static_cast<input_event*>(buf);
    size_t max_events = size / sizeof(input_event);
    size_t count = 0;
    bool nonblocking = info.flags & O_NONBLOCK;
    if (ringBuf.overflowed()) {
      if (nonblocking) {
        auto maybe = ringBuf.take();
        if (!maybe.has_value()) {
          drainEventFd(info.event_fd);
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
        drainEventFd(info.event_fd);
        errno = EAGAIN;
        return -1;
      }
      drainEventFd(info.event_fd);
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
    drainEventFd(info.event_fd);
    return count * sizeof(input_event);
  }

  int fcntl(int fd, int cmd, void* ptr) {
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

  int getEventFd(int fd) {
    auto it = deviceDescriptors.find(fd);
    if (it == deviceDescriptors.end()) {
      return -1;
    }
    return it->second.event_fd;
  }

  bool isEventFd(int fd) {
    for (auto& [inputFd, info] : deviceDescriptors) {
      if (info.event_fd == fd) {
        return true;
      }
    }
    return false;
  }

  int getInputFdByEventFd(int event_fd) {
    for (auto& [inputFd, info] : deviceDescriptors) {
      if (info.event_fd == event_fd) {
        return inputFd;
      }
    }
    return -1;
  }

  void translatePollfds(struct pollfd* fds, nfds_t nfds) {
    for (nfds_t i = 0; i < nfds; i++) {
      if (deviceDescriptors.contains(fds[i].fd)) {
        fds[i].fd = getEventFd(fds[i].fd);
      }
    }
  }

  void restorePollfds(struct pollfd* fds, nfds_t nfds) {
    for (nfds_t i = 0; i < nfds; i++) {
      if (isEventFd(fds[i].fd)) {
        fds[i].fd = getInputFdByEventFd(fds[i].fd);
      }
    }
  }

  int translateSelectFds(
    int& nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds
  ) {
    int orig_nfds = nfds;
    for (int fd = 0; fd < orig_nfds; fd++) {
      if (readfds && FD_ISSET(fd, readfds) && deviceDescriptors.contains(fd)) {
        FD_CLR(fd, readfds);
        int event_fd = getEventFd(fd);
        FD_SET(event_fd, readfds);
        if (event_fd >= nfds)
          nfds = event_fd + 1;
      }
      if (
        writefds && FD_ISSET(fd, writefds) && deviceDescriptors.contains(fd)
      ) {
        FD_CLR(fd, writefds);
        int event_fd = getEventFd(fd);
        FD_SET(event_fd, writefds);
        if (event_fd >= nfds)
          nfds = event_fd + 1;
      }
      if (
        exceptfds && FD_ISSET(fd, exceptfds) && deviceDescriptors.contains(fd)
      ) {
        FD_CLR(fd, exceptfds);
        int event_fd = getEventFd(fd);
        FD_SET(event_fd, exceptfds);
        if (event_fd >= nfds)
          nfds = event_fd + 1;
      }
    }
    return orig_nfds;
  }

  void restoreSelectFds(
    int orig_nfds,
    int mod_nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds
  ) {
    // Map eventfd results back to input fds
    for (int fd = 0; fd < mod_nfds; fd++) {
      if (readfds && FD_ISSET(fd, readfds) && isEventFd(fd)) {
        FD_CLR(fd, readfds);
        FD_SET(getInputFdByEventFd(fd), readfds);
      }
      if (writefds && FD_ISSET(fd, writefds) && isEventFd(fd)) {
        FD_CLR(fd, writefds);
        FD_SET(getInputFdByEventFd(fd), writefds);
      }
      if (exceptfds && FD_ISSET(fd, exceptfds) && isEventFd(fd)) {
        FD_CLR(fd, exceptfds);
        FD_SET(getInputFdByEventFd(fd), exceptfds);
      }
    }
    // Clear noise bits from the expanded nfds range
    for (int fd = orig_nfds; fd < mod_nfds; fd++) {
      if (readfds)
        FD_CLR(fd, readfds);
      if (writefds)
        FD_CLR(fd, writefds);
      if (exceptfds)
        FD_CLR(fd, exceptfds);
    }
  }

  int epoll_ctl(int epfd, int op, int fd, struct epoll_event* ev) {
    if (!deviceDescriptors.contains(fd)) {
      return Libc::epoll_ctl(epfd, op, fd, ev);
    }
    int event_fd = getEventFd(fd);
    if (event_fd < 0) {
      errno = EBADF;
      return -1;
    }
    if (op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) {
      struct epoll_event modified_ev;
      memset(&modified_ev, 0, sizeof(modified_ev));
      modified_ev.events = ev->events;
      modified_ev.data.fd = event_fd;
      {
        std::scoped_lock lock(mutex);
        epollMap[epfd][event_fd] = {fd, *ev};
      }
      return Libc::epoll_ctl(epfd, op, event_fd, &modified_ev);
    } else if (op == EPOLL_CTL_DEL) {
      std::scoped_lock lock(mutex);
      auto epIt = epollMap.find(epfd);
      if (epIt != epollMap.end()) {
        epIt->second.erase(event_fd);
      }
      return Libc::epoll_ctl(epfd, op, event_fd, nullptr);
    }
    return Libc::epoll_ctl(epfd, op, fd, ev);
  }

  int
  epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
    int ret = Libc::epoll_wait(epfd, events, maxevents, timeout);
    if (ret <= 0)
      return ret;
    std::scoped_lock lock(mutex);
    auto epIt = epollMap.find(epfd);
    if (epIt == epollMap.end())
      return ret;
    auto& trackByEventFd = epIt->second;
    for (int i = 0; i < ret; i++) {
      auto it = trackByEventFd.find(events[i].data.fd);
      if (it != trackByEventFd.end()) {
        events[i].data = it->second.orig_ev.data;
      }
    }
    return ret;
  }

  bool hasEpollInterest(int epfd) {
    std::scoped_lock lock(mutex);
    auto epIt = epollMap.find(epfd);
    return epIt != epollMap.end() && !epIt->second.empty();
  }
}
