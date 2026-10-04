// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_message_format.t.cpp                                      -*-C++-*-

//@PURPOSE: Verify the log line a 'LogMessage' becomes, and that producing it
// into a fixed buffer can neither overflow nor allocate.
//
//@DESCRIPTION: The logger's consumer turns each 'LogMessage' it pops into one
// line of text. The contract under test is:
//
//: o 'std::format("{}", message)' gives 'domain | text', with the argument
//:   where the text has '{}', written as the catalog says: decimal, signed
//:   decimal, '0x' and eight hex digits, or the shortest float that reads
//:   back exactly.
//:
//: o 'std::format("{:#}", message)' appends ' <=> raw=0x' and the 16-digit
//:   packed value, so a corrupt message can still be decoded by hand.
//:
//: o 'LogMessageFormat::format_line(buffer, message, raw)' writes the same
//:   text into a caller-owned 'std::span<char>' and returns the number of
//:   characters written. It never writes past the span, never NUL-terminates,
//:   never allocates, and truncates rather than fails when the span is short.
//:
//: o A message line reads like an error line: the same separator, the same
//:   raw value, the same domain names. The logger interleaves both.
//
// The real catalog has no message with an argument yet, so the arguments are
// checked through 'append_text' with texts made here.
//
// Expected strings are spelled out literally rather than rebuilt from the
// catalog, so a change to the layout shows up here as a deliberate edit.

#include <engine/core/core_log_message_format.h>

#include <engine/core/core_error_format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

using engine::core::Domain;
using engine::core::error::Error;
using engine::core::error::ErrorFormat;
using engine::core::format::LineWriter;
using engine::core::log::LogMessage;
using engine::core::log::LogMessageFormat;
using engine::core::log::LogMessageText;
using Argument = LogMessage::Argument;
using RawValue = LogMessageFormat::RawValue;

/// Filler that 'format_line' never writes, so an untouched byte is visible.
constexpr char k_poison = '#';

constexpr LogMessage k_vulkan = LogMessage::make(
    LogMessage::VulkanMessage::e_UNKNOWN);

/// Domain 0x42 and id 0xFFFF name nothing.
constexpr LogMessage k_corrupt = LogMessage::from_raw(0x00000000FFFF0042ULL);

/// Return the first 'count' characters of 'buffer' as a view.
template <std::size_t t_SIZE>
std::string_view written(const std::array<char, t_SIZE>& buffer,
                         const std::size_t               count)
{
    return {buffer.data(), count};
}

/// Return what 'append_text' writes for 'text' and the 'argument' bits.
std::string written_text(const LogMessageText& text,
                         const std::uint32_t   argument)
{
    std::array<char, 64> buffer{};
    LineWriter           writer(buffer);
    LogMessageFormat::append_text(writer, text, argument);
    return std::string(writer.view());
}

// -------------------------------------------------------------- layout -----

TEST(CoreLogMessageFormat, DefaultFormIsDomainAndText)
{
    EXPECT_EQ(std::format("{}", k_vulkan), "vulkan | unknown");
}

TEST(CoreLogMessageFormat, AlternateFormAppendsTheRawValue)
{
    EXPECT_EQ(std::format("{:#}", k_vulkan),
              "vulkan | unknown <=> raw=0x0000000000000002");
}

TEST(CoreLogMessageFormat, BitsTheTextHasNoPlaceForShowOnlyInTheRawValue)
{
    // 'make' cannot give 'e_UNKNOWN' an argument, but 'from_raw' reads any 64
    // bits: its text has no '{}', so the argument shows in the raw value only.
    const LogMessage message = LogMessage::from_raw(0x000000CD00000002ULL);

    EXPECT_EQ(std::format("{}", message), "vulkan | unknown");
    EXPECT_EQ(std::format("{:#}", message),
              "vulkan | unknown <=> raw=0x000000cd00000002");
}

TEST(CoreLogMessageFormat, CorruptMessageMarksWhatItCannotName)
{
    EXPECT_EQ(std::format("{}", k_corrupt), "? | ?");

    // A known domain keeps its name when only the id is unknown.
    EXPECT_EQ(std::format("{}", LogMessage::from_raw(0x00000000FFFF0003ULL)),
              "metal | ?");

    // The raw value is what lets someone reading the log recover the bits
    // the names could not describe.
    EXPECT_EQ(std::format("{:#}", k_corrupt),
              "? | ? <=> raw=0x00000000ffff0042");
}

TEST(CoreLogMessageFormat, ComposesInsideALargerFormatString)
{
    EXPECT_EQ(std::format("logged: {} (twice)", k_vulkan),
              "logged: vulkan | unknown (twice)");
}

// ------------------------------------------------------------ argument -----

TEST(CoreLogMessageFormat, UnsignedArgumentIsDecimal)
{
    constexpr LogMessageText text("streamed {} pages", Argument::e_UNSIGNED);

    EXPECT_EQ(written_text(text, 0), "streamed 0 pages");
    EXPECT_EQ(written_text(text, 412), "streamed 412 pages");
    EXPECT_EQ(written_text(text, 0xFFFFFFFFU), "streamed 4294967295 pages");
}

TEST(CoreLogMessageFormat, SignedArgumentKeepsItsSign)
{
    constexpr LogMessageText text("drift {} us", Argument::e_SIGNED);

    const auto bits = [](const std::int32_t value) {
        return std::bit_cast<std::uint32_t>(value);
    };

    EXPECT_EQ(written_text(text, bits(7)), "drift 7 us");
    EXPECT_EQ(written_text(text, bits(-1)), "drift -1 us");
    EXPECT_EQ(written_text(text,
                           bits(std::numeric_limits<std::int32_t>::min())),
              "drift -2147483648 us");
}

TEST(CoreLogMessageFormat, HexArgumentIsAlwaysEightDigits)
{
    constexpr LogMessageText text("flags {}", Argument::e_HEX);

    EXPECT_EQ(written_text(text, 0), "flags 0x00000000");
    EXPECT_EQ(written_text(text, 0xCDU), "flags 0x000000cd");
    EXPECT_EQ(written_text(text, 0xFFFFFFFFU), "flags 0xffffffff");
}

TEST(CoreLogMessageFormat, FloatArgumentIsTheShortestExactForm)
{
    constexpr LogMessageText text("frame took {} ms", Argument::e_FLOAT);
    using Limits = std::numeric_limits<float>;

    const auto bits = [](const float value) {
        return std::bit_cast<std::uint32_t>(value);
    };

    EXPECT_EQ(written_text(text, bits(16.5F)), "frame took 16.5 ms");
    EXPECT_EQ(written_text(text, bits(0.1F)), "frame took 0.1 ms");
    EXPECT_EQ(written_text(text, bits(-0.0F)), "frame took -0 ms");
    EXPECT_EQ(written_text(text, bits(Limits::infinity())),
              "frame took inf ms");
    EXPECT_EQ(written_text(text, bits(Limits::quiet_NaN())),
              "frame took nan ms");
}

TEST(CoreLogMessageFormat, TextWithoutArgumentIgnoresTheBits)
{
    constexpr LogMessageText text("device lost", Argument::e_NONE);

    EXPECT_EQ(written_text(text, 0xFFFFFFFFU), "device lost");
}

TEST(CoreLogMessageFormat, NoArgumentIsWiderThanItsBudget)
{
    // 'k_MAX_LINE_SIZE' budgets 15 characters for the argument: the longest
    // shortest-exact float, -1.00002075e-36. Bit patterns at the extremes of
    // every way of writing an argument must fit in it.
    constexpr std::size_t k_widest = 15;

    // Only the argument is written: each text is just '{}'.
    constexpr std::array texts = {
        LogMessageText("{}", Argument::e_UNSIGNED),
        LogMessageText("{}", Argument::e_SIGNED),
        LogMessageText("{}", Argument::e_HEX),
        LogMessageText("{}", Argument::e_FLOAT),
    };

    for (const LogMessageText& text : texts) {
        for (const std::uint32_t bits : {
                 0x00000000U,
                 0x00000001U,
                 0x00800000U,
                 0x7F7FFFFFU,
                 0x7FFFFFFFU,
                 0x80000000U,
                 0x83AA250CU,
                 0xFFFFFFFFU,
             }) {
            EXPECT_LE(written_text(text, bits).size(), k_widest)
                << "argument "
                << static_cast<unsigned>(std::to_underlying(text.argument()))
                << ", bits " << bits << ": " << written_text(text, bits);
        }
    }
}

// --------------------------------------------------------- format_line -----

TEST(CoreLogMessageFormatLine, MatchesStdFormatInBothForms)
{
    for (const RawValue raw : {RawValue::e_OMIT, RawValue::e_APPEND}) {
        std::array<char, 128> buffer{};
        const std::size_t     count = LogMessageFormat::format_line(buffer,
                                                                    k_vulkan,
                                                                    raw);

        EXPECT_EQ(written(buffer, count),
                  raw == RawValue::e_APPEND ? std::format("{:#}", k_vulkan)
                                            : std::format("{}", k_vulkan));
    }
}

TEST(CoreLogMessageFormatLine, WritesNothingPastTheLineAndNoTerminator)
{
    std::array<char, 128> buffer{};
    buffer.fill(k_poison);

    const std::size_t count = LogMessageFormat::format_line(buffer,
                                                            k_vulkan,
                                                            RawValue::e_OMIT);

    ASSERT_LT(count, buffer.size());
    EXPECT_TRUE(std::ranges::all_of(std::span(buffer).subspan(count),
                                    [](const char c) {
                                        return c == k_poison;
                                    }));
}

TEST(CoreLogMessageFormatLine, TruncatesToAShortBuffer)
{
    // One byte of guard on each side of the eight the call is given: an
    // off-by-one in either direction lands on a guard.
    std::array<char, 10> storage{};
    storage.fill(k_poison);
    const std::span<char> window = std::span(storage).subspan(1, 8);

    const std::size_t count = LogMessageFormat::format_line(window,
                                                            k_vulkan,
                                                            RawValue::e_OMIT);

    EXPECT_EQ(count, window.size());
    EXPECT_EQ(std::string_view(window.data(), count), "vulkan |");
    EXPECT_EQ(storage.front(), k_poison);
    EXPECT_EQ(storage.back(), k_poison);
}

TEST(CoreLogMessageFormatLine, FitsExactlyWhenTheBufferIsTheLineLength)
{
    const std::string expected = std::format("{:#}", k_vulkan);

    std::array<char, 128> storage{};
    const std::span<char> exact = std::span(storage).first(expected.size());

    const std::size_t count = LogMessageFormat::format_line(
        exact, k_vulkan, RawValue::e_APPEND);

    EXPECT_EQ(count, expected.size());
    EXPECT_EQ(std::string_view(exact.data(), count), expected);
}

TEST(CoreLogMessageFormatLine, EmptyBufferWritesNothing)
{
    EXPECT_EQ(LogMessageFormat::format_line(std::span<char>{},
                                            k_vulkan,
                                            RawValue::e_APPEND),
              0U);
}

TEST(CoreLogMessageFormatLine, CannotFail)
{
    // The consumer calls this for every record it pops. A version that could
    // throw -- or, under -fno-exceptions, abort -- would take the logger down
    // with it.
    static_assert(noexcept(LogMessageFormat::format_line(
        std::declval<std::span<char>>(), k_vulkan, RawValue::e_OMIT)));
    SUCCEED();
}

// -------------------------------------------------------------- limits -----

TEST(CoreLogMessageFormatLine, EveryCatalogLineFitsTheMaximum)
{
    // Every domain and id the catalog names, plus one past the end of each
    // table (read as '?'), with arguments at the widest of every way of
    // writing one and the raw value appended: the longest line the catalog
    // can produce must fit in 'k_MAX_LINE_SIZE'. A text that ever outgrows it
    // fails here, instead of truncating a log line in the field.
    const std::uint64_t domains = std::to_underlying(Domain::e_COUNT);
    const std::uint64_t ids     = std::max({
        std::to_underlying(LogMessage::CoreMessage::e_COUNT),
        std::to_underlying(LogMessage::VulkanMessage::e_COUNT),
        std::to_underlying(LogMessage::MetalMessage::e_COUNT),
    });

    for (std::uint64_t domain = 0; domain <= domains; ++domain) {
        for (std::uint64_t id = 0; id <= ids; ++id) {
            for (const std::uint64_t argument :
                 {0xFFFFFFFFULL, 0x80000000ULL, 0x83AA250CULL}) {
                const LogMessage message = LogMessage::from_raw(
                    domain | (id << 16U) | (argument << 32U));

                // One byte more than the maximum: a line that does not fit
                // shows up as a count above it, not as a silent cut.
                std::array<char, LogMessageFormat::k_MAX_LINE_SIZE + 1>
                                  buffer{};
                const std::size_t count = LogMessageFormat::format_line(
                    buffer, message, RawValue::e_APPEND);
                EXPECT_LE(count, LogMessageFormat::k_MAX_LINE_SIZE)
                    << written(buffer, count);
            }
        }
    }
}

// --------------------------------------------------------- consistency -----

TEST(CoreLogMessageFormat, ReadsLikeAnErrorLine)
{
    // The separator and the raw value are written in two components; this
    // pins them together. The same 64 bits, read as an error and as a
    // message, name the same domain and end with the same raw value.
    EXPECT_EQ(LogMessageFormat::k_SEPARATOR, ErrorFormat::k_SEPARATOR);

    const std::string message = std::format("{:#}", k_vulkan);
    const std::string error   = std::format("{:#}",
                                            Error::from_raw(k_vulkan.raw()));
    const auto        tail    = [](const std::string& line) {
        return line.substr(line.find(" <=> "));
    };
    const auto head = [](const std::string& line) {
        return line.substr(0, line.find(" | "));
    };

    EXPECT_EQ(head(message), head(error));
    EXPECT_EQ(tail(message), tail(error));
}

// ------------------------------------------------------------ contract -----

TEST(CoreLogMessageFormat, MeetsTheFormatterRequirements)
{
    // What 'std::format' requires of a formatter -- default-constructible,
    // copyable, a 'parse', a const 'format' -- checked in one line.
    static_assert(std::formattable<LogMessage, char>);
    SUCCEED();
}

// GoogleTest runs suites named '*DeathTest' first, before any threads exist.
TEST(CoreLogMessageFormatDeathTest, SpecificationBuiltAtRunTimeIsRejected)
{
    // A literal '{:x}' does not compile: 'parse' reaches a non-constexpr
    // function during the compile-time check. A format string built at run
    // time reaches it while running, and with exceptions off it must abort
    // rather than print something wrong. Built by concatenation so the
    // compiler cannot see the string.
    const std::string hex = std::string("{:") + "x}";
    EXPECT_DEATH((void)std::vformat(hex, std::make_format_args(k_vulkan)), "");
}

}  // close unnamed namespace
