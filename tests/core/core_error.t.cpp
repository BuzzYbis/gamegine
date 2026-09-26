// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_error.t.cpp                                                -*-C++-*-

//@PURPOSE: Verify the packing, propagation and cost guarantees of 'Error'.
//
//@DESCRIPTION: 'Error' is a 64-bit value returned from every fallible
// operation in the engine (D9). Three properties have to hold, and only the
// first is obvious:
//
//: o It packs and unpacks without losing a bit. A silent truncation here
//:   mislabels every error raised by one subsystem as another's.
//:
//: o It stays 8 bytes and trivially copyable. That is what lets it be
//:   returned in a register and memcpy'd into the logger's SPSC ring; a field
//:   added later would cost that silently, so the guarantee is asserted
//:   rather than assumed.
//:
//: o The propagation macros carry the error out unchanged, including through
//:   a function whose 'Result' has a different value type.

#include <engine/core/core_error.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using engine::core::Result;
using engine::core::Status;
using engine::core::error::Error;
using engine::core::error::make_unexpected;

// A type that cannot be default-constructed, which is the normal case under
// D9: a constructor cannot report failure, so fallible types expose a static
// factory and keep the constructor private.
class Resource {
  public:
    static Result<Resource> create(std::string name)
    {
        if (name.empty()) {
            return make_unexpected(Error::make(Error::Kind::e_UNKNOWN,
                                               Error::CoreReason::e_UNKNOWN,
                                               1u));
        }
        return Resource(std::move(name));
    }

    [[nodiscard]] const std::string& name() const
    {
        return d_name;
    }

    Resource() = delete;

  private:
    explicit Resource(std::string name)
    : d_name(std::move(name))
    {
    }

    std::string d_name;
};

// ----------------------------------------------------------------- cost -----

TEST(CoreError, IsExactlySixtyFourBits)
{
    // The engine returns this by value everywhere and memcpy's it into the
    // logger's ring. Both stop being free the moment it grows.
    EXPECT_EQ(sizeof(Error), sizeof(std::uint64_t));
    EXPECT_EQ(alignof(Error), alignof(std::uint64_t));
    EXPECT_TRUE(std::is_trivially_copyable_v<Error>);
    EXPECT_TRUE(std::is_trivially_destructible_v<Error>);
}

// -------------------------------------------------------------- packing -----

TEST(CoreError, PacksEveryFieldWithoutCollision)
{
    const Error error = Error::make(Error::Kind::e_UNKNOWN,
                                    Error::CoreReason::e_UNKNOWN,
                                    0x0150A3B2u);

    EXPECT_EQ(error.domain(), Error::Domain::e_CORE);
    EXPECT_EQ(error.kind(), Error::Kind::e_UNKNOWN);
    EXPECT_EQ(error.reason(), 0u);
    EXPECT_EQ(error.context(), 0x0150A3B2u);
}

TEST(CoreError, ContextOccupiesTheTopThirtyTwoBitsOnly)
{
    const Error error = Error::make(Error::Kind::e_UNKNOWN,
                                    Error::VulkanReason::e_UNKNOWN,
                                    0xFFFFFFFFu);

    EXPECT_EQ(error.domain(), Error::Domain::e_VULKAN);
    EXPECT_EQ(error.kind(), Error::Kind::e_UNKNOWN);
    EXPECT_EQ(error.reason(), 0u);
    EXPECT_EQ(error.context(), 0xFFFFFFFFu);

    EXPECT_EQ(error.raw() & 0xFFFFFFFFull, 0x00000002ull);
}

TEST(CoreError, RawRoundTrips)
{
    const Error original = Error::make(Error::Kind::e_UNKNOWN,
                                       Error::MetalReason::e_UNKNOWN,
                                       0x0150A3B2u);
    const Error restored = Error::from_raw(original.raw());

    EXPECT_EQ(restored, original);
    EXPECT_EQ(restored.domain(), Error::Domain::e_METAL);
    EXPECT_EQ(restored.context(), 0x0150A3B2u);
}

TEST(CoreError, SignedContextSurvivesNegativeValues)
{
    // The bits are reinterpreted, not converted, so the exact value returns.
    for (const std::int32_t value :
         {-1, -42, -2147483647 - 1, 0, 2147483647}) {
        const Error error = Error::make_with_signed_context(
            Error::Kind::e_UNKNOWN, Error::CoreReason::e_UNKNOWN, value);
        EXPECT_EQ(error.signed_context(), value);
    }
}

TEST(CoreError, DistinctDomainsProduceDistinctErrors)
{
    const Error core  = Error::make(Error::Kind::e_UNKNOWN,
                                    Error::CoreReason::e_UNKNOWN);
    const Error metal = Error::make(Error::Kind::e_UNKNOWN,
                                    Error::MetalReason::e_UNKNOWN);

    EXPECT_NE(core, metal);
    EXPECT_TRUE(core == core);
}

TEST(CoreError, ReasonAsReturnsTheTypedReason)
{
    const Error error = Error::make(Error::Kind::e_UNKNOWN,
                                    Error::VulkanReason::e_UNKNOWN);

    EXPECT_EQ(error.reason_as<Error::VulkanReason>(),
              Error::VulkanReason::e_UNKNOWN);
    EXPECT_EQ(error.reason(), 0u);
}

TEST(CoreError, IsConstexpr)
{
    constexpr Error error = Error::make(Error::Kind::e_UNKNOWN,
                                        Error::CoreReason::e_UNKNOWN,
                                        7u);

    static_assert(error.context() == 7u);
    static_assert(error.domain() == Error::Domain::e_CORE);
    static_assert(error == Error::from_raw(error.raw()));

    SUCCEED();
}

// ---------------------------------------------------------- propagation -----

Result<int> succeeding_int()
{
    return 11;
}

Result<int> failing_int()
{
    return make_unexpected(Error::make(Error::Kind::e_UNKNOWN,
                                       Error::CoreReason::e_UNKNOWN,
                                       0xABu));
}

Status succeeding_status()
{
    return Status{};
}

Status failing_status()
{
    return make_unexpected(Error::make(Error::Kind::e_UNKNOWN,
                                       Error::VulkanReason::e_UNKNOWN,
                                       0xCDu));
}

// Deliberately returns a different value type than anything it calls: the
// error has to convert across 'Result' instantiations.
Result<std::string> propagates_across_result_types()
{
    GAMEGINE_ERROR_TRY(succeeding_status());
    GAMEGINE_ERROR_TRY_ASSIGN(const auto count, succeeding_int());
    GAMEGINE_ERROR_TRY_ASSIGN(const auto resource, Resource::create("pool"));
    return resource.name() + std::to_string(count);
}

Result<std::string> propagates_status_failure()
{
    GAMEGINE_ERROR_TRY(failing_status());
    return std::string("unreachable");
}

Result<std::string> propagates_value_failure()
{
    GAMEGINE_ERROR_TRY_ASSIGN(const auto count, failing_int());
    return std::to_string(count);
}

TEST(CoreErrorTry, PassesValuesThroughOnSuccess)
{
    const auto result = propagates_across_result_types();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "pool11");
}

TEST(CoreErrorTry, ReturnsTheErrorUnchangedFromAStatus)
{
    const auto result = propagates_status_failure();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain(), Error::Domain::e_VULKAN);
    EXPECT_EQ(result.error().context(), 0xCDu);
}

TEST(CoreErrorTry, ReturnsTheErrorUnchangedFromAValue)
{
    const auto result = propagates_value_failure();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain(), Error::Domain::e_CORE);
    EXPECT_EQ(result.error().context(), 0xABu);
}

TEST(CoreErrorTry, DeclaresTypesThatHaveNoDefaultConstructor)
{
    // The reason TRY_ASSIGN declares rather than assigns. 'Resource' cannot
    // be default-constructed, so an assigning macro could not bind it at all.
    // Because the macro takes a whole declaration, 'const auto' works too.
    static_assert(!std::is_default_constructible_v<Resource>);

    const auto result = propagates_across_result_types();
    EXPECT_TRUE(result.has_value());
}

TEST(CoreErrorTry, EvaluatesItsExpressionExactlyOnce)
{
    // A macro that names its argument twice would run the call twice, which
    // for a fallible operation means doing the work twice.
    int        calls = 0;
    const auto count = [&calls]() -> Result<int> {
        ++calls;
        return 1;
    };

    const auto run = [&count]() -> Status {
        GAMEGINE_ERROR_TRY_ASSIGN(const auto value, count());
        (void)value;
        return Status{};
    };

    ASSERT_TRUE(run().has_value());
    EXPECT_EQ(calls, 1);
}

// ------------------------------------------------------------- exceptions ---

TEST(CoreError, BuildsWithoutExceptions)
{
    // D9: first-party code neither throws nor catches, enforced by the
    // compiler rather than by convention. If someone re-enables exceptions
    // this fails, and the decision gets revisited deliberately.
#ifdef __cpp_exceptions
    FAIL() << "compiled with exceptions enabled; D9 requires -fno-exceptions";
#else
    SUCCEED();
#endif
}

// ------------------------------------------------------------ preconditions
// ---

// GoogleTest runs suites named '*DeathTest' first, before any threads exist,
// which is what makes forking the process to watch it die safe.
TEST(CoreErrorDeathTest, ReasonAsRejectsAReasonFromAnotherDomain)
{
    // Asking a Vulkan error for its CoreReason reads the right bits through
    // the wrong table. The assert in 'reason_as' is the only thing that
    // catches it, so this proves it fires.
    //
    // EXPECT_DEBUG_DEATH, not EXPECT_DEATH: release builds define NDEBUG,
    // which compiles the assert out. There the statement just runs, and the
    // test passes without claiming a guarantee the build does not provide.
    const Error vulkan = Error::make(Error::Kind::e_UNKNOWN,
                                     Error::VulkanReason::e_UNKNOWN);

    EXPECT_DEBUG_DEATH((void)vulkan.reason_as<Error::CoreReason>(), "");
}

}  // namespace
