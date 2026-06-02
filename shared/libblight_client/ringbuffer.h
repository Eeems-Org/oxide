#include <array>
#include <condition_variable>
#include <mutex>
#include <optional>

template<typename T, size_t Size>
class RingBuffer {
public:
  RingBuffer()
    : head{0}
    , tail{0}
    , count{0}
    , overflow{false}
    , values{} {}

  inline void insert(const T& event) {
    {
      std::lock_guard lock(mutex);
      values[tail] = event;
      tail = (tail + 1) % Size;
      if (full()) {
        head = (head + 1) % Size;
        overflow = true;
      } else {
        count++;
      }
    }
    condition.notify_one();
  }

  inline std::optional<T> take() {
    std::lock_guard lock(mutex);
    if (!count) {
      return {};
    }
    overflow = false;
    T event = values[head];
    head = (head + 1) % Size;
    count--;
    return {event};
  }

  inline T wait() {
    std::unique_lock lock(mutex);
    condition.wait(lock, [this]() { return count > 0; });
    overflow = false;
    T event = values[head];
    head = (head + 1) % Size;
    count--;
    return event;
  }

  inline bool empty() {
    std::lock_guard lock(mutex);
    return !count;
  }

  inline bool full() { return count >= Size; }

  inline bool overflowed() {
    std::lock_guard lock(mutex);
    return overflow;
  }

  inline size_t size() {
    std::lock_guard lock(mutex);
    return count;
  }

private:
  std::array<T, Size> values;
  size_t head;
  size_t tail;
  size_t count;
  bool overflow;
  std::mutex mutex;
  std::condition_variable condition;
};
