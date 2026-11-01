// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_record.t.cpp                                              -*-C++-*-

//@PURPOSE: Verify that a 'LogRecord' packs its type, severity, ticks and
// payload into 16 bytes without any of them disturbing another, and gives
// each back exactly.
//
//@DESCRIPTION: Every log call makes one of these on the thread that logs, and
// the logger thread reads it back up to a millisecond later, so what comes
// out has to be what went in. Four properties have to hold:
//
//: o The fields do not collide. Each of the type, the severity and the ticks
//:   is read from its own bits, whatever the others hold. Every field is
//:   tested at a value with bits set in each place, and the severity up to
//:   7, which no enumerator names but the field can hold: with the values
//:   the enumerators give (0, 1, ...) a wrong shift or mask could not show.
//:
//: o The ticks are cut to 59 bits and no more: the bits above them are lost
//:   without reaching the severity or the type.
//:
//: o The payload comes back whole, all 64 bits, as the type it was made
//:   from, and a record of one type does not pass for the other.
//:
//: o The record is 16 bytes and trivially copyable, which is what lets the
//:   ring copy it as bytes, and eight of them fill one cache line.
//
// A record cannot be made from a number, or with a type the caller chose;
// those are compile-time rules, checked by the snippets in
// 'ci/test/compile_fail/'.

#include <engine/core/core_log_record.h>

#include <engine/core/core_spsc_ring.h>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace {

using engine::core::error::Error;
using engine::core::log::LogMessage;
using engine::core::log::LogRecord;
using engine::core::log::LogRecordType;
using engine::core::log::Severity;
using engine::core::spsc::k_CACHE_LINE_SIZE;

/// The two words of 'record', header first, as the ring copies them.
std::array<std::uint64_t, 2> words_of(const LogRecord& record)
{
    return std::bit_cast<std::array<std::uint64_t, 2>>(record);
}

/// Return the severity whose value is 'bits', whether an enumerator has it
/// or not: the field is 3 bits wide, so 0 to 7.
constexpr Severity severity_with(const std::uint32_t bits)
{
    return static_cast<Severity>(bits);
}

/// The ticks the field can hold, and the bits above them.
constexpr std::uint64_t k_TICK_LIMIT = std::uint64_t{1}
                                       << LogRecord::k_TICK_BITS;

/// A small pseudo-random sequence that is the same on every run, so that a
/// failure reproduces.
class Sequence {
  private:
    std::uint64_t d_state = 0x9E37'79B9'7F4A'7C15U;

  public:
    std::uint64_t next()
    {
        d_state ^= d_state << 13U;
        d_state ^= d_state >> 7U;
        d_state ^= d_state << 17U;
        return d_state;
    }
};

// ----------------------------------------------------------------- cost -----

TEST(CoreLogRecord, IsExactlySixteenBytes)
{
    // Pushed by value into the ring on every log call: that stops being cheap
    // the moment it grows, or needs more than a copy of its bytes.
    EXPECT_EQ(sizeof(LogRecord), 16U);
    EXPECT_EQ(alignof(LogRecord), alignof(std::uint64_t));
    EXPECT_TRUE(std::is_trivially_copyable_v<LogRecord>);
    EXPECT_TRUE(std::is_trivially_destructible_v<LogRecord>);
    EXPECT_TRUE(std::is_trivially_default_constructible_v<LogRecord>);
}

TEST(CoreLogRecord, EightRecordsFillOneCacheLine)
{
    EXPECT_EQ(8 * sizeof(LogRecord), k_CACHE_LINE_SIZE);
}

TEST(CoreLogRecord, SeverityAndTypeFitTheirFields)
{
    // 3 bits and 2 bits; the enumerations are checked against them when the
    // header is compiled, and the field widths are what the layout shows.
    EXPECT_LE(static_cast<std::uint32_t>(Severity::e_COUNT), 8U);
    EXPECT_LE(static_cast<std::uint32_t>(LogRecordType::e_COUNT), 4U);
    EXPECT_EQ(LogRecord::k_TICK_BITS + 3U + 2U, 64U);
}

// -------------------------------------------------------------- packing -----

TEST(CoreLogRecord, PinsTheLayoutTheDocumentationShows)
{
    // header  [ type : 2 ][ severity : 3 ][ ticks : 59 ]
    // payload [                 64 bits                ]
    const LogMessage message = LogMessage::from_raw(0xFEDC'BA98'7654'3210ULL);
    const LogRecord  record  = LogRecord::make(Severity::e_ERROR,
                                               0x0123'4567'89AB'CDEFULL,
                                               message);

    // Type 1 (a message) in bits 62..63 is 0x4000'0000'0000'0000, severity 4
    // in bits 59..61 is 0x2000'0000'0000'0000, and the 59 bits of ticks are
    // what is left of 0x0123'4567'89AB'CDEF.
    EXPECT_EQ(words_of(record)[0], 0x6123'4567'89AB'CDEFULL);
    EXPECT_EQ(words_of(record)[1], 0xFEDC'BA98'7654'3210ULL);
}

TEST(CoreLogRecord, EveryTypeAndSeverityComesBackWhateverTheTicksHold)
{
    // Each severity the field can hold, for each type, over ticks that set
    // each of the places the neighbouring fields could leak into.
    constexpr std::array<std::uint64_t, 7> ticks = {
        0,
        1,
        k_TICK_LIMIT >> 1U,
        k_TICK_LIMIT - 1,
        k_TICK_LIMIT,
        k_TICK_LIMIT + 1,
        ~std::uint64_t{0},
    };
    const Error      error   = Error::from_raw(0xAAAA'5555'AAAA'5555ULL);
    const LogMessage message = LogMessage::from_raw(0x5555'AAAA'5555'AAAAULL);

    for (std::uint32_t bits = 0; bits < 8; ++bits) {
        for (const std::uint64_t tick : ticks) {
            const LogRecord of_error = LogRecord::make(severity_with(bits),
                                                       tick,
                                                       error);
            EXPECT_EQ(of_error.type(), LogRecordType::e_ERROR);
            EXPECT_EQ(static_cast<std::uint32_t>(of_error.severity()), bits)
                << "ticks " << tick;
            EXPECT_EQ(of_error.ticks(), tick % k_TICK_LIMIT);

            const LogRecord of_message = LogRecord::make(severity_with(bits),
                                                         tick,
                                                         message);
            EXPECT_EQ(of_message.type(), LogRecordType::e_MESSAGE);
            EXPECT_EQ(static_cast<std::uint32_t>(of_message.severity()), bits)
                << "ticks " << tick;
            EXPECT_EQ(of_message.ticks(), tick % k_TICK_LIMIT);
        }
    }
}

TEST(CoreLogRecord, TicksAreExactBelowTheLimit)
{
    const Error error = Error::from_raw(1);

    for (const std::uint64_t tick : {
             std::uint64_t{0},
             std::uint64_t{1},
             std::uint64_t{0x0123'4567'89AB'CDEF},
             k_TICK_LIMIT >> 1U,
             k_TICK_LIMIT - 1,
         }) {
        EXPECT_EQ(LogRecord::make(Severity::e_INFO, tick, error).ticks(),
                  tick);
    }
}

TEST(CoreLogRecord, TicksAboveTheLimitAreLostWithoutReachingOtherFields)
{
    // All the bits above the 59 are set; the type and the severity, at their
    // lowest, must stay at their lowest.
    const Error     error  = Error::from_raw(0);
    const LogRecord record = LogRecord::make(Severity::e_TRACE,
                                             ~std::uint64_t{0},
                                             error);

    EXPECT_EQ(record.ticks(), k_TICK_LIMIT - 1);
    EXPECT_EQ(record.type(), LogRecordType::e_ERROR);
    EXPECT_EQ(record.severity(), Severity::e_TRACE);
    EXPECT_EQ(words_of(record)[0], k_TICK_LIMIT - 1);
}

TEST(CoreLogRecord, AFieldsHighestValueDoesNotReachItsNeighbour)
{
    // The severity at 7 (all three bits) beside ticks of 0 and a type of 0,
    // and ticks of all ones beside a severity and a type of 0.
    const Error error = Error::from_raw(0);

    const LogRecord high_severity = LogRecord::make(severity_with(7),
                                                    0,
                                                    error);
    EXPECT_EQ(high_severity.ticks(), 0U);
    EXPECT_EQ(high_severity.type(), LogRecordType::e_ERROR);
    EXPECT_EQ(words_of(high_severity)[0], std::uint64_t{7} << 59U);

    const LogMessage message  = LogMessage::from_raw(0);
    const LogRecord high_type = LogRecord::make(Severity::e_TRACE, 0, message);
    EXPECT_EQ(high_type.ticks(), 0U);
    EXPECT_EQ(high_type.severity(), Severity::e_TRACE);
    EXPECT_EQ(words_of(high_type)[0], std::uint64_t{1} << 62U);
}

// -------------------------------------------------------------- payload -----

TEST(CoreLogRecord, ErrorComesBackWhole)
{
    for (const std::uint64_t raw : {
             0x0000'0000'0000'0000ULL,
             0xFFFF'FFFF'FFFF'FFFFULL,
             0x8000'0000'0000'0001ULL,
             0xFEDC'BA98'7654'3210ULL,
         }) {
        const Error     error  = Error::from_raw(raw);
        const LogRecord record = LogRecord::make(Severity::e_WARN, 5, error);

        EXPECT_EQ(record.error(), error) << raw;
        EXPECT_EQ(words_of(record)[1], raw);
    }
}

TEST(CoreLogRecord, MessageComesBackWhole)
{
    for (const std::uint64_t raw : {
             0x0000'0000'0000'0000ULL,
             0xFFFF'FFFF'FFFF'FFFFULL,
             0x8000'0000'0000'0001ULL,
             0xFEDC'BA98'7654'3210ULL,
         }) {
        const LogMessage message = LogMessage::from_raw(raw);
        const LogRecord record = LogRecord::make(Severity::e_WARN, 5, message);

        EXPECT_EQ(record.message(), message) << raw;
        EXPECT_EQ(words_of(record)[1], raw);
    }
}

TEST(CoreLogRecord, TheSameBitsAsAnErrorAndAsAMessageAreTwoRecords)
{
    // A message and an error can hold the same 64 bits. What tells them apart
    // is the type, which the factory sets and the payload cannot.
    const std::uint64_t raw      = 0x0123'4567'89AB'CDEFULL;
    const LogRecord     of_error = LogRecord::make(Severity::e_INFO,
                                                   7,
                                                   Error::from_raw(raw));
    const LogRecord of_message   = LogRecord::make(Severity::e_INFO,
                                                   7,
                                                   LogMessage::from_raw(raw));

    EXPECT_NE(of_error.type(), of_message.type());
    EXPECT_NE(words_of(of_error)[0], words_of(of_message)[0]);
    EXPECT_EQ(words_of(of_error)[1], words_of(of_message)[1]);
}

TEST(CoreLogRecord, AgreesWithTheFieldsOnRandomInput)
{
    // 100,000 records from random severities, ticks and payloads: each field
    // is what it was made from, and none of them changed another.
    Sequence random;

    for (std::int32_t i = 0; i < 100'000; ++i) {
        const std::uint32_t bits = static_cast<std::uint32_t>(random.next() %
                                                              8);
        const std::uint64_t tick = random.next();
        const std::uint64_t payload = random.next();

        if (i % 2 == 0) {
            const Error     error  = Error::from_raw(payload);
            const LogRecord record = LogRecord::make(severity_with(bits),
                                                     tick,
                                                     error);
            ASSERT_EQ(record.type(), LogRecordType::e_ERROR);
            ASSERT_EQ(record.error(), error);
            ASSERT_EQ(static_cast<std::uint32_t>(record.severity()), bits);
            ASSERT_EQ(record.ticks(), tick % k_TICK_LIMIT);
        } else {
            const LogMessage message = LogMessage::from_raw(payload);
            const LogRecord  record  = LogRecord::make(severity_with(bits),
                                                       tick,
                                                       message);
            ASSERT_EQ(record.type(), LogRecordType::e_MESSAGE);
            ASSERT_EQ(record.message(), message);
            ASSERT_EQ(static_cast<std::uint32_t>(record.severity()), bits);
            ASSERT_EQ(record.ticks(), tick % k_TICK_LIMIT);
        }
    }
}

// ----------------------------------------------------------------- misc -----

TEST(CoreLogRecord, IsUsableInAConstantExpression)
{
    constexpr Error     error  = Error::make(Error::Kind::e_UNKNOWN,
                                             Error::CoreReason::e_UNKNOWN,
                                             7U);
    constexpr LogRecord record = LogRecord::make(Severity::e_FATAL, 99, error);

    static_assert(record.type() == LogRecordType::e_ERROR);
    static_assert(record.severity() == Severity::e_FATAL);
    static_assert(record.ticks() == 99U);
    static_assert(record.error() == error);

    SUCCEED();
}

TEST(CoreLogRecord, CopiesAsBytes)
{
    // What the ring does with it, and what a copy must preserve.
    const LogRecord original = LogRecord::make(severity_with(5),
                                               0x0123'4567'89AB'CDEFULL,
                                               LogMessage::from_raw(9));

    std::array<std::byte, sizeof(LogRecord)> bytes{};
    std::memcpy(bytes.data(), &original, bytes.size());
    const auto copy = std::bit_cast<LogRecord>(bytes);

    EXPECT_EQ(words_of(copy), words_of(original));
    EXPECT_EQ(copy.type(), LogRecordType::e_MESSAGE);
    EXPECT_EQ(copy.message(), original.message());
}

// ------------------------------------------------------------ preconditions
// ---

// GoogleTest runs suites named '*DeathTest' first, before any threads exist,
// which is what makes forking the process to watch it die safe.
TEST(CoreLogRecordDeathTest, ErrorOfAMessageRecordIsRejected)
{
    // Reading a message's bits as an error gives a plausible, wrong error.
    // The assert in 'error' is the only thing that catches it, so this proves
    // it fires. Release builds define NDEBUG, which compiles the assert out,
    // so there the test is reported as skipped, not as passed.
#ifdef NDEBUG
    GTEST_SKIP() << "assert() is compiled out of this build: this test checks "
                    "nothing here, and runs in a debug build";
#endif
    const LogRecord record = LogRecord::make(Severity::e_INFO,
                                             1,
                                             LogMessage::from_raw(5));

    EXPECT_DEATH((void)record.error(), "");
}

TEST(CoreLogRecordDeathTest, MessageOfAnErrorRecordIsRejected)
{
#ifdef NDEBUG
    GTEST_SKIP() << "assert() is compiled out of this build: this test checks "
                    "nothing here, and runs in a debug build";
#endif
    const LogRecord record = LogRecord::make(Severity::e_INFO,
                                             1,
                                             Error::from_raw(5));

    EXPECT_DEATH((void)record.message(), "");
}

}  // close unnamed namespace
