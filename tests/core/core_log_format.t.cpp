// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_format.t.cpp                                              -*-C++-*-

//@PURPOSE: Verify that 'LogFormat' writes a record as one line, whose columns
// line up and whose text is that of its error or message.
//
//@DESCRIPTION: The logger thread turns each record into a line, and a person
// reads the lines, so what matters is that they are exact and that they line
// up. Five properties have to hold:
//
//: o The prefix is what the documentation shows: the seconds since the start
//:   in four characters or more, the microseconds in six, the severity in
//:   five, the thread in eight, so that the message starts in the same column
//:   on every line. Cutting a long thread name, padding a short one and
//:   dropping what is below a microsecond are each tested at their edges, and
//:   the whole prefix is compared with 'std::format' on random input, which is
//:   written here and shares nothing with the header.
//:
//: o An error record is its prefix, its error and a newline, and a message
//:   record is its prefix, its message and a newline: each is built here from
//:   the pieces ('ErrorFormat', 'LogMessageFormat') and compared, and a few
//:   lines are pinned as they appear in a log.
//:
//: o One function writes every record and chooses what follows the prefix
//:   from the type in the record, which the logger thread only knows after
//:   popping it. Both types, in mixed order, and a type no factory makes (a
//:   line of its own with a '?') are checked.
//:
//: o A record that does not fit is cut, never overrun: at every capacity, from
//:   nothing to the whole line, what was written is the start of the line,
//:   the writer says it was cut if and only if it was, and the byte after the
//:   buffer is untouched.
//:
//: o The line is one line: it ends with a single newline, holds no other
//:   control character, and is never longer than 'k_MAX_LINE_SIZE', which the
//:   logger's batch relies on.
//
// There is no wrong type to give it: the record says which it is.

#include <engine/core/core_log_format.h>

#include <engine/core/core_clock.h>
#include <engine/core/core_error.h>
#include <engine/core/core_error_format.h>
#include <engine/core/core_format.h>
#include <engine/core/core_log_message.h>
#include <engine/core/core_log_message_format.h>
#include <engine/core/core_log_record.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using engine::core::error::Error;
using engine::core::error::ErrorFormat;
using engine::core::format::LineWriter;
using engine::core::log::LogFormat;
using engine::core::log::LogMessage;
using engine::core::log::LogMessageFormat;
using engine::core::log::LogRecord;
using engine::core::log::Severity;
using engine::core::tic::TickConversion;

/// A start tick that is not zero, so that taking it off is tested.
constexpr std::uint64_t k_START = 1'000'000;

/// A clock whose tick is a nanosecond.
const TickConversion k_NANOSECONDS(k_START, 1, 1);

/// A clock like Apple's: 24 MHz, a tick being 125 / 3 nanoseconds.
const TickConversion k_APPLE(k_START, 125, 3);

/// The ticks of 'k_NANOSECONDS' that 'nanoseconds' after the start are.
constexpr std::uint64_t ticks_at(const std::uint64_t nanoseconds)
{
    return k_START + nanoseconds;
}

constexpr std::uint64_t k_SECOND = 1'000'000'000;

/// The names of the severities, written here and not read from the header.
constexpr std::array<std::string_view, 6> k_NAMES = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

/// Room for any line, so that nothing is cut.
constexpr std::size_t k_ROOMY = 1024;

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

// ------------------------------------------------------------- helpers -----

/// What a writer of a given capacity ended up with.
struct Written {
    std::string text;       // what it wrote
    bool        truncated;  // whether it said it was cut
    bool guard_intact;      // whether the bytes after the buffer are as left
};

/// Run 'append' on a writer over 'capacity' bytes, with guard bytes after
/// them, and return what happened.
template <class t_APPEND>
Written written_with(const std::size_t capacity, t_APPEND&& append)
{
    constexpr char        k_GUARD      = '#';
    constexpr std::size_t k_GUARD_SIZE = 32;

    std::vector<char> buffer(capacity + k_GUARD_SIZE, k_GUARD);
    LineWriter        writer(std::span<char>(buffer.data(), capacity));
    append(writer);

    Written result;
    result.text         = std::string(writer.view());
    result.truncated    = writer.is_truncated();
    result.guard_intact = std::all_of(
        buffer.begin() + static_cast<std::ptrdiff_t>(capacity),
        buffer.end(),
        [](const char c) { return c == k_GUARD; });
    return result;
}

/// The line 'append' writes when there is room for any.
template <class t_APPEND> std::string line_of(t_APPEND&& append)
{
    return written_with(k_ROOMY, append).text;
}

Error core_error(const std::uint32_t context)
{
    return Error::make(Error::Kind::e_UNKNOWN,
                       Error::CoreReason::e_UNKNOWN,
                       context);
}

Error vulkan_error(const std::uint32_t context)
{
    return Error::make(Error::Kind::e_UNKNOWN,
                       Error::VulkanReason::e_UNKNOWN,
                       context);
}

LogMessage core_message()
{
    return LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN);
}

LogMessage vulkan_message()
{
    return LogMessage::make(LogMessage::VulkanMessage::e_UNKNOWN);
}

/// The text of 'error' as 'ErrorFormat' writes it, without the raw value.
std::string error_text(const Error error)
{
    return line_of([error](LineWriter& writer) {
        ErrorFormat::append_error(writer,
                                  error,
                                  ErrorFormat::RawValue::e_OMIT);
    });
}

/// The text of 'message' as 'LogMessageFormat' writes it, without the raw
/// value.
std::string message_text(const LogMessage message)
{
    return line_of([message](LineWriter& writer) {
        LogMessageFormat::append_message(writer,
                                         message,
                                         LogMessageFormat::RawValue::e_OMIT);
    });
}

/// The prefix of a line, written with 'std::format' from the documentation
/// and from nothing in the header: the seconds in four characters or more,
/// the microseconds in six, the severity in five, the thread in eight.
std::string prefix_oracle(const std::uint64_t    nanoseconds,
                          const std::uint32_t    severity,
                          const std::string_view thread)
{
    return std::format("[{:4}.{:06}] {:<5} {:<8.8}",
                       nanoseconds / k_SECOND,
                       nanoseconds % k_SECOND / 1'000U,
                       k_NAMES[severity],
                       thread);
}

/// The record of an error, made 'nanoseconds' after the start of
/// 'k_NANOSECONDS'.
LogRecord error_record(const Severity      severity,
                       const std::uint64_t nanoseconds,
                       const Error         error)
{
    return LogRecord::make(severity, ticks_at(nanoseconds), error);
}

LogRecord message_record(const Severity      severity,
                         const std::uint64_t nanoseconds,
                         const LogMessage    message)
{
    return LogRecord::make(severity, ticks_at(nanoseconds), message);
}

/// A record whose type is the 2-bit 'type', whether an enumerator has it or
/// not: no factory makes the two the enumeration leaves, so the bits are set
/// by hand.
LogRecord record_of_type_bits(const std::uint32_t type,
                              const std::uint32_t severity,
                              const std::uint64_t ticks,
                              const std::uint64_t payload)
{
    const std::uint64_t header = (std::uint64_t{type} << 62U) |
                                 (std::uint64_t{severity} << 59U) | ticks;
    return std::bit_cast<LogRecord>(std::array<std::uint64_t, 2>{
        header,
        payload,
    });
}

std::string record_line(const LogRecord&       record,
                        const std::string_view thread,
                        const TickConversion&  clock = k_NANOSECONDS)
{
    return line_of([&](LineWriter& writer) {
        LogFormat::append_record(writer, record, thread, clock);
    });
}

/// The prefix of a line as 'append_prefix' writes it.
std::string prefix_line(const std::uint64_t    ticks,
                        const Severity         severity,
                        const std::string_view thread,
                        const TickConversion&  clock = k_NANOSECONDS)
{
    return line_of([&](LineWriter& writer) {
        LogFormat::append_prefix(writer, ticks, severity, thread, clock);
    });
}

std::string timestamp_line(const std::uint64_t   ticks,
                           const TickConversion& clock = k_NANOSECONDS)
{
    return line_of([&](LineWriter& writer) {
        LogFormat::append_timestamp(writer, ticks, clock);
    });
}

constexpr std::array<std::string_view, 5> k_THREADS = {
    "",
    "a",
    "render",
    "12345678",
    "streaming-worker",
};

/// One random record, with what its line must say.
struct Sample {
    LogRecord   record;
    bool        is_error;
    std::string expected;  // the whole line, newline included
    std::string thread;
};

Sample random_sample(Sequence& random)
{
    const std::uint32_t severity = static_cast<std::uint32_t>(random.next() %
                                                              6U);
    // Up to 2^57 nanoseconds, so that the ticks fit their 59 bits.
    const std::uint64_t    nanoseconds = (random.next() >> 7U) >>
                                         (random.next() % 40U);
    const std::string_view thread =
        k_THREADS[random.next() % k_THREADS.size()];
    const auto context = static_cast<std::uint32_t>(random.next());

    Sample sample;
    sample.thread   = std::string(thread);
    sample.is_error = (random.next() % 2U) == 0U;
    if (sample.is_error) {
        const Error error = (random.next() % 2U) == 0U ? core_error(context)
                                                       : vulkan_error(context);
        sample.record     = error_record(static_cast<Severity>(severity),
                                         nanoseconds,
                                         error);
        sample.expected = prefix_oracle(nanoseconds, severity, thread) + " " +
                          error_text(error) + "\n";
    } else {
        const LogMessage message = (random.next() % 2U) == 0U
                                       ? core_message()
                                       : vulkan_message();
        sample.record   = message_record(static_cast<Severity>(severity),
                                         nanoseconds,
                                         message);
        sample.expected = prefix_oracle(nanoseconds, severity, thread) + " " +
                          message_text(message) + "\n";
    }
    return sample;
}

// ----------------------------------------------------------- constants -----

TEST(CoreLogFormat, ColumnsAreTheWidthsTheDocumentationGives)
{
    EXPECT_EQ(LogFormat::k_SEVERITY_WIDTH, 5U);
    EXPECT_EQ(LogFormat::k_THREAD_WIDTH, 8U);
    EXPECT_EQ(LogFormat::k_MAX_LINE_SIZE, 512U);
}

TEST(CoreLogFormat, TheLongestLineFitsTheLongestLineSize)
{
    // The logger's batch keeps this much room for a record, so a line longer
    // than it is cut. The largest prefix is that of the largest tick (59
    // bits, a nanosecond each, so 9 digits of seconds) and the longest thread
    // name, followed by the longest error or message the formats can make.
    const TickConversion clock(0, 1, 1);
    const std::uint64_t  largest_tick = (std::uint64_t{1}
                                         << LogRecord::k_TICK_BITS) -
                                        1;
    const std::string    prefix       = prefix_line(largest_tick,
                                                    Severity::e_FATAL,
                                                    "12345678",
                                                    clock);

    EXPECT_EQ(prefix.size(), 33U);
    EXPECT_LE(prefix.size() + 1 + ErrorFormat::k_MAX_LINE_SIZE + 1,
              LogFormat::k_MAX_LINE_SIZE);
    EXPECT_LE(prefix.size() + 1 + LogMessageFormat::k_MAX_LINE_SIZE + 1,
              LogFormat::k_MAX_LINE_SIZE);
}

// ------------------------------------------------------------ severity -----

TEST(CoreLogFormat, SeveritiesHaveTheirNames)
{
    for (std::uint32_t i = 0; i < k_NAMES.size(); ++i) {
        EXPECT_EQ(LogFormat::severity_name(static_cast<Severity>(i)),
                  k_NAMES[i])
            << "severity " << i;
    }
}

TEST(CoreLogFormat, NoSeverityNamesAQuestionMark)
{
    // The sentinel, and the values of the 3-bit field that no enumerator
    // names.
    EXPECT_EQ(LogFormat::severity_name(Severity::e_COUNT), "?");
    for (const std::uint32_t bits : {6U, 7U, 8U, 255U}) {
        EXPECT_EQ(LogFormat::severity_name(static_cast<Severity>(bits)), "?")
            << "severity " << bits;
    }
}

TEST(CoreLogFormat, EveryNameFitsItsColumn)
{
    for (std::uint32_t i = 0; i <= 7; ++i) {
        EXPECT_LE(LogFormat::severity_name(static_cast<Severity>(i)).size(),
                  LogFormat::k_SEVERITY_WIDTH)
            << "severity " << i;
    }
}

TEST(CoreLogFormat, SeverityNameIsUsableInAConstantExpression)
{
    static_assert(LogFormat::severity_name(Severity::e_INFO) == "INFO");
    static_assert(LogFormat::severity_name(Severity::e_COUNT) == "?");
    SUCCEED();
}

// ----------------------------------------------------------- timestamp -----

TEST(CoreLogFormat, TimestampIsSecondsAndMicroseconds)
{
    struct Case {
        std::uint64_t nanoseconds;
        const char*   expected;
    };
    const auto cases = std::to_array<Case>({
        {0, "[   0.000000]"},
        {1'000, "[   0.000001]"},
        {999'000, "[   0.000999]"},
        {1'000'000, "[   0.001000]"},
        {999'999'000, "[   0.999999]"},
        {k_SECOND, "[   1.000000]"},
        {k_SECOND + 1'000, "[   1.000001]"},
        {(12 * k_SECOND) + 345'678'000, "[  12.345678]"},
        {9'999 * k_SECOND, "[9999.000000]"},
    });
    for (const Case& c : cases) {
        EXPECT_EQ(timestamp_line(ticks_at(c.nanoseconds)), c.expected)
            << c.nanoseconds << " ns";
    }
}

TEST(CoreLogFormat, WhatIsBelowAMicrosecondIsDroppedNotRounded)
{
    // A rounding one would turn each of these into the next microsecond.
    EXPECT_EQ(timestamp_line(ticks_at(1)), "[   0.000000]");
    EXPECT_EQ(timestamp_line(ticks_at(999)), "[   0.000000]");
    EXPECT_EQ(timestamp_line(ticks_at(1'999)), "[   0.000001]");
    EXPECT_EQ(timestamp_line(ticks_at(999'999'999)), "[   0.999999]");
    EXPECT_EQ(timestamp_line(ticks_at(k_SECOND + 999)), "[   1.000000]");
}

TEST(CoreLogFormat, TheSecondsGrowPastFourDigitsAndNothingIsCut)
{
    EXPECT_EQ(timestamp_line(ticks_at(10'000 * k_SECOND)), "[10000.000000]");
    EXPECT_EQ(timestamp_line(ticks_at((99'999 * k_SECOND) + 5'000)),
              "[99999.000005]");
    EXPECT_EQ(timestamp_line(ticks_at(400'000'000 * k_SECOND)),
              "[400000000.000000]");
}

TEST(CoreLogFormat, TheStartTickIsTheZero)
{
    // Ticks before the start are not made, and the start itself is time 0.
    EXPECT_EQ(timestamp_line(k_START), "[   0.000000]");
    EXPECT_EQ(timestamp_line(k_START + (5 * k_SECOND)), "[   5.000000]");
}

TEST(CoreLogFormat, ConvertsWithTheClockItIsGiven)
{
    // 24 ticks of Apple's clock are 1000 nanoseconds exactly, and 24 million
    // of them are a second: 125 / 3 nanoseconds a tick, not 1.
    struct Case {
        std::uint64_t ticks;
        const char*   expected;
    };
    const auto cases = std::to_array<Case>({
        {0, "[   0.000000]"},
        {23, "[   0.000000]"},  // 958 ns
        {24, "[   0.000001]"},  // 1000 ns
        {25, "[   0.000001]"},  // 1041 ns
        {24'000'000, "[   1.000000]"},
        {std::uint64_t{24'000'000} * 3'600, "[3600.000000]"},
    });
    for (const Case& c : cases) {
        EXPECT_EQ(timestamp_line(k_START + c.ticks, k_APPLE), c.expected)
            << c.ticks << " ticks";
    }
}

TEST(CoreLogFormat, TimestampAgreesWithStdFormatOnRandomTicks)
{
    Sequence random;
    for (std::int32_t i = 0; i < 20'000; ++i) {
        const std::uint64_t nanoseconds = (random.next() >> 7U) >>
                                          (random.next() % 40U);
        const std::string   expected    = std::format("[{:4}.{:06}]",
                                                      nanoseconds / k_SECOND,
                                                      nanoseconds % k_SECOND /
                                                          1'000U);
        ASSERT_EQ(timestamp_line(ticks_at(nanoseconds)), expected)
            << nanoseconds << " ns";
    }
}

// -------------------------------------------------------------- prefix -----

TEST(CoreLogFormat, PrefixIsTimestampSeverityAndThreadInTheirColumns)
{
    EXPECT_EQ(prefix_line(ticks_at(0), Severity::e_INFO, "render"),
              "[   0.000000] INFO  render  ");
    EXPECT_EQ(prefix_line(ticks_at(12'351'000'000),
                          Severity::e_ERROR,
                          "stream"),
              "[  12.351000] ERROR stream  ");
}

TEST(CoreLogFormat, EverySeverityHasItsColumn)
{
    for (std::uint32_t i = 0; i < k_NAMES.size(); ++i) {
        const std::string prefix = prefix_line(ticks_at(0),
                                               static_cast<Severity>(i),
                                               "render");
        EXPECT_EQ(prefix, prefix_oracle(0, i, "render")) << "severity " << i;
        EXPECT_EQ(prefix.substr(14, 5), std::format("{:<5}", k_NAMES[i]))
            << "severity " << i;
    }
}

TEST(CoreLogFormat, TheMessageStartsInTheSameColumnWhateverIsBefore)
{
    // Seconds below 10000, so the field has not grown: the prefix is 28
    // characters whatever the severity or the thread. The space that follows
    // belongs to the record, so a message starts in column 29 on every line.
    for (std::uint32_t severity = 0; severity <= 7; ++severity) {
        for (const std::string_view thread : k_THREADS) {
            EXPECT_EQ(prefix_line(ticks_at(1'500'000'000),
                                  static_cast<Severity>(severity),
                                  thread)
                          .size(),
                      28U)
                << "severity " << severity << ", thread '" << thread << "'";
        }
    }

    for (const std::string_view thread : k_THREADS) {
        const std::string line = record_line(message_record(Severity::e_INFO,
                                                            k_SECOND,
                                                            core_message()),
                                             thread);
        ASSERT_GT(line.size(), 29U);
        EXPECT_EQ(line[28], ' ') << "thread '" << thread << "'";
        EXPECT_EQ(line.substr(29), "core | unknown\n")
            << "thread '" << thread << "'";
    }
}

TEST(CoreLogFormat, ALongThreadNameIsCutToItsColumn)
{
    EXPECT_EQ(prefix_line(ticks_at(0), Severity::e_INFO, "abcdefghij"),
              "[   0.000000] INFO  abcdefgh");
    EXPECT_EQ(prefix_line(ticks_at(0), Severity::e_INFO, "streaming-worker"),
              "[   0.000000] INFO  streamin");
}

TEST(CoreLogFormat, AThreadNameOfTheColumnsWidthIsNeitherCutNorPadded)
{
    EXPECT_EQ(prefix_line(ticks_at(0), Severity::e_INFO, "12345678"),
              "[   0.000000] INFO  12345678");
}

TEST(CoreLogFormat, AShortThreadNameIsPadded)
{
    EXPECT_EQ(prefix_line(ticks_at(0), Severity::e_INFO, ""),
              "[   0.000000] INFO          ");
    EXPECT_EQ(prefix_line(ticks_at(0), Severity::e_INFO, "a"),
              "[   0.000000] INFO  a       ");
}

TEST(CoreLogFormat, CuttingAThreadNameIsNotTruncation)
{
    // The writer says it was cut only when it ran out of room, never because
    // a name was longer than its column.
    const Written written = written_with(k_ROOMY, [](LineWriter& writer) {
        LogFormat::append_prefix(writer,
                                 ticks_at(0),
                                 Severity::e_INFO,
                                 "streaming-worker",
                                 k_NANOSECONDS);
    });
    EXPECT_FALSE(written.truncated);
}

TEST(CoreLogFormat, ASeverityNoEnumeratorNamesIsAQuestionMarkInItsColumn)
{
    EXPECT_EQ(prefix_line(ticks_at(0), static_cast<Severity>(7), "render"),
              "[   0.000000] ?     render  ");
}

TEST(CoreLogFormat, PrefixAgreesWithStdFormatOnRandomInput)
{
    Sequence random;
    for (std::int32_t i = 0; i < 20'000; ++i) {
        const std::uint32_t severity = static_cast<std::uint32_t>(
            random.next() % 6U);
        const std::uint64_t    nanoseconds = (random.next() >> 7U) >>
                                             (random.next() % 40U);
        const std::string_view thread =
            k_THREADS[random.next() % k_THREADS.size()];
        ASSERT_EQ(prefix_line(ticks_at(nanoseconds),
                              static_cast<Severity>(severity),
                              thread),
                  prefix_oracle(nanoseconds, severity, thread))
            << nanoseconds << " ns, severity " << severity << ", thread '"
            << thread << "'";
    }
}

// -------------------------------------------------------- error record -----

TEST(CoreLogFormat, AnErrorRecordIsALineAsItAppearsInALog)
{
    EXPECT_EQ(record_line(error_record(Severity::e_WARN, 0, core_error(0xCD)),
                          "render"),
              "[   0.000000] WARN  render   "
              "core | unknown | unknown | ctx=0x000000cd\n");
    EXPECT_EQ(record_line(error_record(Severity::e_FATAL,
                                       (2 * k_SECOND) + 5'000,
                                       vulkan_error(0xDEADBEEF)),
                          "stream"),
              "[   2.000005] FATAL stream   "
              "vulkan | unknown | unknown | ctx=0xdeadbeef\n");
}

TEST(CoreLogFormat, AnErrorRecordIsItsPrefixItsErrorAndANewline)
{
    Sequence random;
    for (std::int32_t i = 0; i < 2'000; ++i) {
        const std::uint32_t severity = static_cast<std::uint32_t>(
            random.next() % 6U);
        const std::uint64_t nanoseconds = (random.next() >> 7U) >>
                                          (random.next() % 40U);
        const Error         error       = core_error(
            static_cast<std::uint32_t>(random.next()));
        const std::string_view thread =
            k_THREADS[random.next() % k_THREADS.size()];

        EXPECT_EQ(record_line(error_record(static_cast<Severity>(severity),
                                           nanoseconds,
                                           error),
                              thread),
                  prefix_oracle(nanoseconds, severity, thread) + " " +
                      error_text(error) + "\n");
    }
}

TEST(CoreLogFormat, AnErrorRecordDoesNotShowTheRawValue)
{
    const std::string line = record_line(
        error_record(Severity::e_INFO, 0, core_error(1)), "render");
    EXPECT_EQ(line.find("raw="), std::string::npos) << line;
}

TEST(CoreLogFormat, AnErrorRecordIsOneLine)
{
    const std::string line = record_line(
        error_record(Severity::e_INFO, 5, vulkan_error(0xFFFFFFFF)),
        "streaming-worker");
    ASSERT_FALSE(line.empty());
    EXPECT_EQ(line.back(), '\n');
    EXPECT_EQ(std::count(line.begin(), line.end(), '\n'), 1);
    for (const char c : line.substr(0, line.size() - 1)) {
        EXPECT_GE(static_cast<std::uint8_t>(c), 0x20U);
        EXPECT_NE(static_cast<std::uint8_t>(c), 0x7FU);
    }
}

// ----------------------------------------------------- message record -----

TEST(CoreLogFormat, AMessageRecordIsALineAsItAppearsInALog)
{
    EXPECT_EQ(record_line(message_record(Severity::e_INFO, 0, core_message()),
                          "render"),
              "[   0.000000] INFO  render   core | unknown\n");
    EXPECT_EQ(record_line(message_record(Severity::e_DEBUG,
                                         k_SECOND + 42'000,
                                         vulkan_message()),
                          "stream"),
              "[   1.000042] DEBUG stream   vulkan | unknown\n");
}

TEST(CoreLogFormat, AMessageRecordIsItsPrefixItsMessageAndANewline)
{
    Sequence random;
    for (std::int32_t i = 0; i < 2'000; ++i) {
        const std::uint32_t severity = static_cast<std::uint32_t>(
            random.next() % 6U);
        const std::uint64_t    nanoseconds = (random.next() >> 7U) >>
                                             (random.next() % 40U);
        const LogMessage       message     = (random.next() % 2U) == 0U
                                                 ? core_message()
                                                 : vulkan_message();
        const std::string_view thread =
            k_THREADS[random.next() % k_THREADS.size()];

        EXPECT_EQ(record_line(message_record(static_cast<Severity>(severity),
                                             nanoseconds,
                                             message),
                              thread),
                  prefix_oracle(nanoseconds, severity, thread) + " " +
                      message_text(message) + "\n");
    }
}

TEST(CoreLogFormat, AMessageRecordDoesNotShowTheRawValue)
{
    const std::string line = record_line(
        message_record(Severity::e_INFO, 0, core_message()), "render");
    EXPECT_EQ(line.find("raw="), std::string::npos) << line;
}

TEST(CoreLogFormat, AMessageRecordIsOneLine)
{
    const std::string line = record_line(message_record(Severity::e_INFO,
                                                        5,
                                                        vulkan_message()),
                                         "streaming-worker");
    ASSERT_FALSE(line.empty());
    EXPECT_EQ(line.back(), '\n');
    EXPECT_EQ(std::count(line.begin(), line.end(), '\n'), 1);
    for (const char c : line.substr(0, line.size() - 1)) {
        EXPECT_GE(static_cast<std::uint8_t>(c), 0x20U);
        EXPECT_NE(static_cast<std::uint8_t>(c), 0x7FU);
    }
}

// ------------------------------------------------------------ dispatch -----

TEST(CoreLogFormat, ARecordOfATypeNoFactoryMakesIsAQuestionMarkOnALineOfItsOwn)
{
    // The enumeration's sentinel, and the one value of the 2-bit field that
    // is not even that. Neither reads the payload as an error or a message.
    for (const std::uint32_t type : {2U, 3U}) {
        const LogRecord record = record_of_type_bits(type,
                                                     3,
                                                     ticks_at(2 * k_SECOND),
                                                     0xFFFF'FFFF'FFFF'FFFFULL);
        EXPECT_EQ(record_line(record, "render"),
                  "[   2.000000] WARN  render   ?\n")
            << "type " << type;
    }
}

TEST(CoreLogFormat, RecordsAppendedToOneWriterComeOutInOrder)
{
    // The writer spans a whole batch, so a record is appended after others:
    // the line must not depend on where it starts.
    Sequence            random;
    std::vector<Sample> samples;
    std::string         expected;
    for (std::int32_t i = 0; i < 200; ++i) {
        samples.push_back(random_sample(random));
        expected += samples.back().expected;
    }

    std::vector<char> buffer(expected.size() + 64);
    LineWriter        writer(buffer);
    for (const Sample& sample : samples) {
        LogFormat::append_record(writer,
                                 sample.record,
                                 sample.thread,
                                 k_NANOSECONDS);
    }

    EXPECT_FALSE(writer.is_truncated());
    EXPECT_EQ(std::string(writer.view()), expected);
}

TEST(CoreLogFormat, ARecordOfAnyTypeIsCutAtAnyCapacityAndNeverOverrun)
{
    const LogRecord records[] = {
        error_record(Severity::e_ERROR, 4 * k_SECOND, core_error(0xBEEF)),
        message_record(Severity::e_WARN, 5 * k_SECOND, vulkan_message()),
        record_of_type_bits(3, 1, ticks_at(6 * k_SECOND), 0),
    };
    for (const LogRecord& record : records) {
        const std::string full = record_line(record, "render");
        for (std::size_t capacity = 0; capacity <= full.size() + 2;
             ++capacity) {
            const Written written = written_with(
                capacity, [&](LineWriter& writer) {
                    LogFormat::append_record(writer,
                                             record,
                                             "render",
                                             k_NANOSECONDS);
                });
            EXPECT_EQ(written.text, full.substr(0, capacity))
                << "type " << static_cast<std::uint32_t>(record.type()) << ", "
                << capacity;
            EXPECT_EQ(written.truncated, capacity < full.size())
                << "type " << static_cast<std::uint32_t>(record.type()) << ", "
                << capacity;
            EXPECT_TRUE(written.guard_intact) << capacity;
        }
    }
}

TEST(CoreLogFormat, RecordAgreesWithStdFormatOnRandomRecords)
{
    Sequence random;
    for (std::int32_t i = 0; i < 20'000; ++i) {
        const Sample sample = random_sample(random);
        ASSERT_EQ(record_line(sample.record, sample.thread), sample.expected)
            << "record " << i
            << (sample.is_error ? " (an error)" : " (a message)");
    }
}

}  // close unnamed namespace
