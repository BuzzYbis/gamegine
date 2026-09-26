// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_error_catalog.t.cpp                                           -*-C++-*-

//@PURPOSE: Verify that every error part has a name, and that a malformed error
// still describes itself without reading out of bounds.
//
//@DESCRIPTION: The catalog is what turns an 'Error' into something a person
// can read in a log. Four properties have to hold:
//
//: o Every enumerator is named. The 'static_assert's in the catalog catch a
//:   table that is the wrong LENGTH; these tests catch one that is the right
//:   length but holds an empty or placeholder entry.
//:
//: o Names are log-safe and unique. The formatter joins them with '/', so a
//:   name containing '/' or a space makes a log line ambiguous, and two
//:   enumerators sharing a name make it impossible to tell them apart.
//:
//: o Anything that is not a real enumerator -- the 'e_COUNT' sentinel, or a
//:   value 'from_raw' produced -- is reported as '?', never as a real name and
//:   never by reading past a table.
//:
//: o The type-erased 'make', which the logger uses, agrees with the typed one.
//
// The tests iterate from 0 to each enum's 'e_COUNT', so reasons added later
// are covered without editing this file.

#include <engine/core/core_error_catalog.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using engine::core::error::Error;
using engine::core::error::ErrorCatalog;
using engine::core::error::ErrorDescriptor;

/// What the catalog returns for a value that names nothing.
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

/// Return true if 'name' can appear in a '/'-separated log line unambiguously:
/// non-empty, and made only of lower-case letters, digits and underscores.
bool is_log_safe(const std::string_view name)
{
    return !name.empty() && std::ranges::all_of(name, [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

/// Return true if no two entries of 'names' are equal.
bool all_distinct(std::vector<std::string_view> names)
{
    std::ranges::sort(names);
    return std::ranges::adjacent_find(names) == names.end();
}

// ---------------------------------------------------------------- domain ---

TEST(CoreErrorCatalog, NamesEveryDomain)
{
    EXPECT_EQ(ErrorCatalog::to_string_domain(Error::Domain::e_UNKNOWN),
              "unknown");
    EXPECT_EQ(ErrorCatalog::to_string_domain(Error::Domain::e_CORE), "core");
    EXPECT_EQ(ErrorCatalog::to_string_domain(Error::Domain::e_VULKAN),
              "vulkan");
    EXPECT_EQ(ErrorCatalog::to_string_domain(Error::Domain::e_METAL), "metal");
}

TEST(CoreErrorCatalog, DomainNamesAreLogSafeAndDistinct)
{
    std::vector<std::string_view> names;
    for (const auto domain : every_value_of<Error::Domain>()) {
        const auto name = ErrorCatalog::to_string_domain(domain);
        EXPECT_TRUE(is_log_safe(name)) << "domain name '" << name << "'";
        names.push_back(name);
    }
    EXPECT_TRUE(all_distinct(names));
}

TEST(CoreErrorCatalog, SentinelDomainIsNeverNamed)
{
    EXPECT_EQ(ErrorCatalog::to_string_domain(Error::Domain::e_COUNT),
              k_unnamed);
}

TEST(CoreErrorCatalog, OutOfRangeDomainIsUnnamed)
{
    // 'Domain' has a fixed 8-bit underlying type, so any byte is a valid
    // value of the enum -- it just names nothing.
    EXPECT_EQ(ErrorCatalog::to_string_domain(static_cast<Error::Domain>(0x42)),
              k_unnamed);
}

// ------------------------------------------------------------------ kind ---

TEST(CoreErrorCatalog, KindNamesAreLogSafeAndDistinct)
{
    std::vector<std::string_view> names;
    for (const auto kind : every_value_of<Error::Kind>()) {
        const auto name = ErrorCatalog::to_string_kind(kind);
        EXPECT_TRUE(is_log_safe(name)) << "kind name '" << name << "'";
        names.push_back(name);
    }
    EXPECT_TRUE(all_distinct(names));
}

TEST(CoreErrorCatalog, SentinelAndOutOfRangeKindsAreUnnamed)
{
    EXPECT_EQ(ErrorCatalog::to_string_kind(Error::Kind::e_COUNT), k_unnamed);
    EXPECT_EQ(ErrorCatalog::to_string_kind(static_cast<Error::Kind>(0xFF)),
              k_unnamed);
}

// --------------------------------------------------------------- reasons ---

/// The same properties hold for every domain's reason table, so they are
/// written once and run for each.
template <class t_REASON>
class CoreErrorCatalogReason : public testing::Test {};

using ReasonTypes =
    testing::Types<Error::CoreReason, Error::VulkanReason, Error::MetalReason>;
TYPED_TEST_SUITE(CoreErrorCatalogReason, ReasonTypes);

TYPED_TEST(CoreErrorCatalogReason, EveryReasonIsNamedLogSafeAndDistinct)
{
    std::vector<std::string_view> names;
    for (const auto reason : every_value_of<TypeParam>()) {
        const auto name = ErrorCatalog::to_string_reason(reason);
        EXPECT_NE(name, k_unnamed)
            << "reason " << std::to_underlying(reason) << " has no name";
        EXPECT_TRUE(is_log_safe(name)) << "reason name '" << name << "'";
        names.push_back(name);
    }
    EXPECT_TRUE(all_distinct(names));
}

TYPED_TEST(CoreErrorCatalogReason, SentinelAndOutOfRangeReasonsAreUnnamed)
{
    EXPECT_EQ(ErrorCatalog::to_string_reason(TypeParam::e_COUNT), k_unnamed);

    // The largest 16-bit value: as far past the table as a reason can get.
    EXPECT_EQ(ErrorCatalog::to_string_reason(static_cast<TypeParam>(0xFFFF)),
              k_unnamed);
}

TYPED_TEST(CoreErrorCatalogReason, ErasedLookupAgreesWithTypedLookup)
{
    // 'to_string_reason_of' has to choose the table from the domain at run
    // time. It must land on the same name the typed lookup finds.
    //
    // Honest limit: while every reason table holds the same single name, a
    // lookup in the WRONG table would also agree. This becomes a real check
    // of the dispatch the moment two tables differ.
    for (const auto reason : every_value_of<TypeParam>()) {
        const Error error = Error::make(Error::Kind::e_UNKNOWN, reason);
        EXPECT_EQ(ErrorCatalog::to_string_reason_of(error),
                  ErrorCatalog::to_string_reason(reason));
    }
}

// -------------------------------------------------------------- dispatch ---

TEST(CoreErrorCatalog, UnknownDomainHasNoReasonTable)
{
    // Domain 0, reason 0. If the unknown domain were mistakenly routed to a
    // real table this would read "unknown" instead of "?".
    EXPECT_EQ(ErrorCatalog::to_string_reason_of(Error::from_raw(0)),
              k_unnamed);
}

TEST(CoreErrorCatalog, SentinelDomainHasNoReasonTable)
{
    const auto sentinel = std::to_underlying(Error::Domain::e_COUNT);
    EXPECT_EQ(ErrorCatalog::to_string_reason_of(Error::from_raw(sentinel)),
              k_unnamed);
}

TEST(CoreErrorCatalog, OutOfRangeDomainHasNoReasonTable)
{
    // Regression test. A domain byte no enumerator names falls through the
    // exhaustive switch in 'to_string_reason_of'; without the return after
    // it, this is undefined behaviour, and GCC refuses to build it.
    //
    // A release build may pass here by luck even when broken. The failure is
    // certain under GCC -Werror=return-type or -fsanitize=undefined.
    EXPECT_EQ(ErrorCatalog::to_string_reason_of(Error::from_raw(0x42)),
              k_unnamed);
}

// ------------------------------------------------------------ descriptor ---

TEST(CoreErrorDescriptor, TypedMakeNamesAllThreeParts)
{
    const Error error      = Error::make(Error::Kind::e_UNKNOWN,
                                         Error::VulkanReason::e_UNKNOWN);
    const auto  descriptor = ErrorDescriptor::make<Error::VulkanReason>(error);

    EXPECT_EQ(descriptor.domain(), "vulkan");
    EXPECT_EQ(descriptor.kind(), "unknown");
    EXPECT_EQ(descriptor.reason(), "unknown");
}

TYPED_TEST(CoreErrorCatalogReason, ErasedMakeAgreesWithTypedMake)
{
    // The logger only ever holds a propagated 'Error', whose reason type is
    // gone, so it uses the erased form. It must describe errors identically.
    for (const auto reason : every_value_of<TypeParam>()) {
        const Error error  = Error::make(Error::Kind::e_UNKNOWN, reason);
        const auto  typed  = ErrorDescriptor::make<TypeParam>(error);
        const auto  erased = ErrorDescriptor::make(error);

        EXPECT_EQ(erased.domain(), typed.domain());
        EXPECT_EQ(erased.kind(), typed.kind());
        EXPECT_EQ(erased.reason(), typed.reason());
    }
}

TEST(CoreErrorDescriptor, CorruptErrorStillDescribesItself)
{
    // Domain 0x42 and reason 0xFFFF name nothing; kind 0 is a real kind. The
    // descriptor must say what it can and mark the rest, not guess.
    const auto descriptor = ErrorDescriptor::make(
        Error::from_raw(0x00000000FFFF0042ull));

    EXPECT_EQ(descriptor.domain(), k_unnamed);
    EXPECT_EQ(descriptor.kind(), "unknown");
    EXPECT_EQ(descriptor.reason(), k_unnamed);
}

TEST(CoreErrorDescriptor, IsUsableInAConstantExpression)
{
    // Evaluated at compile time: if any part stopped being constexpr, this
    // file would fail to build.
    constexpr auto descriptor = ErrorDescriptor::make<Error::MetalReason>(
        Error::make(Error::Kind::e_UNKNOWN, Error::MetalReason::e_UNKNOWN));
    static_assert(descriptor.domain() == "metal");
    static_assert(descriptor.reason() == "unknown");

    constexpr auto erased = ErrorDescriptor::make(
        Error::make(Error::Kind::e_UNKNOWN, Error::CoreReason::e_UNKNOWN));
    static_assert(erased.domain() == "core");

    SUCCEED();
}

TEST(CoreErrorDescriptor, IsCheapToCopyAndOwnsNothing)
{
    // The logger's consumer builds one per record. A member that allocated
    // or needed a destructor -- a std::string, say -- would fail here.
    EXPECT_TRUE(std::is_trivially_copyable_v<ErrorDescriptor>);
    EXPECT_TRUE(std::is_trivially_destructible_v<ErrorDescriptor>);
}

}  // close unnamed namespace
