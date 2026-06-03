#pragma once
#include <libblight.h>
#include <libblight/ringbuffer.h>

#include <linux/input.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#define RING_BUFFER_SIZE 64

namespace Input {
  extern std::map<int, RingBuffer<input_event, RING_BUFFER_SIZE>> ringBuffers;
  extern std::map<int, std::pair<int, int>> deviceDescriptors;
  extern std::mutex mutex;

  int open(const std::string& path, int flags);
  void readEvents();
  int close(int fd);
  int ioctlv(int fd, unsigned long request, char* ptr);
  ssize_t read(int fd, void* buf, size_t size);
  int fcntl(int fd, int cmd, void* ptr);
}
