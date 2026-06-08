#include "ringbuffer.h"

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace Blight {
  void
  RingBufferBase::futex_wait(AtomicWord& word, uint32_t expected) noexcept {
    syscall(
      SYS_futex,
      reinterpret_cast<uint32_t*>(&word),
      FUTEX_WAIT,
      static_cast<int>(expected),
      nullptr,
      nullptr,
      0
    );
  }

  void RingBufferBase::futex_wake(AtomicWord& word) noexcept {
    syscall(
      SYS_futex,
      reinterpret_cast<uint32_t*>(&word),
      FUTEX_WAKE,
      1,
      nullptr,
      nullptr,
      0
    );
  }

  bool RingBufferBase::empty() {
    uint32_t h = head.load(std::memory_order_acquire);
    uint32_t t = tail.load(std::memory_order_relaxed);
    return h == t;
  }

  bool RingBufferBase::overflowed() {
    return overflow.load(std::memory_order_acquire) != 0;
  }

  size_t RingBufferBase::size() {
    uint32_t h = head.load(std::memory_order_acquire);
    uint32_t t = tail.load(std::memory_order_relaxed);
    return static_cast<size_t>(h - t);
  }

  template class RingBuffer<input_event, EVDEV_RING_BUFFER_SIZE>;
}
