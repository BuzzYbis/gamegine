// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_message.t.cpp                                             -*-C++-*-

//@PURPOSE: Verify the packing, checking and cost guarantees of 'LogMessage'.
//
//@DESCRIPTION: 'LogMessage' is what a thread pushes into the logger's ring in
// place of text, so that the text is written later, on the logger thread.
// Four properties have to hold:
//
//: o It packs and unpacks without losing a bit. A silent truncation here
//:   shows every message of one subsystem as another's, or with another
//:   number.
//:
//: o A signed or float argument comes back bit for bit, so the log shows
//:   exactly what was logged: -1 as -1, and a NaN as a NaN.
//:
//: o 'make' takes only the argument a message declares. A call that breaks
//:   the rule does not compile, which cannot be a unit test; the rule itself,
//:   'LogMessageId::accepts', is tested here for every pair.
//:
//: o It stays 8 bytes and trivially copyable, like 'Error': a push is a
//:   16-byte record copy, and a field added later would cost that silently.
//
// The real catalog has no message with an argument yet, so the arguments are
// checked through 'Probe', an enumeration made here with one message for
// each argument there is.

#include <engine/core/core_log_message.h>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace {

using engine::core::Domain;
using engine::core::log::LogMessage;
using engine::core::log::LogMessageId;
using engine::core::log::LogMessageIdType;
using Argument = LogMessage::Argument;

/// A message-sized enumeration that no domain claims.
enum class Stray : std::uint16_t {
    e_ANY,
};

/// One message declaring each argument there is, in the [core] domain.
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

// ----------------------------------------------------------------- cost -----

TEST(CoreLogMessage, IsExactlySixtyFourBits)
{
    // Pushed by value into the logger's ring on every call that logs one:
    // that stops being free the moment it grows.
    EXPECT_EQ(sizeof(LogMessage), sizeof(std::uint64_t));
    EXPECT_EQ(alignof(LogMessage), alignof(std::uint64_t));
    EXPECT_TRUE(std::is_trivially_copyable_v<LogMessage>);
    EXPECT_TRUE(std::is_trivially_destructible_v<LogMessage>);
}

// -------------------------------------------------------------- packing -----

TEST(CoreLogMessage, PacksEveryFieldWithoutCollision)
{
    const LogMessage message = LogMessage::make(Probe::e_UNSIGNED,
                                                0x0150A3B2U);

    EXPECT_EQ(message.domain(), Domain::e_CORE);
    EXPECT_EQ(message.id(), 1U);
    EXPECT_EQ(message.argument(), 0x0150A3B2U);
}

TEST(CoreLogMessage, ArgumentOccupiesTheTopThirtyTwoBitsOnly)
{
    const LogMessage message = LogMessage::make(Probe::e_HEX, 0xFFFFFFFFU);

    EXPECT_EQ(message.domain(), Domain::e_CORE);
    EXPECT_EQ(message.id(), 3U);
    EXPECT_EQ(message.argument(), 0xFFFFFFFFU);

    EXPECT_EQ(message.raw() & 0xFFFFFFFFULL, 0x00030001ULL);
}

TEST(CoreLogMessage, TheByteOfAnErrorKindStaysZero)
{
    // A message has no kind. The byte an 'Error' keeps it in stays 0, so the
    // two layouts line up in a hex dump.
    const LogMessage message = LogMessage::make(Probe::e_UNSIGNED,
                                                0xFFFFFFFFU);

    EXPECT_EQ((message.raw() >> 8U) & 0xFFU, 0U);
}

TEST(CoreLogMessage, MessageWithoutArgumentCarriesZero)
{
    const LogMessage message = LogMessage::make(
        LogMessage::MetalMessage::e_UNKNOWN);

    EXPECT_EQ(message.domain(), Domain::e_METAL);
    EXPECT_EQ(message.argument(), 0U);
}

TEST(CoreLogMessage, RawRoundTrips)
{
    const LogMessage original = LogMessage::make(Probe::e_UNSIGNED,
                                                 0x0150A3B2U);
    const LogMessage restored = LogMessage::from_raw(original.raw());

    EXPECT_EQ(restored, original);
    EXPECT_EQ(restored.id_as<Probe>(), Probe::e_UNSIGNED);
    EXPECT_EQ(restored.argument(), 0x0150A3B2U);
}

TEST(CoreLogMessage, SignedArgumentSurvivesNegativeValues)
{
    // The bits are reinterpreted, not converted, so the exact value returns.
    for (const std::int32_t value :
         {-1, -42, -2147483647 - 1, 0, 2147483647}) {
        const LogMessage message = LogMessage::make(Probe::e_SIGNED, value);
        EXPECT_EQ(message.signed_argument(), value);
    }
}

TEST(CoreLogMessage, FloatArgumentSurvivesEveryKindOfFloat)
{
    // Compared as bits: a NaN is not equal to itself and -0 equals +0, but
    // the log must show each one as it was logged.
    using Limits = std::numeric_limits<float>;
    for (const float value : {
             0.0F,
             -0.0F,
             1.5F,
             -16.666666F,
             Limits::max(),
             Limits::lowest(),
             Limits::denorm_min(),
             Limits::infinity(),
             -Limits::infinity(),
             Limits::quiet_NaN(),
             std::bit_cast<float>(0x7FC00123U),
         }) {
        const LogMessage message = LogMessage::make(Probe::e_FLOAT, value);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(message.float_argument()),
                  std::bit_cast<std::uint32_t>(value));
    }
}

TEST(CoreLogMessage, DistinctDomainsProduceDistinctMessages)
{
    const LogMessage core = LogMessage::make(
        LogMessage::CoreMessage::e_UNKNOWN);
    const LogMessage metal = LogMessage::make(
        LogMessage::MetalMessage::e_UNKNOWN);

    EXPECT_NE(core, metal);
    EXPECT_TRUE(core == core);
}

TEST(CoreLogMessage, IdAsReturnsTheTypedId)
{
    const LogMessage message = LogMessage::make(
        LogMessage::VulkanMessage::e_UNKNOWN);

    EXPECT_EQ(message.id_as<LogMessage::VulkanMessage>(),
              LogMessage::VulkanMessage::e_UNKNOWN);
    EXPECT_EQ(message.id(), 0U);
}

TEST(CoreLogMessage, IsConstexpr)
{
    constexpr LogMessage message = LogMessage::make(Probe::e_UNSIGNED, 7U);

    static_assert(message.argument() == 7U);
    static_assert(message.domain() == Domain::e_CORE);
    static_assert(message == LogMessage::from_raw(message.raw()));
    static_assert(LogMessage::make(Probe::e_FLOAT, 0.5F).float_argument() ==
                  0.5F);

    SUCCEED();
}

TEST(CoreLogMessage, AcceptsOnlyMessageEnumerations)
{
    // 'make' takes the domain from the type of the id, so an id whose type
    // has no domain is rejected when it is compiled: a plain number, an
    // enumeration of the wrong width, or one no domain claims.
    static_assert(LogMessageIdType<LogMessage::CoreMessage>);
    static_assert(LogMessageIdType<Probe>);
    static_assert(!LogMessageIdType<std::uint16_t>);
    static_assert(!LogMessageIdType<Domain>);
    static_assert(!LogMessageIdType<Stray>);

    SUCCEED();
}

// ---------------------------------------------------------------- check -----

TEST(CoreLogMessageId, AcceptsEachArgumentTypeForItsDeclarationsOnly)
{
    // The rule 'make' applies when a call is compiled: every type an argument
    // can have, against every argument a message can declare. An unsigned
    // number is shown two ways, so it suits two declarations.
    struct Row {
        Argument declared;
        bool     none;
        bool     unsigned_argument;
        bool     signed_argument;
        bool     float_argument;
    };
    constexpr std::array rows = {
        Row{Argument::e_NONE, true, false, false, false},
        Row{Argument::e_UNSIGNED, false, true, false, false},
        Row{Argument::e_SIGNED, false, false, true, false},
        Row{Argument::e_HEX, false, true, false, false},
        Row{Argument::e_FLOAT, false, false, false, true},
        Row{Argument::e_COUNT, false, false, false, false},
    };

    for (const Row& row : rows) {
        SCOPED_TRACE(static_cast<unsigned>(std::to_underlying(row.declared)));
        EXPECT_EQ(LogMessageId<void>::accepts(row.declared), row.none);
        EXPECT_EQ(LogMessageId<std::uint32_t>::accepts(row.declared),
                  row.unsigned_argument);
        EXPECT_EQ(LogMessageId<std::int32_t>::accepts(row.declared),
                  row.signed_argument);
        EXPECT_EQ(LogMessageId<float>::accepts(row.declared),
                  row.float_argument);
    }
}

TEST(CoreLogMessageId, KeepsTheDomainAndTheIdOfItsEnumerator)
{
    constexpr LogMessageId<void>  none = LogMessage::MetalMessage::e_UNKNOWN;
    constexpr LogMessageId<float> floating = Probe::e_FLOAT;

    static_assert(none.domain() == Domain::e_METAL);
    static_assert(none.value() == 0U);
    static_assert(floating.domain() == Domain::e_CORE);
    static_assert(floating.value() == 4U);

    SUCCEED();
}

// ------------------------------------------------------------ preconditions
// ---

// GoogleTest runs suites named '*DeathTest' first, before any threads exist,
// which is what makes forking the process to watch it die safe.
TEST(CoreLogMessageDeathTest, IdAsRejectsAnIdFromAnotherDomain)
{
    // Asking a Vulkan message for its CoreMessage reads the right bits through
    // the wrong enumeration. The assert in 'id_as' is the only thing that
    // catches it, so this proves it fires.
    //
    // EXPECT_DEBUG_DEATH, not EXPECT_DEATH: release builds define NDEBUG,
    // which compiles the assert out. There the statement just runs, and the
    // test passes without claiming a guarantee the build does not provide.
    const LogMessage vulkan = LogMessage::make(
        LogMessage::VulkanMessage::e_UNKNOWN);

    EXPECT_DEBUG_DEATH((void)vulkan.id_as<LogMessage::CoreMessage>(), "");
}

}  // close unnamed namespace
