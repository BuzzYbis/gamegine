// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_clock.t.cpp                                                   -*-C++-*-

//@PURPOSE: Verify that 'Clock' never runs backwards and that 'TickConversion'
// turns its ticks into nanoseconds exactly, at every ratio and without
// overflowing.
//
//@DESCRIPTION: Every log record is stamped with 'Clock::now()' on the thread
// that logs it, and turned into time later by a 'TickConversion' captured
// when the logger starts. Four properties have to hold:
//
//: o The clock is monotonic: a later read is never smaller, or two records
//:   would appear in the wrong order.
//:
//: o The conversion is exact: 'ns_elapsed_since_start' equals the exact
//:   'elapsed * numerator / denominator', rounded down, computed here in 128
//:   bits, for every ratio a clock can have and tick counts up to the largest
//:   whose answer fits in 64 bits. The product never overflows on the way.
//:
//: o The conversion is the platform's: a sleep measured with 'Clock' and with
//:   'std::chrono::steady_clock' agrees, which catches a ratio that is
//:   inverted or wrong.
//:
//: o A tick at or before the start converts to 0, not to a wrapped-around
//:   value.

#include <engine/core/core_clock.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <thread>

namespace {

using engine::core::tic::Clock;
using engine::core::tic::TickConversion;

// A 128-bit unsigned integer: a GCC and clang extension, marked as one so
// that -Wpedantic accepts it.
__extension__ using Wide = unsigned __int128;

/// The largest 64-bit value, as a [Wide].
constexpr Wide k_UINT64_MAX = std::numeric_limits<std::uint64_t>::max();

/// Return the exact 'elapsed * numerator / denominator', rounded down.
Wide exact_ns(const std::uint64_t elapsed,
              const std::uint32_t numerator,
              const std::uint32_t denominator)
{
    return Wide{elapsed} * numerator / denominator;
}

// ----------------------------------------------------------------- clock ---

TEST(CoreClock, NeverRunsBackwards)
{
    // A million reads in a row on one thread.
    std::uint64_t previous = Clock::now();
    for (int i = 0; i < 1'000'000; ++i) {
        const std::uint64_t current = Clock::now();
        ASSERT_GE(current, previous) << "read " << i;
        previous = current;
    }
}

TEST(CoreClock, AdvancesWhileTimePasses)
{
    const std::uint64_t before = Clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const std::uint64_t after = Clock::now();

    EXPECT_GT(after, before);
}

// ----------------------------------------------------------- conversion ---

TEST(CoreTickConversion, IsExactAtEveryRatio)
{
    // The ratios a clock has: nanoseconds (Linux), Apple silicon's 125/3,
    // a slow tick, a fast one, and both terms at their 32-bit maximum.
    constexpr std::uint32_t k_MAX = std::numeric_limits<std::uint32_t>::max();
    struct Ratio {
        std::uint32_t numerator;
        std::uint32_t denominator;
    };
    constexpr std::array ratios = {
        Ratio{1, 1},
        Ratio{125, 3},
        Ratio{1'000'000'000, 1},
        Ratio{1, 1'000},
        Ratio{k_MAX, 1},
        Ratio{1, k_MAX},
        Ratio{k_MAX, k_MAX - 1},
        Ratio{k_MAX - 1, k_MAX},
    };

    for (const Ratio& ratio : ratios) {
        const TickConversion clock(1'000, ratio.numerator, ratio.denominator);

        // The largest elapsed tick count whose answer fits in 64 bits.
        const Wide largest = ((k_UINT64_MAX * ratio.denominator) +
                              ratio.denominator - 1) /
                             ratio.numerator;
        const auto limit   = static_cast<std::uint64_t>(
            std::min(largest, k_UINT64_MAX - 1'000));

        // Small counts, counts around multiples of the denominator, and
        // counts spread over the whole range up to the limit.
        const std::array<std::uint64_t, 6> fixed = {
            0,
            1,
            ratio.denominator - 1ULL,
            ratio.denominator + 1ULL,
            limit - 1,
            limit,
        };
        std::size_t checked = 0;
        for (const std::uint64_t elapsed : fixed) {
            if (elapsed > limit) {
                continue;
            }
            ++checked;
            EXPECT_EQ(Wide{clock.ns_elapsed_since_start(1'000 + elapsed)},
                      exact_ns(elapsed, ratio.numerator, ratio.denominator))
                << ratio.numerator << "/" << ratio.denominator << ", elapsed "
                << elapsed;
        }
        // The ones above the limit are left out; most must remain, or the
        // loop would pass by checking nothing.
        EXPECT_GE(checked, 4U) << ratio.numerator << "/" << ratio.denominator;

        // Steps of 2^64 divided by the golden ratio land evenly over the
        // range without repeating, and the same counts every run, so a
        // failure reproduces.
        constexpr std::uint64_t k_STEP = 0x9E37'79B9'7F4A'7C15U;
        for (std::uint64_t i = 0; i < 100'000; ++i) {
            const std::uint64_t spread = i * k_STEP;  // wraps, on purpose
            const auto elapsed = static_cast<std::uint64_t>(Wide{spread} %
                                                            (Wide{limit} + 1));
            ASSERT_EQ(Wide{clock.ns_elapsed_since_start(1'000 + elapsed)},
                      exact_ns(elapsed, ratio.numerator, ratio.denominator))
                << ratio.numerator << "/" << ratio.denominator << ", elapsed "
                << elapsed;
        }
    }
}

TEST(CoreTickConversion, TicksAtOrBeforeTheStartAreZero)
{
    const TickConversion clock(1'000, 125, 3);

    EXPECT_EQ(clock.ns_elapsed_since_start(1'000), 0U);
    EXPECT_EQ(clock.ns_elapsed_since_start(999), 0U);
    EXPECT_EQ(clock.ns_elapsed_since_start(0), 0U);
}

TEST(CoreTickConversion, KeepsWhatItWasMadeWith)
{
    const TickConversion clock(42, 125, 3);

    EXPECT_EQ(clock.start(), 42U);
    EXPECT_EQ(clock.num(), 125U);
    EXPECT_EQ(clock.denom(), 3U);
}

TEST(CoreTickConversion, IsUsableInAConstantExpression)
{
    // 3 ticks of 125/3 ns are exactly 125 ns; 4 ticks are 166.67, rounded
    // down to 166.
    constexpr TickConversion clock(10, 125, 3);
    static_assert(clock.ns_elapsed_since_start(13) == 125U);
    static_assert(clock.ns_elapsed_since_start(14) == 166U);

    SUCCEED();
}

// -------------------------------------------------------------- platform ---

TEST(CoreTickConversion, StartsNowWithAUsableRatio)
{
    const std::uint64_t  before = Clock::now();
    const TickConversion clock  = TickConversion::start_now();
    const std::uint64_t  after  = Clock::now();

    EXPECT_GE(clock.start(), before);
    EXPECT_LE(clock.start(), after);
    EXPECT_GT(clock.num(), 0U);
    EXPECT_GT(clock.denom(), 0U);
}

TEST(CoreTickConversion, AgreesWithTheStandardSteadyClock)
{
    // The same 50 ms sleep, timed both ways. The reads are not simultaneous,
    // so they differ by microseconds, or by a scheduling quantum on a busy
    // machine: no run in 200 (half of them with 16 busy processes on 8 cores)
    // came near 1%. A wrong ratio is off by far more: an inverted 125/3 by a
    // factor of 1,736, ticks taken for nanoseconds by 42. So 10% is a margin
    // that costs nothing in what it can detect.
    const TickConversion clock = TickConversion::start_now();

    const auto          steady_before = std::chrono::steady_clock::now();
    const std::uint64_t ticks_before  = Clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::uint64_t ticks_after  = Clock::now();
    const auto          steady_after = std::chrono::steady_clock::now();

    const auto by_clock = static_cast<double>(
        clock.ns_elapsed_since_start(ticks_after) -
        clock.ns_elapsed_since_start(ticks_before));
    const auto by_steady = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(steady_after -
                                                             steady_before)
            .count());

    EXPECT_NEAR(by_clock, by_steady, by_steady * 0.1);
}

// ------------------------------------------------------------ preconditions
// ---

// GoogleTest runs suites named '*DeathTest' first, before any threads exist,
// which is what makes forking the process to watch it die safe.
TEST(CoreTickConversionDeathTest, RejectsAZeroDenominator)
{
    // Every conversion divides by it. Release builds compile the assert out,
    // so there the test is reported as skipped, not as passed.
#ifdef NDEBUG
    GTEST_SKIP() << "assert() is compiled out of this build: this test checks "
                    "nothing here, and runs in a debug build";
#endif
    EXPECT_DEATH((void)TickConversion(0, 1, 0), "");
}

}  // close unnamed namespace
