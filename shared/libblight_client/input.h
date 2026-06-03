#pragma once
#include <libblight.h>
#include <libblight/ringbuffer.h>

#include <linux/input.h>
#include <map>
#include <mutex>
#include <poll.h>
#include <string>
#include <sys/epoll.h>
#include <sys/select.h>
#include <vector>

namespace Input {
  struct DeviceInfo {
    int device;
    int flags;
  };

  extern std::map<int, Blight::EvdevRingBuffer> ringBuffers;
  extern std::map<int, DeviceInfo> deviceDescriptors;
  extern std::map<int, int> deviceEventFds;
  extern std::map<int, std::map<int, struct epoll_event>> epollMap;
  extern std::mutex mutex;

  void readEvents();
  int open(const std::string& path, int flags);
  int close(int fd);
  int ioctlv(int fd, unsigned long request, char* ptr);
  ssize_t read(int fd, void* buf, size_t size);
  int fcntl(int fd, int cmd, void* ptr);
  void translatePollfds(struct pollfd* fds, nfds_t nfds);
  void restorePollfds(struct pollfd* fds, nfds_t nfds);
  int translateSelectFds(
    int& nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds
  );
  void restoreSelectFds(
    int orig_nfds,
    int mod_nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds
  );
  int epoll_ctl(int epfd, int op, int fd, struct epoll_event* ev);
  int restoreEpollfds(int epfd, struct epoll_event* events, int res);
}
