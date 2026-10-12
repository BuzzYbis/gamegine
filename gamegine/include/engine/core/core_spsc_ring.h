// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_spsc_ring.h                                                   -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_SPSC_RING_H
#define INCLUDED_ENGINE_CORE_SPSC_RING_H

//@PURPOSE: Provides a wait-free single-producer, single-consumer ring buffer.
//
//@CLASSES:
//
//
//@DESCRIPTION:

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

template <class t_TYPE, std::size_t t_CAPACITY>
concept SpscRingParameters = std::is_trivially_copyable_v<t_TYPE> &&
                             std::has_single_bit(t_CAPACITY);

/// Distance (in bytes) that keeps two variables written by different
/// threads off one cache line, on every platform we build for. Why 128,
/// and why not [std::hardware_destructive_interference_size]:
///
///  o The standard constant is not the hardware's line size, only each
///    compiler's guess, and the guesses differ on one machine. On the M2:
///    Apple clang 21 gives 256, GCC 16 gives 256, Homebrew clang 23 gives
///    128. For x86 we only recall that GCC gives 64, unchecked: if so it
///    would be the one value we have a reason to distrust (see below).
///  o GCC rejects it in a header: [-Werror=interference-size], "use of
///    std::hardware_destructive_interference_size". Silencing it takes a
///    GCC-only flag in every translation unit ([-Wno-interference-size],
///    and
///    [--param hardware_destructive_interference_size=N] to pin the
///    value). The same use in a [.cpp] file is silent, so a test may use
///    it.
///  o Nobody measured 256. We measured 128 (see below).
///
/// What was measured on the M2 (two threads incrementing counters [gap]
/// bytes apart, best of 7): both on P-cores, 64 bytes apart is as good as
/// 128 (2.0 ns per increment); with an E-core involved, 64 interferes (5-7
/// ns) and 128 does not (3 ns). Each core's L1 has 64-byte lines, the
/// cluster L2 and the system cache 128-byte ones, which is what [sysctl
/// hw.cachelinesize] reports. The logger thread is the kind macOS puts on
/// an E-core, so 128 is needed. On x86, lines are 64 bytes, but the L2
/// prefetcher fetches them in aligned 128-byte pairs.
///
/// TODO(Profile L): measure the same on the Core Ultra 9 185H, with the
/// threads on P-cores, on E-cores, and one on each, and record whether 64
/// bytes apart interferes there. Until then 128 on x86 rests on the
/// documented prefetcher pairing, not on a measurement.
///
/// TODO(test): [core_spsc_ring.t.cpp] must check this constant, in the
/// test file, where GCC does not warn: that it is a power of two, and at
/// least [std::hardware_constructive_interference_size] (64 here). That
/// catches a bad edit without putting the standard constant in a layout.
inline constexpr std::size_t k_CACHE_LINE_SIZE = 128;

                         // ==============
                         // class SpscRing
                         // ==============

template <class t_TYPE, std::size_t t_CAPACITY>
    requires SpscRingParameters<t_TYPE, t_CAPACITY>
class SpscRing final {
  private:
    // PRIVATE CLASS CONSTANTS

    static constexpr std::uint64_t k_MASK = t_CAPACITY - 1;

    static_assert(std::atomic_uint64_t::is_always_lock_free,
                  "the ring's counters SHALL be lock-free atomics");

    // DATA

    alignas(k_CACHE_LINE_SIZE) std::atomic_uint64_t d_head{0};
    std::uint64_t d_cached_tail = 0;

    alignas(k_CACHE_LINE_SIZE) std::atomic_uint64_t d_tail{0};
    std::uint64_t d_cached_head = 0;

    alignas(std::max(k_CACHE_LINE_SIZE, alignof(t_TYPE)))
        std::array<std::byte, t_CAPACITY * sizeof(t_TYPE)> d_slots{};

    // PRIVATE MANIPULATORS

    std::byte* slot(const std::uint64_t counter) noexcept;

    // NOT IMPLEMENTED

    SpscRing(const SpscRing&)            = delete;
    SpscRing& operator=(const SpscRing&) = delete;

  public:
    // CLASS METHODS

    [[nodiscard]] static constexpr std::size_t capacity() noexcept;

    // CREATORS

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
    /// all. They become visible to the consumer together, with one release.
    /// Call from the producer thread only.
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
    const std::uint64_t head = d_head.load(std::memory_order_relaxed);
    if (head - d_cached_tail == t_CAPACITY) {
        d_cached_tail = d_tail.load(std::memory_order_acquire);
        if (head - d_cached_tail == t_CAPACITY) {
            return false;                                             // RETURN
        }
    }

    std::memcpy(slot(head), std::addressof(value), sizeof(t_TYPE));

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

    const std::uint64_t count = values.size();
    if (count > t_CAPACITY) {
        return false;                                                 // RETURN
    }

    const std::uint64_t head = d_head.load(std::memory_order_relaxed);
    if (count > t_CAPACITY - (head - d_cached_tail)) {
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

    d_head.store(head + count, std::memory_order_release);
    return true;
}

template <class t_TYPE, std::size_t t_CAPACITY>
    requires SpscRingParameters<t_TYPE, t_CAPACITY>
inline std::optional<t_TYPE> SpscRing<t_TYPE, t_CAPACITY>::try_pop() noexcept
{
    const std::uint64_t tail = d_tail.load(std::memory_order_relaxed);
    if (tail == d_cached_head) {
        d_cached_head = d_head.load(std::memory_order_acquire);
        if (tail == d_cached_head) {
            return std::nullopt;
        }
    }

    std::array<std::byte, sizeof(t_TYPE)> bytes;
    std::memcpy(bytes.data(), slot(tail), bytes.size());

    d_tail.store(tail + 1, std::memory_order_release);
    return std::bit_cast<t_TYPE>(bytes);
}

}  // close namespace spsc
}  // close namespace engine::core

#endif
