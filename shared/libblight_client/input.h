#pragma once
#include <libblight.h>
#include <libblight/ringbuffer.h>

#include <poll.h>
#include <string>
#include <sys/epoll.h>
#include <sys/select.h>

namespace Input {
  enum InputType : uint8_t { Unknown = 0, Wacom, Touch, Buttons, Invalid };
  struct MinMax {
    int min;
    int max;
  };
  struct Transform {
    bool invertX;
    bool invertY;
    float rotation;
  };
  struct DeviceState {
    int x;
    int y;
    int pressure;
    int distance;
    int tiltX;
    int tiltY;
    int orientation;
    int major;
    int minor;
  };
  struct DeviceInfo {
    unsigned short device;
    int fd;
    int eventFd;
    InputType type;
    DeviceState state;
    DeviceState minimums;
    DeviceState maximums;
    std::shared_ptr<Blight::EvdevRingBuffer> ringBuffer;
    std::shared_ptr<Blight::EvdevRingBuffer> remapBuffer;
    std::shared_ptr<std::thread> thread;
    ~DeviceInfo();
  };
  struct DeviceMap {
    unsigned short device;
    int flags;
  };
  bool init();
  void readEvents();
  bool isInputFd(int fd);
  int open(const std::string& path, int flags);
  int close(int fd);
  int ioctlv(int fd, unsigned long request, char* ptr);
  ssize_t read(int fd, void* buf, size_t size);
  int fcntl(int fd, int cmd, void* ptr);
  struct pollfd* translatePollfds(struct pollfd* fds, nfds_t nfds);
  void restorePollfds(struct pollfd* fds, nfds_t nfds, struct pollfd* backup);
  int translateSelectFds(
    int& nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds
  );
  void restoreSelectFds(
    int nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds,
    int backup
  );
  int epoll_ctl(int epfd, int op, int fd, struct epoll_event* ev);
  int restoreEpollfds(int epfd, struct epoll_event* events, int res);
}
