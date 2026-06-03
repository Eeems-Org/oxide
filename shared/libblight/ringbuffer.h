#include <array>
#include <condition_variable>
#include <mutex>
#include <optional>

template<typename T, size_t Size>
class RingBuffer {
public:
  RingBuffer(bool allow_overflow = false)
    : head{0}
    , tail{0}
    , count{0}
    , overflow{false}
    , values{}
    , allow_overflow{allow_overflow} {}

  inline void insert(const T& event) {
    {
      std::unique_lock lock(mutex);
      if (count >= Size && !allow_overflow) {
        notFullCondition.wait(lock, [this]() { return count < Size; });
      }
      values[tail] = event;
      tail = (tail + 1) % Size;
      if (count < Size) {
        count++;
      } else if (allow_overflow) {
        head = (head + 1) % Size;
        overflow = true;
      }
    }
    notEmptyCondition.notify_one();
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
    notFullCondition.notify_one();
    return {event};
  }

  inline T wait_for_values() {
    std::unique_lock lock(mutex);
    notEmptyCondition.wait(lock, [this]() { return count > 0; });
    overflow = false;
    T event = values[head];
    head = (head + 1) % Size;
    count--;
    notFullCondition.notify_one();
    return event;
  }

  inline void wait_for_space() {
    std::unique_lock lock(mutex);
    if (count >= Size) {
      notFullCondition.wait(lock, [this]() { return count < Size; });
    }
  }

  inline bool empty() {
    std::lock_guard lock(mutex);
    return !count;
  }

  inline bool full() {
    std::lock_guard lock(mutex);
    return count >= Size;
  }

  inline bool overflowed() {
    std::lock_guard lock(mutex);
    return overflow;
  }

  inline size_t size() {
    std::lock_guard lock(mutex);
    return count;
  }

  inline T next() {
    std::lock_guard lock(mutex);
    return values[head];
  }

private:
  std::array<T, Size> values;
  size_t head;
  size_t tail;
  size_t count;
  bool overflow;
  bool allow_overflow;
  std::mutex mutex;
  std::condition_variable notEmptyCondition;
  std::condition_variable notFullCondition;
};
