// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_message_catalog.t.cpp                                     -*-C++-*-

//@PURPOSE: Verify that every log message has a text, that a text splits
// exactly at its argument, and that a malformed message still reads.
//
//@DESCRIPTION: The catalog is what turns a 'LogMessage' into a sentence in a
// log. Five properties have to hold:
//
//: o 'LogMessageText' splits at its one '{}' exactly: its prefix, '{}' and
//:   its suffix put back together give the text back. 'table' gives each
//:   text the argument its message declares, and the default text reads '?'.
//:
//: o 'is_valid', which decides what compiles, accepts what it documents and
//:   rejects the rest: a control character, a stray brace, a '{}' with no
//:   argument to put in it, or an argument with no '{}'. The build failure
//:   itself cannot be a unit test; it is 'is_valid' and one 'if'.
//:
//: o Every enumerator has its own text. The 'static_assert's in the catalog
//:   catch a table of the wrong LENGTH; these tests catch one holding the '?'
//:   of an unnamed message, or two messages that read the same.
//:
//: o Domains are named as errors name them: both catalogs read
//:   'k_DOMAIN_NAMES', and a message line must not disagree with an error
//:   line about which subsystem it is.
//:
//: o Anything that is not a real enumerator -- an 'e_COUNT' sentinel, or a
//:   value 'from_raw' produced -- reads '?', never a real text and never by
//:   reading past a table. The type-erased lookup, which the logger uses,
//:   agrees with the typed one.
//
// The tests iterate from 0 to each enum's 'e_COUNT', so messages added later
// are covered without editing this file.

#include <engine/core/core_log_message_catalog.h>

#include <engine/core/core_error_catalog.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using engine::core::Domain;
using engine::core::error::ErrorCatalog;
using engine::core::log::LogMessage;
using engine::core::log::LogMessageCatalog;
using engine::core::log::LogMessageText;
using Argument = LogMessage::Argument;

/// One message declaring each argument there is, in the [core] domain, for
/// the texts 'table' makes.
enum class Probe : std::uint16_t {
    e_NONE,
    e_UNSIGNED,
    e_SIGNED,
    e_HEX,
    e_FLOAT,

    e_COUNT,
};

}  // close unnamed namespace

/// Declare what the argument of each [Probe] message holds.
template <> struct engine::core::log::LogMessageIdTraits<Probe> {
    static constexpr Domain k_DOMAIN = Domain::e_CORE;

    static constexpr auto k_ARGUMENTS = std::to_array<Argument>({
        Argument::e_NONE,
        Argument::e_UNSIGNED,
        Argument::e_SIGNED,
        Argument::e_HEX,
        Argument::e_FLOAT,
    });
};

namespace {

/// What the catalog returns for a message that has no text.
constexpr std::string_view k_unnamed = "?";

/// Return every real enumerator of 't_ENUM', in order: 0 up to, but not
/// including, its 'e_COUNT' sentinel.
template <class t_ENUM> std::vector<t_ENUM> every_value_of()
{
    using Underlying = std::underlying_type_t<t_ENUM>;

    const auto count = static_cast<std::size_t>(
        std::to_underlying(t_ENUM::e_COUNT));

    std::vector<t_ENUM> values;
    values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        values.push_back(static_cast<t_ENUM>(static_cast<Underlying>(i)));
    }
    return values;
}

/// Return the text 'text' was made from: its prefix, then '{}' and its
/// suffix if it has an argument.
std::string whole(const LogMessageText& text)
{
    std::string result(text.prefix());
    if (text.argument() != Argument::e_NONE) {
        result += LogMessageText::k_PLACEHOLDER;
        result += text.suffix();
    }
    return result;
}

/// Return true if no two entries of 'texts' are equal.
bool all_distinct(std::vector<std::string> texts)
{
    std::ranges::sort(texts);
    return std::ranges::adjacent_find(texts) == texts.end();
}

// ------------------------------------------------------------------ text ---

TEST(CoreLogMessageText, SplitsAtThePlaceholder)
{
    constexpr LogMessageText text("streamed {} pages", Argument::e_UNSIGNED);

    EXPECT_EQ(text.prefix(), "streamed ");
    EXPECT_EQ(text.suffix(), " pages");
    EXPECT_EQ(text.argument(), Argument::e_UNSIGNED);
    EXPECT_EQ(whole(text), "streamed {} pages");
}

TEST(CoreLogMessageText, PlaceholderMayOpenOrCloseTheText)
{
    constexpr LogMessageText first("{} pages streamed", Argument::e_UNSIGNED);
    constexpr LogMessageText last("frame took {}", Argument::e_FLOAT);

    EXPECT_EQ(first.prefix(), "");
    EXPECT_EQ(first.suffix(), " pages streamed");
    EXPECT_EQ(last.prefix(), "frame took ");
    EXPECT_EQ(last.suffix(), "");
}

TEST(CoreLogMessageText, TextWithoutArgumentIsAllPrefix)
{
    constexpr LogMessageText text("device lost", Argument::e_NONE);

    EXPECT_EQ(text.prefix(), "device lost");
    EXPECT_EQ(text.suffix(), "");
}

TEST(CoreLogMessageText, DefaultTextReadsAsUnnamed)
{
    // The text of a message no enumerator has, and of nothing else.
    constexpr LogMessageText text;

    EXPECT_EQ(whole(text), k_unnamed);
    EXPECT_EQ(text.argument(), Argument::e_NONE);
}

TEST(CoreLogMessageText, TableTakesEachArgumentFromItsMessage)
{
    // A table lists texts only; the argument of each comes from the message
    // at the same position, and the text is split for it.
    constexpr auto texts = LogMessageText::table<Probe>({
        "idle",
        "streamed {} pages",
        "drift {} us",
        "flags {}",
        "frame took {} ms",
    });

    EXPECT_EQ(texts[0], LogMessageText("idle", Argument::e_NONE));
    EXPECT_EQ(texts[1],
              LogMessageText("streamed {} pages", Argument::e_UNSIGNED));
    EXPECT_EQ(texts[2], LogMessageText("drift {} us", Argument::e_SIGNED));
    EXPECT_EQ(texts[3], LogMessageText("flags {}", Argument::e_HEX));
    EXPECT_EQ(texts[4], LogMessageText("frame took {} ms", Argument::e_FLOAT));
}

TEST(CoreLogMessageText, IsValidAcceptsWhatItDocuments)
{
    static_assert(LogMessageText::is_valid("device lost", Argument::e_NONE));
    static_assert(LogMessageText::is_valid("{}", Argument::e_FLOAT));
    static_assert(LogMessageText::is_valid("sizes: (a) [b] 1/2 ~3 #4",
                                           Argument::e_NONE));

    for (const Argument argument : {
             Argument::e_UNSIGNED,
             Argument::e_SIGNED,
             Argument::e_HEX,
             Argument::e_FLOAT,
         }) {
        EXPECT_TRUE(LogMessageText::is_valid("streamed {} pages", argument))
            << "argument "
            << static_cast<unsigned>(std::to_underlying(argument));
    }
}

TEST(CoreLogMessageText, IsValidRejectsAnythingButOnePrintableLine)
{
    // Each would break a log line in two, or reach whoever 'cat's the log in
    // a terminal.
    for (const std::string_view text : {
             "",
             " leading",
             "trailing ",
             "two\nlines",
             "tab\there",
             "return\r",
             "bell\a",
             "delete\x7f",
             "escape\x1b[31m",
             "caf\xc3\xa9",
         }) {
        EXPECT_FALSE(LogMessageText::is_valid(text, Argument::e_NONE))
            << "'" << text << "'";
    }
}

TEST(CoreLogMessageText, IsValidRejectsBracesButOnePlaceholder)
{
    for (const std::string_view text :
         {"{", "}", "a {", "}{", "{x}", "{}}", "{{}}", "{} and {}"}) {
        EXPECT_FALSE(LogMessageText::is_valid(text, Argument::e_UNSIGNED))
            << "'" << text << "'";
        EXPECT_FALSE(LogMessageText::is_valid(text, Argument::e_NONE))
            << "'" << text << "'";
    }
}

TEST(CoreLogMessageText, IsValidMatchesThePlaceholderToTheArgument)
{
    // A '{}' with nothing to put in it, an argument with nowhere to go, and
    // the sentinel, which is no way of writing an argument at all.
    EXPECT_FALSE(LogMessageText::is_valid("streamed {} pages",
                                          Argument::e_NONE));
    EXPECT_FALSE(LogMessageText::is_valid("streamed pages",
                                          Argument::e_UNSIGNED));
    EXPECT_FALSE(LogMessageText::is_valid("streamed {} pages",
                                          Argument::e_COUNT));
}

TEST(CoreLogMessageText, IsCheapToCopyAndOwnsNothing)
{
    // The consumer looks one up per record. A member that allocated or
    // needed a destructor -- a std::string, say -- would fail here.
    EXPECT_TRUE(std::is_trivially_copyable_v<LogMessageText>);
    EXPECT_TRUE(std::is_trivially_destructible_v<LogMessageText>);
}

// ---------------------------------------------------------------- domain ---

TEST(CoreLogMessageCatalog, NamesDomainsAsErrorsDo)
{
    // Every byte a domain can hold, named or not: a message line and an error
    // line must never disagree about which subsystem they come from.
    for (unsigned value = 0; value <= 0xFFU; ++value) {
        const auto domain = static_cast<Domain>(value);
        EXPECT_EQ(LogMessageCatalog::to_string_domain(domain),
                  ErrorCatalog::to_string_domain(domain))
            << "domain " << value;
    }
}

TEST(CoreLogMessageCatalog, SentinelAndOutOfRangeDomainsAreUnnamed)
{
    EXPECT_EQ(LogMessageCatalog::to_string_domain(Domain::e_COUNT), k_unnamed);
    EXPECT_EQ(LogMessageCatalog::to_string_domain(static_cast<Domain>(0x42)),
              k_unnamed);
}

// ----------------------------------------------------------------- texts ---

/// The same properties hold for every domain's text table, so they are
/// written once and run for each.
template <class t_MESSAGE>
class CoreLogMessageCatalogTexts : public testing::Test {};

using MessageTypes = testing::Types<LogMessage::CoreMessage,
                                    LogMessage::VulkanMessage,
                                    LogMessage::MetalMessage>;
TYPED_TEST_SUITE(CoreLogMessageCatalogTexts, MessageTypes);

TYPED_TEST(CoreLogMessageCatalogTexts, EveryMessageHasItsOwnText)
{
    std::vector<std::string> texts;
    for (const auto id : every_value_of<TypeParam>()) {
        const std::string text = whole(LogMessageCatalog::text(id));
        EXPECT_NE(text, k_unnamed)
            << "message " << std::to_underlying(id) << " has no text";
        texts.push_back(text);
    }
    EXPECT_TRUE(all_distinct(texts));
}

TYPED_TEST(CoreLogMessageCatalogTexts, SentinelAndOutOfRangeIdsAreUnnamed)
{
    EXPECT_EQ(whole(LogMessageCatalog::text(TypeParam::e_COUNT)), k_unnamed);

    // The largest 16-bit value: as far past the table as an id can get.
    EXPECT_EQ(whole(LogMessageCatalog::text(static_cast<TypeParam>(0xFFFF))),
              k_unnamed);
}

TYPED_TEST(CoreLogMessageCatalogTexts, ErasedLookupAgreesWithTypedLookup)
{
    // 'text_of' has to choose the table from the domain at run time. It must
    // land on the same text the typed lookup finds. Each message is read back
    // from its bits, as the logger thread reads it: 'make' takes only an id
    // known when the call is compiled, not one from a loop.
    //
    // Honest limit: while every table holds the same single text, a lookup in
    // the WRONG table would also agree. This becomes a real check of the
    // dispatch the moment two tables differ.
    using Traits = engine::core::log::LogMessageIdTraits<TypeParam>;
    for (const auto id : every_value_of<TypeParam>()) {
        const LogMessage message = LogMessage::from_raw(
            static_cast<std::uint64_t>(Traits::k_DOMAIN) |
            (static_cast<std::uint64_t>(std::to_underlying(id)) << 16U));
        EXPECT_EQ(LogMessageCatalog::text_of(message),
                  LogMessageCatalog::text(id));
    }
}

// -------------------------------------------------------------- dispatch ---

TEST(CoreLogMessageCatalog, UnknownDomainHasNoTexts)
{
    // Domain 0, id 0. If the unknown domain were mistakenly routed to a real
    // table this would read "unknown" instead of "?".
    EXPECT_EQ(whole(LogMessageCatalog::text_of(LogMessage::from_raw(0))),
              k_unnamed);
}

TEST(CoreLogMessageCatalog, SentinelDomainHasNoTexts)
{
    const auto sentinel = std::to_underlying(Domain::e_COUNT);
    EXPECT_EQ(
        whole(LogMessageCatalog::text_of(LogMessage::from_raw(sentinel))),
        k_unnamed);
}

TEST(CoreLogMessageCatalog, OutOfRangeDomainHasNoTexts)
{
    // A domain byte no enumerator names falls through the exhaustive switch
    // in 'text_of'; without the return after it, this is undefined behavior.
    // The failure is certain under GCC -Werror=return-type or
    // -fsanitize=undefined, not in a release build.
    EXPECT_EQ(whole(LogMessageCatalog::text_of(LogMessage::from_raw(0x42))),
              k_unnamed);
}

TEST(CoreLogMessageCatalog, KnownDomainWithAnUnknownIdIsUnnamed)
{
    // Domain vulkan, id 0xFFFF: the right table, an index past its end.
    EXPECT_EQ(whole(LogMessageCatalog::text_of(
                  LogMessage::from_raw(0x00000000FFFF0002ULL))),
              k_unnamed);
}

TEST(CoreLogMessageCatalog, IsUsableInAConstantExpression)
{
    // Evaluated at compile time: if any part stopped being constexpr, this
    // file would fail to build.
    static_assert(LogMessageCatalog::to_string_domain(Domain::e_METAL) ==
                  "metal");
    static_assert(LogMessageCatalog::text(LogMessage::CoreMessage::e_UNKNOWN)
                      .prefix() == "unknown");
    static_assert(LogMessageCatalog::text_of(
                      LogMessage::make(LogMessage::VulkanMessage::e_UNKNOWN))
                      .prefix() == "unknown");

    SUCCEED();
}

}  // close unnamed namespace
