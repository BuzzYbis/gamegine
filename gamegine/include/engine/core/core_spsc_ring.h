// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_spsc_ring.h                                                   -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_SPSC_RING_H
#define INCLUDED_ENGINE_CORE_SPSC_RING_H

//@PURPOSE: Provide a wait-free single-producer, single-consumer ring buffer.
//
//@CLASSES:
//  engine::core::spsc::SpscRing: bounded FIFO between exactly two threads
//
//@SEE_ALSO: core_log
//
//@DESCRIPTION: [SpscRing] moves trivially copyable values from exactly one
// producer thread to exactly one consumer thread. Every call is wait-free: it
// finishes in a bounded number of its own steps whatever the other thread
// does, including being descheduled in the middle of its own call. There is
// no loop to retry, and "full" and "empty" are answers ([false], or nothing),
// not waits. Wrapping [try_push] in a retry loop builds blocking back on top.
//
///Design
///------
// Two counters only ever grow: [d_head] counts the values ever pushed and
// [d_tail] the values ever popped. Each has one writer, the producer for
// [d_head] and the consumer for [d_tail], so neither needs a
// read-modify-write: no compare-and-swap, no [fetch_add], nothing to retry.
// The slot for a counter [c] is [c & (capacity - 1)], which is why the
// capacity is a power of two. The ring holds [head - tail] values: it is empty
// when they are equal and full when they differ by the capacity. Unsigned
// subtraction stays right even if a counter wraps, which takes 584 years at a
// push per nanosecond.
//
// The slots in [tail, head) belong to the consumer and the slots in
// [head, tail + capacity) to the producer. A thread touches only slots it
// owns, and ownership moves only when a counter is stored. Two
// release/acquire pairs do all the synchronization:
//
//: o A -- publish: the producer copies a value into its slot, then stores
//:   [d_head] with release; the consumer loads [d_head] with acquire before
//:   it reads the slot, and never reads a half-written value.
//:
//: o B -- reclaim: the consumer reads a slot, then stores [d_tail] with
//:   release; the producer loads [d_tail] with acquire before it reuses the
//:   slot, and never overwrites a value still being read. This is the one
//:   that is forgotten: ARM may let a load complete after a later store.
//
// Each side also keeps a private copy of the other side's counter, and reads
// the real one only when its copy says full (the producer) or empty (the
// consumer). A stale copy is always too small, since the counters only grow,
// so it can only make a side refresh early, never overwrite or read early.
//
///Layout
///------
// The producer's counter and its copy of [d_tail] share a block of
// [k_CACHE_LINE_SIZE] bytes, the consumer's counter and its copy another, and
// the slots start on a third: no line is written by both threads. The slots
// are zeroed at construction, which touches every page of the ring, so that
// no push takes a first-touch page fault, the one trap into the kernel a
// wait-free call could otherwise still make.
//
///Thread Safety
///-------------
// [try_push], in both forms, may be called from one producer thread, and
// [try_pop] from one consumer thread, concurrently with each other. Any other
// use is undefined: two producers, or a third thread calling anything.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Handing Records to the Logger Thread
///- - - - - - - - - - - - - - - - - - - - - - - -
// The ring is large and aligned to a cache line, so it lives on the heap, and
// is created before either thread uses it:
// ```
//  const auto ring = std::make_unique<SpscRing<LogRecord, 4096>>();
//
//  // producer thread: a full ring is not an error, the record is dropped
//  if (!ring->try_push(record)) {
//      ++dropped;
//  }
//
//  // consumer thread
//  while (const std::optional<LogRecord> next = ring->try_pop()) {
//      write(*next);
//  }
// ```
//
///Example 2: Pushing Several Values Together
/// - - - - - - - - - - - - - - - - - - - - -
// A batch is accepted whole or not at all, and the consumer sees all of it
// at once:
// ```
//  const std::array<LogRecord, 3> text = {head, body, tail};
//  if (!ring->try_push(std::span<const LogRecord>(text))) {
//      ++dropped;  // none of the three went in
//  }
// ```

// std
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

namespace engine::core {
namespace spsc {

/// This concept is satisfied by the arguments a [SpscRing] accepts: a
/// trivially copyable [t_TYPE], since slots are raw bytes copied in and out,
/// and a [t_CAPACITY] that is a power of two, since a slot is found with a
/// mask.
template <class t_TYPE, std::size_t t_CAPACITY>
concept SpscRingParameters = std::is_trivially_copyable_v<t_TYPE> &&
                             std::has_single_bit(t_CAPACITY);

// Why 128, and why not [std::hardware_destructive_interference_size]:
//
//  o The standard constant is not the hardware's line size, only each
//    compiler's guess, and the guesses differ on one machine. On the M2:
//    Apple clang 21 gives 256, GCC 16 gives 256, Homebrew clang 23 gives
//    128. For x86 we only recall that GCC gives 64, unchecked: if so it
//    would be the one value we have a reason to distrust (see below).
//  o GCC rejects it in a header: [-Werror=interference-size], "use of
//    std::hardware_destructive_interference_size". Silencing it takes a
//    GCC-only flag in every translation unit ([-Wno-interference-size], and
//    [--param hardware_destructive_interference_size=N] to pin the value).
//    The same use in a [.cpp] file is silent, so a test may use it.
//  o Nobody measured 256. We measured 128.
//
// What was measured on the M2 (two threads incrementing counters [gap] bytes
// apart, best of 7): both on P-cores, 64 bytes apart is as good as 128 (2.0 ns
// per increment); with an E-core involved, 64 interferes (5-7 ns) and 128 does
// not (3 ns). Each core's L1 has 64-byte lines, the cluster L2 and the system
// cache 128-byte ones, which is what [sysctl hw.cachelinesize] reports. The
// logger thread is the kind macOS puts on an E-core, so 128 is needed. On x86,
// lines are 64 bytes, but the L2 prefetcher fetches them in aligned 128-byte
// pairs.
//
// TODO(Profile L): measure the same on the Core Ultra 9 185H, with the threads
// on P-cores, on E-cores, and one on each, and record whether 64 bytes apart
// interferes there. Until then 128 on x86 rests on the documented prefetcher
// pairing, not on a measurement.
//
// [core_spsc_ring.t.cpp] checks the constant in a [.cpp] file, where GCC does
// not warn about the standard one: a power of two, and at least
// [std::hardware_constructive_interference_size].

/// Distance (in bytes) that keeps two variables written by different threads
/// off one cache line, on every platform we build for.
inline constexpr std::size_t k_CACHE_LINE_SIZE = 128;

                         // ==============
                         // class SpscRing
                         // ==============

/// This mechanism moves values of the trivially copyable [t_TYPE] from one
/// producer thread to one consumer thread through [t_CAPACITY] slots, and
/// never waits: a call returns at once, answering [false] or nothing when the
/// ring is full or empty. It cannot be copied. Note that the ring is large
/// and aligned to [k_CACHE_LINE_SIZE], so create it on the heap; see the
/// component documentation for which thread may call what.
template <class t_TYPE, std::size_t t_CAPACITY>
    requires SpscRingParameters<t_TYPE, t_CAPACITY>
class SpscRing final {
  private:
    // CLASS DATA

    /// Mask that turns a counter into a slot number; see [slot].
    static constexpr std::uint64_t k_MASK = t_CAPACITY - 1;

    // The counters are the only variables both threads touch, so they must
    // not hide a lock: a lock would make the calls wait.
    static_assert(std::atomic_uint64_t::is_always_lock_free,
                  "the ring's counters SHALL be lock-free atomics");

    // DATA

    // The producer's block: [d_head] counts the values ever pushed, written by
    // the producer and read by the consumer; [d_cached_tail] is the
    // producer's own copy of [d_tail].
    alignas(k_CACHE_LINE_SIZE) std::atomic_uint64_t d_head{0};
    std::uint64_t d_cached_tail = 0;

    // The consumer's block, the mirror image: [d_tail] counts the values ever
    // popped; [d_cached_head] is the consumer's own copy of [d_head].
    alignas(k_CACHE_LINE_SIZE) std::atomic_uint64_t d_tail{0};
    std::uint64_t d_cached_head = 0;

    // The slots, as raw bytes rather than [t_TYPE]s: the only requirement on
    // [t_TYPE] is then trivial copyability, so a type with no default
    // constructor (such as [Error]) still fits. Zeroed here, which touches
    // every page.
    alignas(std::max(k_CACHE_LINE_SIZE, alignof(t_TYPE)))
        std::array<std::byte, t_CAPACITY * sizeof(t_TYPE)> d_slots{};

    // PRIVATE MANIPULATORS

    /// Return the address of the slot for the specified [counter]: its low
    /// bits select the slot, and [sizeof(t_TYPE)] bytes make a slot.
    std::byte* slot(const std::uint64_t counter) noexcept;

    // NOT IMPLEMENTED

    SpscRing(const SpscRing&)            = delete;
    SpscRing& operator=(const SpscRing&) = delete;

  public:
    // CLASS METHODS

    /// Return the number of values the ring holds when full.
    [[nodiscard]] static constexpr std::size_t capacity() noexcept;

    // CREATORS

    /// Create an empty ring with every slot zeroed. Note that nothing
    /// synchronizes the creation with the two threads, so create the ring
    /// before either of them starts to use it.
    SpscRing() noexcept = default;

    /// Destroy this object.
    //! ~SpscRing() = default;

    // MANIPULATORS

    // Why [try_push] and [try_pop] answer with a [bool] and an
    // [std::optional], not a [Status] (D9): a full or empty ring is not a
    // failure with a cause the caller could act on, so the rule is [expected]
    // when the failure has a cause worth reporting, and [bool] or [optional]
    // when the only information is "not now". It costs nothing either way: a
    // push and a pop of 16 bytes took 4.80 ns (clang) and 5.00 ns (GCC 16)
    // with all three, the calls being inline. [std::mutex::try_lock], Boost's
    // [spsc_queue] and Folly's [ProducerConsumerQueue] return a [bool] too.

    /// Append the specified [value] and return [true], or return [false] and
    /// change nothing if the ring is full. Note that a full ring is an
    /// answer, not an error: the logger drops the record, counts it and
    /// carries on, where an [Error] would be passed up. Call from the
    /// producer thread only.
    [[nodiscard]] bool try_push(const t_TYPE& value) noexcept;

    /// Append all of the specified [values], in order, and return [true]; or
    /// return [false] and append none of them if the ring cannot take them
    /// all, which is always so for more than [capacity()]. They become visible
    /// to the consumer together, with one release. An empty [values] is
    /// accepted and changes nothing. Call from the producer thread only.
    [[nodiscard]] bool try_push(const std::span<const t_TYPE> values) noexcept;

    /// Remove and return the oldest value, or return nothing if the ring is
    /// empty. Note that the value is returned, not written to a parameter, so
    /// a type with no default constructor (such as [Error]) can be popped.
    /// Call from the consumer thread only.
    [[nodiscard]] std::optional<t_TYPE> try_pop() noexcept;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

                         // --------------
                         // class SpscRing
                         // --------------

// PRIVATE MANIPULATORS

template <class t_TYPE, std::size_t t_CAPACITY>
    requires SpscRingParameters<t_TYPE, t_CAPACITY>
inline std::byte*
SpscRing<t_TYPE, t_CAPACITY>::slot(const std::uint64_t counter) noexcept
{
    return d_slots.data() +
           (static_cast<std::size_t>(counter & k_MASK) * sizeof(t_TYPE));
}

// CLASS METHODS

template <class t_TYPE, std::size_t t_CAPACITY>
    requires SpscRingParameters<t_TYPE, t_CAPACITY>
inline constexpr std::size_t SpscRing<t_TYPE, t_CAPACITY>::capacity() noexcept
{
    return t_CAPACITY;
}

// MANIPULATORS

template <class t_TYPE, std::size_t t_CAPACITY>
    requires SpscRingParameters<t_TYPE, t_CAPACITY>
inline bool
SpscRing<t_TYPE, t_CAPACITY>::try_push(const t_TYPE& value) noexcept
{
    // Our own counter: only this thread writes it, so a relaxed load reads
    // back our last store.
    const std::uint64_t head = d_head.load(std::memory_order_relaxed);
    if (head - d_cached_tail == t_CAPACITY) {
        // Full as far as our copy knows: refresh it. Acquire pairs with the
        // consumer's release of [d_tail] (edge B), so the slot we are about
        // to reuse has been read completely.
        d_cached_tail = d_tail.load(std::memory_order_acquire);
        if (head - d_cached_tail == t_CAPACITY) {
            return false;                                             // RETURN
        }
    }

    // A slot we own: a plain copy, no atomics needed.
    std::memcpy(slot(head), std::addressof(value), sizeof(t_TYPE));

    // Publish (edge A): the copy above becomes visible no later than the new
    // [d_head] does.
    d_head.store(head + 1, std::memory_order_release);
    return true;
}

template <class t_TYPE, std::size_t t_CAPACITY>
    requires SpscRingParameters<t_TYPE, t_CAPACITY>
inline bool
SpscRing<t_TYPE, t_CAPACITY>::try_push(std::span<const t_TYPE> values) noexcept
{
    if (values.empty()) {
        return true;
    }

    // More than the ring holds never fits, whatever the consumer does; this
    // also keeps the room computed below from wrapping.
    const std::uint64_t count = values.size();
    if (count > t_CAPACITY) {
        return false;                                                 // RETURN
    }

    // The room left is [t_CAPACITY - (head - tail)]: our copy of [tail] is
    // never ahead of the real one, so it can only under-estimate the room.
    const std::uint64_t head = d_head.load(std::memory_order_relaxed);
    if (count > t_CAPACITY - (head - d_cached_tail)) {
        // Too little room as far as our copy knows: refresh it (edge B).
        d_cached_tail = d_tail.load(std::memory_order_acquire);
        if (count > t_CAPACITY - (head - d_cached_tail)) {
            return false;                                             // RETURN
        }
    }

    for (std::uint64_t i = 0; i < count; ++i) {
        std::memcpy(slot(head + i),
                    std::addressof(values[static_cast<std::size_t>(i)]),
                    sizeof(t_TYPE));
    }

    // Publish all of them with one release (edge A): the consumer sees the
    // whole batch or none of it.
    d_head.store(head + count, std::memory_order_release);
    return true;
}

template <class t_TYPE, std::size_t t_CAPACITY>
    requires SpscRingParameters<t_TYPE, t_CAPACITY>
inline std::optional<t_TYPE> SpscRing<t_TYPE, t_CAPACITY>::try_pop() noexcept
{
    // Our own counter: only this thread writes it.
    const std::uint64_t tail = d_tail.load(std::memory_order_relaxed);
    if (tail == d_cached_head) {
        // Empty as far as our copy knows: refresh it. Acquire pairs with the
        // producer's release of [d_head] (edge A), so the slot is whole.
        d_cached_head = d_head.load(std::memory_order_acquire);
        if (tail == d_cached_head) {
            return std::nullopt;
        }
    }

    // Copied into bytes first, then made a [t_TYPE]: that needs no default
    // constructor, and compiles to the same copy.
    std::array<std::byte, sizeof(t_TYPE)> bytes;
    std::memcpy(bytes.data(), slot(tail), bytes.size());

    // Hand the slot back (edge B): our read above completes no later than the
    // new [d_tail] becomes visible.
    d_tail.store(tail + 1, std::memory_order_release);
    return std::bit_cast<t_TYPE>(bytes);
}

}  // close namespace spsc
}  // close namespace engine::core

#endif
