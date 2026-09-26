// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_error_format.t.cpp                                            -*-C++-*-

//@PURPOSE: Verify the log line an 'Error' becomes, and that producing it into
// a fixed buffer can neither overflow nor allocate.
//
//@DESCRIPTION: The logger's consumer turns each 'Error' it pops into one line
// of text. The contract under test is:
//
//: o 'std::format("{}", error)' gives
//:   'domain | kind | reason | ctx=0xXXXXXXXX'. The context is always eight
//:   lower-case hex digits: its meaning depends on the reason (a count, a
//:   handle, a negative VkResult), and hex is the one spelling that is
//:   unambiguous for all of them.
//:
//: o 'std::format("{:#}", error)' appends ' <=> raw=0x' and the 16-digit
//:   packed value, so a corrupt error can still be decoded by hand.
//:
//: o 'std::format("{}", descriptor)' gives the 'domain | kind | reason' part
//:   alone.
//:
//: o 'ErrorFormat::format_line(buffer, error, raw)' writes the same text,
//:   caller-owned 'std::span<char>' and returns the number of characters
//:   written. It never writes past the span, never NUL-terminates, never
//:   allocates, and truncates rather than fails when the span is short.
//
// Expected strings are spelled out literally rather than rebuilt from the
// catalog, so a change to the layout shows up here as a deliberate edit.

#include <engine/core/core_error_format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

using engine::core::error::Error;
using engine::core::error::ErrorDescriptor;
using engine::core::error::ErrorFormat;
using RawValue = engine::core::error::ErrorFormat::RawValue;

/// Filler that 'format_line' never writes, so an untouched byte is visible.
constexpr char k_poison = '#';

constexpr Error k_vulkan = Error::make(Error::Kind::e_UNKNOWN,
                                       Error::VulkanReason::e_UNKNOWN,
                                       0xCDu);

/// Domain 0x42 and reason 0xFFFF name nothing; kind 0 does.
constexpr Error k_corrupt = Error::from_raw(0x00000000FFFF0042ull);

/// Return the first 'count' characters of 'buffer' as a view.
template <std::size_t t_SIZE>
std::string_view written(const std::array<char, t_SIZE>& buffer,
                         const std::size_t               count)
{
    return {buffer.data(), count};
}

// -------------------------------------------------------------- layout -----

TEST(CoreErrorFormat, DefaultFormIsDomainKindReasonAndContext)
{
    EXPECT_EQ(std::format("{}", k_vulkan),
              "vulkan | unknown | unknown | ctx=0x000000cd");
}

TEST(CoreErrorFormat, AlternateFormAppendsTheRawValue)
{
    EXPECT_EQ(std::format("{:#}", k_vulkan),
              "vulkan | unknown | unknown | ctx=0x000000cd "
              "<=> raw=0x000000cd00000002");
}

TEST(CoreErrorFormat, ContextIsAlwaysEightHexDigits)
{
    EXPECT_EQ(std::format("{}",
                          Error::make(Error::Kind::e_UNKNOWN,
                                      Error::CoreReason::e_UNKNOWN)),
              "core | unknown | unknown | ctx=0x00000000");
    EXPECT_EQ(std::format("{}",
                          Error::make(Error::Kind::e_UNKNOWN,
                                      Error::CoreReason::e_UNKNOWN,
                                      0xFFFFFFFFu)),
              "core | unknown | unknown | ctx=0xffffffff");
}

TEST(CoreErrorFormat, NegativeContextPrintsItsBitsNotASign)
{
    // VkResult error codes are negative. They are stored bit for bit, and
    // printed the same way, so -60 reads as its two's complement.
    const Error error = Error::make_with_signed_context(
        Error::Kind::e_UNKNOWN, Error::VulkanReason::e_UNKNOWN, -60);

    EXPECT_EQ(std::format("{}", error),
              "vulkan | unknown | unknown | ctx=0xffffffc4");
}

TEST(CoreErrorFormat, CorruptErrorMarksWhatItCannotName)
{
    EXPECT_EQ(std::format("{}", k_corrupt),
              "? | unknown | ? | ctx=0x00000000");

    // The raw value is what lets someone reading the log recover the bits
    // the names could not describe.
    EXPECT_EQ(std::format("{:#}", k_corrupt),
              "? | unknown | ? | ctx=0x00000000 <=> raw=0x00000000ffff0042");
}

TEST(CoreErrorFormat, ComposesInsideALargerFormatString)
{
    EXPECT_EQ(std::format("create failed: {} (attempt {})", k_vulkan, 3),
              "create failed: vulkan | unknown | unknown | ctx=0x000000cd "
              "(attempt 3)");
}

// ---------------------------------------------------------- descriptor -----

TEST(CoreErrorFormat, DescriptorFormatsAsItsThreeNames)
{
    EXPECT_EQ(std::format("{}", ErrorDescriptor::make(k_vulkan)),
              "vulkan | unknown | unknown");
    EXPECT_EQ(std::format("{}", ErrorDescriptor::make(k_corrupt)),
              "? | unknown | ?");
}

TEST(CoreErrorFormat, ErrorLineStartsWithItsDescriptor)
{
    // One layout, not two that happen to agree today.
    const std::string line  = std::format("{}", k_vulkan);
    const std::string names = std::format("{}",
                                          ErrorDescriptor::make(k_vulkan));

    EXPECT_TRUE(line.starts_with(names + " | ")) << line;
}

// --------------------------------------------------------- format_line -----

TEST(CoreErrorFormatLine, MatchesStdFormatInBothForms)
{
    for (const RawValue raw : {RawValue::e_OMIT, RawValue::e_APPEND}) {
        std::array<char, 128> buffer{};
        const std::size_t     count = ErrorFormat::format_line(buffer,
                                                               k_vulkan,
                                                               raw);

        EXPECT_EQ(written(buffer, count),
                  raw == RawValue::e_APPEND ? std::format("{:#}", k_vulkan)
                                            : std::format("{}", k_vulkan));
    }
}

TEST(CoreErrorFormatLine, WritesNothingPastTheLineAndNoTerminator)
{
    std::array<char, 128> buffer{};
    buffer.fill(k_poison);

    const std::size_t count = ErrorFormat::format_line(buffer,
                                                       k_vulkan,
                                                       RawValue::e_OMIT);

    ASSERT_LT(count, buffer.size());
    EXPECT_TRUE(std::ranges::all_of(std::span(buffer).subspan(count),
                                    [](const char c) {
                                        return c == k_poison;
                                    }));
}

TEST(CoreErrorFormatLine, TruncatesToAShortBuffer)
{
    // One byte of guard on each side of the eight the call is given: an
    // off-by-one in either direction lands on a guard.
    std::array<char, 10> storage{};
    storage.fill(k_poison);
    const std::span<char> window = std::span(storage).subspan(1, 8);

    const std::size_t count = ErrorFormat::format_line(window,
                                                       k_vulkan,
                                                       RawValue::e_OMIT);

    EXPECT_EQ(count, window.size());
    EXPECT_EQ(std::string_view(window.data(), count), "vulkan |");
    EXPECT_EQ(storage.front(), k_poison);
    EXPECT_EQ(storage.back(), k_poison);
}

TEST(CoreErrorFormatLine, FitsExactlyWhenTheBufferIsTheLineLength)
{
    const std::string expected = std::format("{:#}", k_vulkan);

    std::array<char, 128> storage{};
    const std::span<char> exact = std::span(storage).first(expected.size());

    const std::size_t count = ErrorFormat::format_line(exact,
                                                       k_vulkan,
                                                       RawValue::e_APPEND);

    EXPECT_EQ(count, expected.size());
    EXPECT_EQ(std::string_view(exact.data(), count), expected);
}

TEST(CoreErrorFormatLine, EmptyBufferWritesNothing)
{
    EXPECT_EQ(ErrorFormat::format_line(std::span<char>{},
                                       k_vulkan,
                                       RawValue::e_APPEND),
              0u);
}

TEST(CoreErrorFormatLine, CannotFail)
{
    // The consumer calls this for every record it pops. A version that could
    // throw -- or, under -fno-exceptions, abort -- would take the logger down
    // with it.
    static_assert(noexcept(ErrorFormat::format_line(
        std::declval<std::span<char>>(), k_vulkan, RawValue::e_OMIT)));
    SUCCEED();
}

// -------------------------------------------------------------- limits -----

TEST(CoreErrorFormatLine, EveryCatalogLineFitsTheMaximum)
{
    // Every domain, kind and reason value the catalog names, plus one past
    // the end of each table (printed as '?'), with every context bit set and
    // the raw value appended: the longest line the catalog can produce must
    // fit in 'k_MAX_LINE_SIZE'. A name that ever outgrows it fails here,
    // instead of truncating a log line in the field.
    const std::uint64_t domains = std::to_underlying(Error::Domain::e_COUNT);
    const std::uint64_t kinds   = std::to_underlying(Error::Kind::e_COUNT);
    const std::uint64_t reasons = std::max({
        std::to_underlying(Error::CoreReason::e_COUNT),
        std::to_underlying(Error::VulkanReason::e_COUNT),
        std::to_underlying(Error::MetalReason::e_COUNT),
    });

    for (std::uint64_t domain = 0; domain <= domains; ++domain) {
        for (std::uint64_t kind = 0; kind <= kinds; ++kind) {
            for (std::uint64_t reason = 0; reason <= reasons; ++reason) {
                const Error error = Error::from_raw(domain | (kind << 8U) |
                                                    (reason << 16U) |
                                                    (0xFFFFFFFFULL << 32U));

                // One byte more than the maximum: a line that does not fit
                // shows up as a count above it, not as a silent cut.
                std::array<char, ErrorFormat::k_MAX_LINE_SIZE + 1> buffer{};
                const std::size_t count = ErrorFormat::format_line(
                    buffer, error, RawValue::e_APPEND);
                EXPECT_LE(count, ErrorFormat::k_MAX_LINE_SIZE)
                    << written(buffer, count);
            }
        }
    }
}

// ------------------------------------------------------------ contract -----

TEST(CoreErrorFormat, BothTypesMeetTheFormatterRequirements)
{
    // What 'std::format' requires of a formatter -- default-constructible,
    // copyable, a 'parse', a const 'format' -- checked in one line each.
    static_assert(std::formattable<Error, char>);
    static_assert(std::formattable<ErrorDescriptor, char>);
    SUCCEED();
}

// GoogleTest runs suites named '*DeathTest' first, before any threads exist.
TEST(CoreErrorFormatDeathTest, SpecificationBuiltAtRunTimeIsRejected)
{
    // A literal '{:x}' does not compile: 'parse' reaches a non-constexpr
    // function during the compile-time check. A format string built at run
    // time reaches it while running, and with exceptions off it must abort
    // rather than print something wrong. Built by concatenation so the
    // compiler cannot see the string.
    const std::string hex = std::string("{:") + "x}";
    EXPECT_DEATH((void)std::vformat(hex, std::make_format_args(k_vulkan)), "");

    // A descriptor has no raw form, so '#' is rejected for it too.
    const ErrorDescriptor descriptor = ErrorDescriptor::make(k_vulkan);
    const std::string     hash       = std::string("{:") + "#}";
    EXPECT_DEATH((void)std::vformat(hash, std::make_format_args(descriptor)),
                 "");
}

}  // close unnamed namespace
