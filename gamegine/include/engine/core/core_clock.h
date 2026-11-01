// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_clock.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_CLOCK_H
#define INCLUDED_ENGINE_CORE_CLOCK_H

//@PURPOSE: Provide a monotonic clock and the conversion of its ticks to time.
//
//@CLASSES:
//  engine::core::tic::Clock: this platform's cheapest monotonic tick counter
//  engine::core::tic::TickConversion: ticks to nanoseconds since a start tick
//
//@SEE_ALSO: core_log
//
//@DESCRIPTION: [Clock::now] returns raw ticks, as cheaply as the platform
// allows, because every log call of every thread reads it. What a tick is
// worth depends on the platform, so turning ticks into time is left to a
// [TickConversion], captured once when the logger starts and used on the
// logger thread only:
// ```
//  platform  source                                 one tick       now()
//  macOS     mach_absolute_time                     125/3 ns (M*)  5.2 ns
//  Linux     clock_gettime(CLOCK_MONOTONIC), vDSO   1 ns           ~20 ns
//  other     std::chrono::steady_clock              its period     ~14 ns
// ```
// The costs were measured on the M2, best of 9; the Linux one is typical,
// not measured (TODO(Profile L): measure it). On macOS and Linux the clock
// does not count time the machine spends asleep.
//
///Conversion
///----------
// A tick is [num / denom] nanoseconds, both terms 32-bit. The elapsed
// nanoseconds, [elapsed * num / denom], are computed split:
// ```
//  elapsed / denom * num  +  elapsed % denom * num / denom
// ```
// The remainder is below [denom], so its product with [num] is below 2^64
// and never overflows: the result is exact, rounded down, as long as it fits
// in 64 bits, which is 584 years of nanoseconds.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Stamping on One Thread, Converting on Another
/// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// ```
//  const TickConversion conversion = TickConversion::start_now();  // startup
//  ...
//  const std::uint64_t ticks = Clock::now();  // any thread, when it logs
//  ...
//  const std::uint64_t ns = conversion.ns_elapsed_since_start(ticks);
// ```

// std
#include <cassert>
#include <cstdint>

#ifdef __APPLE__
#include <mach/mach_time.h>
#elifdef __linux__
#include <time.h>
#else
#include <chrono>
#include <limits>
#include <ratio>
#endif

namespace engine::core {
namespace tic {

                         // ============
                         // struct Clock
                         // ============

/// This utility [struct] reads this platform's monotonic tick counter; see
/// the component documentation for what a tick is on each platform.
struct Clock final {
    // CLASS METHODS

    /// Return the current value of this platform's monotonic tick counter,
    /// in ticks, not nanoseconds. A later call never returns less.
    [[nodiscard("Clock reading shall not be ignored.")]] static std::uint64_t
    now() noexcept;
};

                         // ====================
                         // class TickConversion
                         // ====================

/// This value-semantic class converts ticks of [Clock] into the nanoseconds
/// elapsed since a start tick, one tick being [num() / denom()] nanoseconds.
class TickConversion final {
  private:
    // DATA

    std::uint64_t d_start;  // tick that converts to 0 ns
    std::uint32_t d_num;    // nanoseconds per tick: numerator
    std::uint32_t d_denom;  // nanoseconds per tick: denominator, never 0

  public:
    // CLASS METHODS

    /// Return the conversion for this platform's [Clock], starting at the
    /// current tick.
    [[nodiscard]] static TickConversion start_now() noexcept;

    // CREATORS

    /// Create a [TickConversion] object converting ticks into the
    /// nanoseconds elapsed since the specified [start] tick, one tick being
    /// the specified [num] over the specified [denom] nanoseconds. The
    /// behavior is undefined unless [0 < denom].
    constexpr TickConversion(const std::uint64_t start,
                             const std::uint32_t num,
                             const std::uint32_t denom) noexcept;

    /// Create a [TickConversion] object having the same value as the
    /// specified [original] object.
    //! TickConversion(const TickConversion& original) = default;

    /// Destroy this object.
    //! ~TickConversion() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! TickConversion& operator=(const TickConversion& rhs) = default;

    // ACCESSORS

    /// Return the nanoseconds from the start tick to the specified [ticks],
    /// rounded down, or 0 if [ticks] is not after the start. The result is
    /// exact as long as it fits in 64 bits.
    [[nodiscard]] constexpr std::uint64_t
    ns_elapsed_since_start(const std::uint64_t ticks) const noexcept;

    /// Return the tick that converts to 0 ns.
    [[nodiscard]] constexpr std::uint64_t start() const noexcept;

    /// Return the numerator of the nanoseconds per tick.
    [[nodiscard]] constexpr std::uint32_t num() const noexcept;

    /// Return the denominator of the nanoseconds per tick.
    [[nodiscard]] constexpr std::uint32_t denom() const noexcept;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

                         // ------------
                         // struct Clock
                         // ------------

// CLASS METHODS

inline std::uint64_t Clock::now() noexcept
{
#ifdef __APPLE__
    return mach_absolute_time();
#elifdef __linux__
    // Cannot fail: CLOCK_MONOTONIC exists on every Linux since 2.6, and [ts]
    // is on the stack.
    constexpr std::uint64_t k_NS_PER_SECOND = 1'000'000'000U;

    timespec ts{};
    (void)::clock_gettime(CLOCK_MONOTONIC, &ts);

    return (static_cast<std::uint64_t>(ts.tv_sec) * k_NS_PER_SECOND) +
           static_cast<std::uint64_t>(ts.tv_nsec);
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

                         // --------------------
                         // class TickConversion
                         // --------------------

// CLASS METHODS

inline TickConversion TickConversion::start_now() noexcept
{
#ifdef __APPLE__
    // Cannot fail in practice: it copies a ratio the kernel publishes at
    // boot. On Apple silicon it is 125/3.
    mach_timebase_info_data_t timebase{};
    (void)::mach_timebase_info(&timebase);

    return TickConversion(Clock::now(), timebase.numer, timebase.denom);
#elifdef __linux__
    // [CLOCK_MONOTONIC] counts nanoseconds: one tick is 1/1 ns.
    return TickConversion(Clock::now(), 1, 1);
#else
    // Nanoseconds per tick, as a reduced fraction.
    using TickInNanoseconds =
        std::ratio_divide<std::chrono::steady_clock::period, std::nano>;

    // Both terms are narrowed to 32 bits below: a clock whose ratio does not
    // fit is a build error here, not a wrong timestamp later.
    static_assert(TickInNanoseconds::num > 0 &&
                      TickInNanoseconds::num <=
                          std::numeric_limits<std::uint32_t>::max() &&
                      TickInNanoseconds::den <=
                          std::numeric_limits<std::uint32_t>::max(),
                  "the steady clock's tick must fit a 32-bit ratio");

    return TickConversion(Clock::now(),
                          static_cast<std::uint32_t>(TickInNanoseconds::num),
                          static_cast<std::uint32_t>(TickInNanoseconds::den));
#endif
}

// CREATORS

inline constexpr TickConversion::TickConversion(
    const std::uint64_t start,
    const std::uint32_t num,
    const std::uint32_t denom) noexcept
: d_start(start)
, d_num(num)
, d_denom(denom)
{
    assert(denom > 0);
}

// ACCESSORS

inline constexpr std::uint64_t TickConversion::ns_elapsed_since_start(
    const std::uint64_t ticks) const noexcept
{
    if (ticks <= d_start) {
        return 0;                                                     // RETURN
    }

    // [elapsed * d_num / d_denom], split so that no product overflows; see
    // the component documentation.
    const std::uint64_t elapsed    = ticks - d_start;
    const std::uint64_t elapsed_ns = (elapsed / d_denom * d_num) +
                                     (elapsed % d_denom * d_num / d_denom);
    return elapsed_ns;
}

inline constexpr std::uint64_t TickConversion::start() const noexcept
{
    return d_start;
}

inline constexpr std::uint32_t TickConversion::num() const noexcept
{
    return d_num;
}

inline constexpr std::uint32_t TickConversion::denom() const noexcept
{
    return d_denom;
}

}  // close namespace tic
}  // close namespace engine::core

#endif
