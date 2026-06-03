#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

#include "libblight_global.h"

#include <linux/input.h>

namespace Blight {
  class RingBufferBase {
  protected:
    using AtomicWord = std::atomic<uint32_t>;

    alignas(64) AtomicWord head;
    alignas(64) AtomicWord tail;
    alignas(64) AtomicWord overflow;
    bool allow_overflow;

    RingBufferBase(bool allow_overflow) noexcept
      : head{0}
      , tail{0}
      , overflow{0}
      , allow_overflow{allow_overflow} {}

    static void futex_wait(AtomicWord& word, uint32_t expected) noexcept;
    static void futex_wake(AtomicWord& word) noexcept;

  public:
    bool empty();
    bool overflowed();
    size_t size();
  };

  template<typename T, size_t Size>
  class RingBuffer : private RingBufferBase {
    static_assert(
      (Size & (Size - 1)) == 0,
      "RingBuffer size must be a power of 2 for lock-free operation"
    );
    static constexpr size_t MASK = Size - 1;

  public:
    using RingBufferBase::empty;
    using RingBufferBase::overflowed;
    using RingBufferBase::size;

    RingBuffer(bool allow_overflow = false) noexcept
      : RingBufferBase{allow_overflow}
      , values{} {}

    void insert(const T& event) {
      uint32_t h = head.load(std::memory_order_relaxed);
      uint32_t t = tail.load(std::memory_order_acquire);
      while (h - t >= static_cast<uint32_t>(Size)) {
        if (allow_overflow) {
          overflow.store(1, std::memory_order_release);
          break;
        }
        futex_wait(tail, t);
        t = tail.load(std::memory_order_acquire);
      }
      values[h & MASK] = event;
      head.store(h + 1, std::memory_order_release);
      futex_wake(head);
    }

    std::optional<T> take() {
      uint32_t t = tail.load(std::memory_order_relaxed);
      uint32_t h = head.load(std::memory_order_acquire);
      if (h == t) {
        overflow.exchange(0, std::memory_order_acq_rel);
        return {};
      }
      T event = values[t & MASK];
      overflow.exchange(0, std::memory_order_acq_rel);
      tail.store(t + 1, std::memory_order_release);
      futex_wake(tail);
      return {event};
    }

    T wait_for_values() {
      uint32_t t = tail.load(std::memory_order_relaxed);
      uint32_t h = head.load(std::memory_order_acquire);
      while (h == t) {
        futex_wait(head, h);
        h = head.load(std::memory_order_acquire);
      }
      T event = values[t & MASK];
      overflow.exchange(0, std::memory_order_acq_rel);
      tail.store(t + 1, std::memory_order_release);
      futex_wake(tail);
      return event;
    }

    void wait_for_space() {
      uint32_t h = head.load(std::memory_order_relaxed);
      uint32_t t = tail.load(std::memory_order_acquire);
      while (h - t >= static_cast<uint32_t>(Size)) {
        futex_wait(tail, t);
        t = tail.load(std::memory_order_acquire);
      }
    }

    bool full() {
      uint32_t h = head.load(std::memory_order_relaxed);
      uint32_t t = tail.load(std::memory_order_acquire);
      return h - t >= static_cast<uint32_t>(Size);
    }

    T next() {
      uint32_t t = tail.load(std::memory_order_relaxed);
      return values[t & MASK];
    }

  private:
    std::array<T, Size> values;
  };

  constexpr size_t EVDEV_RING_BUFFER_SIZE = 64;
  using EvdevRingBuffer = RingBuffer<input_event, EVDEV_RING_BUFFER_SIZE>;
  extern template class LIBBLIGHT_EXPORT
    RingBuffer<input_event, EVDEV_RING_BUFFER_SIZE>;
}
