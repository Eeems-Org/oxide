#pragma once
#include <libblight.h>
#include <map>
#include <mutex>
#include <vector>

namespace Input {
  extern unsigned int BATCH_SIZE;
  extern std::map<int, int[2]> fds;
  extern std::map<int, int> deviceMap;
  extern std::mutex mutex;
  void sendEvents(
    unsigned int device,
    std::vector<Blight::partial_input_event_t>& queue
  );
  void read();
  int ioctlv(int fd, unsigned long request, char* ptr);
}
