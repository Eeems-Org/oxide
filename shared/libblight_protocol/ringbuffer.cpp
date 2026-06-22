#include "ringbuffer.h"

#include <cerrno>
#include <fcntl.h>
#include <linux/futex.h>
#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace BlightProtocol {
  bool RingBufferBase::futex_wait(
    AtomicWord& word,
    uint32_t expected,
    int timeout
  ) noexcept {
    struct timespec ts;
    struct timespec* tsPtr = nullptr;
    if (timeout > 0) {
      ts.tv_sec = timeout / 1000;
      ts.tv_nsec = (timeout % 1000) * 1000000LL;
      tsPtr = &ts;
    }
    int ret = syscall(
      SYS_futex,
      reinterpret_cast<uint32_t*>(&word),
      FUTEX_WAIT,
      static_cast<int>(expected),
      tsPtr,
      nullptr,
      0
    );
    if (ret == 0) {
      return true; // Woken by futex_wake
    }
    if (errno == EAGAIN) {
      return true; // Value changed before we blocked
    }
    return false; // ETIMEDOUT or EINTR
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

  bool RingBufferBase::full(uint32_t size) {
    uint32_t h = head.load(std::memory_order_relaxed);
    uint32_t t = tail.load(std::memory_order_acquire);
    return h - t >= size;
  }

  bool RingBufferBase::wait_for_space(uint32_t size, int timeout) {
    uint32_t h = head.load(std::memory_order_relaxed);
    uint32_t t = tail.load(std::memory_order_acquire);
    while (h - t >= size) {
      if (interrupted.exchange(false)) {
        return false; // One-shot interrupt consumed, flag auto-cleared
      }
      if (!futex_wait(tail, t, timeout)) {
        if (timeout > 0) {
          return false; // Timed out waiting for space
        }
      }
      t = tail.load(std::memory_order_acquire);
    }
    return true;
  }

  uint32_t RingBufferBase::reserveHead(uint32_t size) {
    uint32_t h = head.load(std::memory_order_relaxed);
    uint32_t t = tail.load(std::memory_order_acquire);
    while (h - t >= size) {
      if (allow_overflow) {
        overflow.store(1, std::memory_order_release);
        break;
      }
      futex_wait(tail, t, 0); // Wait forever
      t = tail.load(std::memory_order_acquire);
    }
    return h;
  }

  void RingBufferBase::commitHead(uint32_t headIndex) {
    head.store(headIndex + 1, std::memory_order_release);
    futex_wake(head);
  }

  std::optional<uint32_t> RingBufferBase::tryConsume() {
    uint32_t t = tail.load(std::memory_order_relaxed);
    uint32_t h = head.load(std::memory_order_acquire);
    if (h == t) {
      overflow.exchange(0, std::memory_order_acq_rel);
      return {};
    }
    return t;
  }

  std::optional<uint32_t> RingBufferBase::waitForTail(int timeout) {
    uint32_t t = tail.load(std::memory_order_relaxed);
    uint32_t h = head.load(std::memory_order_acquire);
    while (h == t) {
      if (interrupted.exchange(false)) {
        return {}; // One-shot interrupt consumed, flag auto-cleared
      }
      if (!futex_wait(head, h, timeout)) {
        return {}; // Timed out or EINTR
      }
      h = head.load(std::memory_order_acquire);
    }
    return t;
  }

  std::optional<uint32_t> RingBufferBase::peekTail() {
    uint32_t h = head.load(std::memory_order_acquire);
    uint32_t t = tail.load(std::memory_order_relaxed);
    if (h == t) {
      return {};
    }
    return t;
  }

  void RingBufferBase::releaseTail(uint32_t tailIndex) {
    overflow.exchange(0, std::memory_order_acq_rel);
    tail.store(tailIndex + 1, std::memory_order_release);
    futex_wake(tail);
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

  int RingBufferBase::createSharedMemoryInstance(
    size_t size,
    const char* name,
    void** instancePtr
  ) {
    *instancePtr = nullptr;
    int fd = memfd_create(name, MFD_ALLOW_SEALING);
    if (fd < 0) {
      return -1;
    }
    if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
      int err = errno;
      close(fd);
      errno = err;
      return -1;
    }
    void* mem =
      mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, fd, 0);
    if (mem == MAP_FAILED || mem == nullptr) {
      int err = errno;
      close(fd);
      errno = err;
      return -1;
    }
    *instancePtr = mem;
    return fd;
  }

  void* RingBufferBase::mapSharedMemoryInstance(int fd, size_t size) {
    void* mem =
      mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, fd, 0);
    if (mem == MAP_FAILED) {
      return nullptr;
    }
    return mem;
  }

  void RingBufferBase::unmapSharedMemoryInstance(void* mem, size_t size) {
    munmap(mem, size);
  }

  void RingBufferBase::interrupt() noexcept {
    interrupted.store(true, std::memory_order_release);
    futex_wake(head);
    futex_wake(tail);
  }

  template class LIBBLIGHT_PROTOCOL_EXPORT
    RingBuffer<input_event, EVDEV_RING_BUFFER_SIZE>;
}
