#include "input.h"
#include "fb.h"
#include "libc.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <libblight/debug.h>
#include <libblight/socket.h>
#include <linux/input.h>
#include <sys/select.h>

namespace Input {
    unsigned int BATCH_SIZE = 0;
    std::map<int, int[2]> fds = std::map<int, int[2]>{};
    std::map<int, int> deviceMap = std::map<int, int>{};
    std::mutex mutex = std::mutex();

    void sendEvents(
        unsigned int device,
        std::vector<Blight::partial_input_event_t>& queue
    )
    {
        if (queue.empty()) {
            return;
        }
        timeval time;
        ::gettimeofday(&time, NULL);
        std::vector<input_event> data(queue.size());
        bool allow_power_button =
            getenv("OXIDE_PRELOAD_ALLOW_POWER_BUTTON") != nullptr;
        for (unsigned int i = 0; i < queue.size(); i++) {
            auto& event = queue[i];
            if (!allow_power_button && event.type == EV_KEY &&
                event.code == KEY_POWER) {
                event.value = 0;
            }
            data[i] = input_event{ // .time = time,
                                   .type = event.type,
                                   .code = event.code,
                                   .value = event.value
            };
        }
        auto fd = fds[device][0];
        size_t total = sizeof(input_event) * data.size();
        size_t written = 0;
        while (written < total) {
            auto res = Libc::write(
                fd,
                reinterpret_cast<char*>(data.data()) + written,
                total - written
            );
            if (res < 0) {
                if (errno == EAGAIN || errno == EINTR) {
                    size_t completed = written / sizeof(input_event);
                    _WARN(
                        "Partial input event write: %d of %d bytes", res, total
                    );
                    if (completed > 0) {
                        _DEBUG("Sent %d input events", completed);
                        queue.erase(queue.begin(), queue.begin() + completed);
                    }
                    size_t remainder = res - completed * sizeof(input_event);
                    if (remainder > 0) {
                        _WARN(
                            "Partial input event, blocking until remainder sent"
                        );
                        if (Blight::send_blocking(
                                fd,
                                (Blight::data_t)(data.data() + written),
                                remainder
                            )) {
                            queue.erase(queue.begin(), queue.begin() + 1);
                        } else {
                            _CRIT(
                                "%d input events failed to send: %s",
                                1,
                                std::strerror(errno)
                            );
                        }
                    }
                    return;
                }
                _CRIT(
                    "%d input events failed to send: %s",
                    data.size(),
                    std::strerror(errno)
                );
                return;
            }
            written += res;
        }
        _DEBUG("Sent %d input events", data.size());
        queue.clear();
    }

    bool __hasAny(
        std::map<unsigned int, std::vector<Blight::partial_input_event_t>>
            events
    )
    {
        for (auto& item : events) {
            auto& queue = item.second;
            if (queue.empty()) {
                return true;
            }
        }
        return false;
    }

    void read()
    {
        _INFO("%s", "InputWorker starting");
        prctl(PR_SET_NAME, "InputWorker\0", 0, 0, 0);
        nice(-10);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        auto fd = FB::connection->input_handle();
        std::map<unsigned int, std::vector<Blight::partial_input_event_t>>
            events;
        constexpr int timeoutMs = 25;
        while (fd > 0 && FB::connection != nullptr &&
               getenv("OXIDE_PRELOAD_DISABLE_INPUT") == nullptr) {
            auto maybe = FB::connection->read_event();
            if (!maybe.has_value()) {
                if (errno != EAGAIN && errno != EINTR) {
                    _WARN(
                        "[InputWorker] Failed to read message %s",
                        std::strerror(errno)
                    );
                    break;
                }
                _DEBUG("Waiting for next input event");
                if (!Blight::wait_for_read(
                        fd, __hasAny(events) ? timeoutMs : -1
                    ) &&
                    errno != EAGAIN) {
                    _WARN(
                        "[InputWorker] Failed to wait for next input event %s",
                        std::strerror(errno)
                    );
                    break;
                }
                continue;
            }
            const auto& device = maybe.value().device;
            mutex.lock();
            if (!fds.contains(device)) {
                _INFO("Ignoring event for unopened device: %d", device);
                mutex.unlock();
                continue;
            }
            if (fds[device][0] < 0) {
                _INFO("Ignoring event for invalid device: %d", device);
                mutex.unlock();
                continue;
            }
            mutex.unlock();
            auto& event = maybe.value().event;
            auto& queue = events[device];
            queue.push_back(event);
            if ((BATCH_SIZE && queue.size() >= BATCH_SIZE) ||
                (!BATCH_SIZE && event.type == EV_SYN &&
                 event.code == SYN_REPORT)) {
                sendEvents(device, queue);
            }
        }
        _INFO("%s", "InputWorker quitting");
    }

    int ioctlv(int fd, unsigned long request, char* ptr)
    {
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
}
