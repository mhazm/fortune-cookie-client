///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
#pragma once
#include <Ultralight/Defines.h>
#include <atomic>
#include <mutex> // for std::lock_guard<>

namespace ultralight {

///
/// Lightweight mutex optimized for short locking periods.
///
/// Uncontended lock and unlock are a single inline atomic operation. Contended waiters spin
/// briefly, then block in the OS until the lock is released (no CPU is consumed while waiting).
///
/// Can be used in place of std::mutex since it implements the STL's Lockable interface.
///
/// @note  This lock is not recursive: locking it again from the thread that already holds it
///        will deadlock. Debug builds assert when this is attempted.
///
class Lock {
 public:
  constexpr Lock() = default;

  UL_ALWAYS_INLINE void lock() noexcept {
    // CAS rather than exchange: an exchange would overwrite kLockedContended and lose the
    // pending-waiter state, so a blocked waiter would never be woken.
    uint32_t expected = kUnlocked;
    if (!state_.compare_exchange_strong(expected, kLocked, std::memory_order_acquire,
                                        std::memory_order_relaxed))
      contended_lock();
#ifndef NDEBUG
    owner_.store(current_thread_id(), std::memory_order_relaxed);
#endif
  }

  UL_ALWAYS_INLINE bool try_lock() noexcept {
    // First do a relaxed load to check if lock is free in order to prevent
    // unnecessary cache misses if someone does while(!try_lock())
    if (state_.load(std::memory_order_relaxed) != kUnlocked)
      return false;
    uint32_t expected = kUnlocked;
    if (!state_.compare_exchange_strong(expected, kLocked, std::memory_order_acquire,
                                        std::memory_order_relaxed))
      return false;
#ifndef NDEBUG
    owner_.store(current_thread_id(), std::memory_order_relaxed);
#endif
    return true;
  }

  UL_ALWAYS_INLINE void unlock() noexcept {
#ifndef NDEBUG
    // Clear before releasing the lock word so the next holder never observes a stale owner.
    owner_.store(0, std::memory_order_relaxed);
#endif
    if (state_.exchange(kUnlocked, std::memory_order_release) == kLockedContended)
      contended_unlock();
  }

 protected:
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

  // Lock word states. kLockedContended means a waiter may be blocked in the OS, so unlock()
  // must issue a wake.
  static constexpr uint32_t kUnlocked = 0;
  static constexpr uint32_t kLocked = 1;
  static constexpr uint32_t kLockedContended = 2;

  UExport void contended_lock() noexcept;
  UExport void contended_unlock() noexcept;
  UExport static uint32_t current_thread_id() noexcept;

  std::atomic<uint32_t> state_ = { kUnlocked };
  // Diagnostic only: thread id of the current holder, checked to assert on recursive locking.
  // Written only by debug builds, but present in all builds so the layout of Lock (and of
  // public types that embed it) does not vary with the consumer's build configuration.
  std::atomic<uint32_t> owner_ = { 0 };
};

using LockHolder = std::lock_guard<Lock>;

} // namespace ultralight
