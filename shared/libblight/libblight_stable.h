#include <systemd/sd-bus.h>
#include <iostream>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <vector>
#include <atomic>
#include <sys/mman.h>
#include <sys/socket.h>
#include <functional>
#include <sys/prctl.h>
#include <mutex>
#include <condition_variable>
#include <poll.h>
#include <string>
#include <chrono>
#include <memory>
