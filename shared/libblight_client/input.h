#pragma once
#include <libblight.h>
#include <libblight_protocol/ringbuffer.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <poll.h>
#include <string>
#include <sys/epoll.h>
#include <sys/select.h>
#include <thread>

struct linux_dirent;
struct linux_dirent64;

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
    std::shared_ptr<Blight::input_buffer_t> inputBuffer;
    std::shared_ptr<Blight::EvdevRingBuffer> ringBuffer;
    std::shared_ptr<std::thread> thread;
    std::atomic<bool> stopRequested;
    DeviceInfo();
    DeviceInfo(
      unsigned short device,
      int fd,
      InputType type,
      std::shared_ptr<Blight::input_buffer_t> inputBuffer
    );
    ~DeviceInfo();
    void stop();
  };
  struct DeviceMap {
    unsigned short device;
    int flags;
  };
  bool init();
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
  int epollCtl(int epfd, int op, int fd, struct epoll_event* ev);
  int restoreEpollfds(int epfd, struct epoll_event* events, int res);
  bool isDevInputDir(unsigned int fd);
  bool shouldHideEvent(const char* entryName);
  int compactDirents(void* inBuffer, int size, void* outBuffer, int offset);
  template<typename DirEnt, typename F>
  int getdents(unsigned int fd, DirEnt* outBuffer, unsigned int count, F func) {
    if (!isDevInputDir(fd)) {
      return func(fd, outBuffer, count);
    }
    void* inBuffer = malloc(count);
    if (inBuffer == nullptr) {
      errno = ENOMEM;
      return -1;
    }
    auto size = func(fd, static_cast<DirEnt*>(inBuffer), count);
    if (size <= 0) {
      free(inBuffer);
      return size;
    }
    int dstLength =
      compactDirents(inBuffer, size, outBuffer, offsetof(DirEnt, d_name));
    free(inBuffer);
    return dstLength;
  }
}
