#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <linux/input.h>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#include "libblight_global.h"

namespace Blight {
  class RingBufferBase {
  protected:
    using AtomicWord = std::atomic<uint32_t>;
    static_assert(
      AtomicWord::is_always_lock_free,
      "RingBuffer requires lock-free atomic for shared memory safety"
    );
    static_assert(
      sizeof(AtomicWord) == sizeof(uint32_t),
      "AtomicWord layout must match uint32_t for futex"
    );
    static_assert(
      alignof(AtomicWord) == alignof(uint32_t),
      "AtomicWord alignment must match uint32_t for futex"
    );
    static_assert(
      std::is_standard_layout_v<AtomicWord>,
      "AtomicWord must be standard layout for direct memory access"
    );

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

    static int createSharedMemoryInstance(
      size_t size,
      const char* name,
      void** instancePtr
    );
    static void* mapSharedMemoryInstance(int fd, size_t size);
    static void unmapSharedMemoryInstance(void* mem, size_t size);

    bool full(uint32_t size);
    void wait_for_space(uint32_t size);
    uint32_t reserveHead(uint32_t size);
    void commitHead(uint32_t headIndex);
    std::optional<uint32_t> tryConsume();
    uint32_t waitForTail();
    std::optional<uint32_t> peekTail();
    void releaseTail(uint32_t tailIndex);

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
      uint32_t h = reserveHead(Size);
      values[h & MASK] = event;
      commitHead(h);
    }

    std::optional<T> take() {
      std::optional<uint32_t> t = tryConsume();
      if (!t.has_value()) {
        return {};
      }
      T event = values[t.value() & MASK];
      releaseTail(t.value());
      return {event};
    }

    T wait_for_values() {
      uint32_t t = waitForTail();
      T event = values[t & MASK];
      releaseTail(t);
      return event;
    }

    void wait_for_space() { RingBufferBase::wait_for_space(Size); }

    bool full() { return RingBufferBase::full(Size); }

    std::optional<T> next() {
      auto t = peekTail();
      return t.has_value() ? std::optional<T>(values[t.value() & MASK])
                           : std::nullopt;
    }

    static std::pair<int, RingBuffer<T, Size>*> createSharedMemory(
      bool allow_overflow = false
    ) {
      char name[64];
      std::snprintf(name, sizeof(name), "RingBuffer<%zu,%zu>", sizeof(T), Size);
      void* mem;
      int fd =
        createSharedMemoryInstance(sizeof(RingBuffer<T, Size>), name, &mem);
      if (fd < 0) {
        return {-1, nullptr};
      }
      return {fd, new (mem) RingBuffer<T, Size>(allow_overflow)};
    }

    static RingBuffer<T, Size>* fromSharedMemory(int fd) {
      return static_cast<RingBuffer<T, Size>*>(
        mapSharedMemoryInstance(fd, sizeof(RingBuffer<T, Size>))
      );
    }

  private:
    std::array<T, Size> values;
  };

  constexpr size_t EVDEV_RING_BUFFER_SIZE = 64;
  using EvdevRingBuffer = RingBuffer<input_event, EVDEV_RING_BUFFER_SIZE>;
  extern template class LIBBLIGHT_EXPORT
    RingBuffer<input_event, EVDEV_RING_BUFFER_SIZE>;
}
