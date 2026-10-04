// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_error.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_ERROR_H
#define INCLUDED_ENGINE_CORE_ERROR_H

//@PURPOSE: Provide a 64-bit error value and the [Result] and [Status] aliases.
//
//@CLASSES:
//  engine::core::error::ErrorReasonTraits: domain of each reason enumeration
//  engine::core::error::Error: 64-bit value saying what failed and why
//
//@MACROS:
//  GAMEGINE_ERROR_TRY: early return of a failed [Result] or [Status]
//  GAMEGINE_ERROR_TRY_ASSIGN: early return, or a declaration from the value
//
//@SEE_ALSO: core_domain, core_error_catalog, core_error_format
//
//@DESCRIPTION: This component provides [Error], a value saying what failed
// and why in 64 bits, so that it is returned in a register and stored in a
// log record as is. Errors are values, never exceptions (D9): a fallible
// function returns a [Result<T>], or a [Status] when it has no value, and
// [GAMEGINE_ERROR_TRY] passes a failure on to the caller.
//
///Layout
///------
// ```
//  bits  0 -  7  domain   subsystem that failed        [Domain]
//  bits  8 - 15  kind     what kind of failure         [Kind]
//  bits 16 - 31  reason   why, in the domain's terms   [CoreReason], ...
//  bits 32 - 63  context  a number: count, handle, VkResult...
// ```
// Each reason enumeration belongs to one domain, set by its
// [ErrorReasonTraits] specialization. [Error::make] takes the domain from
// the type of the reason, so a reason cannot be paired with the wrong one.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Returning and Propagating an Error
///- - - - - - - - - - - - - - - - - - - - - - -
// A failing function returns its error; a caller passes it on:
// ```
//  Status check_extent(const std::uint32_t width)
//  {
//      if (width == 0) {
//          return make_unexpected(Error::make(Error::Kind::e_UNKNOWN,
//                                             Error::VulkanReason::e_UNKNOWN,
//                                             width));
//      }
//      return {};
//  }
//
//  Status create_swapchain(const std::uint32_t width)
//  {
//      GAMEGINE_ERROR_TRY(check_extent(width));  // returns its error, if any
//      ...
//  }
// ```

// engine
#include "engine/core/core_domain.h"

// std
#include <bit>
#include <cassert>
#include <cstdint>
#include <expected>
#include <type_traits>
#include <utility>

namespace engine::core {
namespace error {

                         // ========================
                         // struct ErrorReasonTraits
                         // ========================

/// This traits type associates a reason enumeration with the domain it
/// belongs to: each specialization defines [k_DOMAIN]. It must be specialized
/// for every reason enumeration accepted by [Error::make] and
/// [Error::reason_as].
template <class t_REASON> struct ErrorReasonTraits;

/// This concept is satisfied by a reason enumeration [Error] accepts: a
/// scoped enumeration of [std::uint16_t] with an [ErrorReasonTraits]
/// specialization.
template <class t_REASON>
concept ErrorReasonType = std::is_scoped_enum_v<t_REASON> && requires {
    requires std::same_as<std::underlying_type_t<t_REASON>, std::uint16_t>;

    ErrorReasonTraits<t_REASON>::k_DOMAIN;
};

                         // ===========
                         // class Error
                         // ===========

/// This value-semantic type packs an error into 64 bits: a domain, a kind, a
/// domain-specific reason and a 32-bit context (see the component
/// documentation for the layout). It is trivially copyable, and two errors
/// have the same value if their [raw()] values are equal. Discarding one is
/// a compile error under [-Werror]: an error is returned or logged, never
/// dropped.
class [[nodiscard("An error shall not be discarded")]] Error final {
  public:
    // TYPES

    /// [Domain] is an alias for [engine::core::Domain], the subsystem raising
    /// the error; log messages share it.
    using Domain = ::engine::core::Domain;

    /// Enumeration used to identify the kind of an error, independently of
    /// its domain.
    enum class Kind : std::uint8_t {
        e_UNKNOWN = 0,  // no more precise kind

        e_COUNT,  // number of kinds: always last, never raised
    };

    /// Enumeration used to identify why the [core] subsystem failed.
    enum class CoreReason : std::uint16_t {
        e_UNKNOWN = 0,  // no more precise reason

        e_COUNT,  // number of reasons: always last, never raised
    };

    /// Enumeration used to identify why the [vulkan] subsystem failed.
    enum class VulkanReason : std::uint16_t {
        e_UNKNOWN = 0,  // no more precise reason

        e_COUNT,  // number of reasons: always last, never raised
    };

    /// Enumeration used to identify why the [metal] subsystem failed.
    enum class MetalReason : std::uint16_t {
        e_UNKNOWN = 0,  // no more precise reason

        e_COUNT,  // number of reasons: always last, never raised
    };

  private:
    // DATA

    std::uint64_t d_raw;  // domain, kind, reason and context, packed

    // PRIVATE CREATORS

    /// Create an [Error] object holding the specified [raw] value.
    explicit constexpr Error(const std::uint64_t raw) noexcept;

  public:
    // CLASS METHODS

    /// Return an error of the domain of [t_REASON], made of the specified
    /// [kind] and [reason]. Optionally specify a [context], a number whose
    /// meaning depends on the reason; it is 0 by default.
    template <ErrorReasonType t_REASON>
        requires std::same_as<decltype(ErrorReasonTraits<t_REASON>::k_DOMAIN),
                              const Error::Domain>
    [[nodiscard("An error is created only when raised and shall not be "
                "discarded. It is either returned as a value through a proper "
                "`std::expected` or logged and swallowed.")]]
    static constexpr Error make(const Kind          kind,
                                const t_REASON      reason,
                                const std::uint32_t context = 0) noexcept;

    /// Return an error as [make] does, from the specified [kind] and
    /// [reason]. Optionally specify a signed [context], 0 by default, kept
    /// bit for bit so that [signed_context()] returns it: a negative
    /// [VkResult], typically.
    template <ErrorReasonType t_REASON>
        requires std::same_as<decltype(ErrorReasonTraits<t_REASON>::k_DOMAIN),
                              const Error::Domain>
    [[nodiscard("An error is created only when raised and shall not be "
                "discarded. It is either returned as a value through a proper "
                "`std::expected` or logged and swallowed.")]]
    static constexpr Error
    make_with_signed_context(const Kind         kind,
                             const t_REASON     reason,
                             const std::int32_t context = 0) noexcept;

    /// Return the error whose packed value is the specified [raw], the
    /// inverse of [raw()]. Note that any 64-bit value is accepted, even one
    /// whose parts no enumerator names.
    [[nodiscard("An error is created only when raised and shall not be "
                "discarded. It is either returned as a value through a proper "
                "`std::expected` or logged and swallowed.")]]
    static constexpr Error from_raw(const std::uint64_t raw) noexcept;

    // CREATORS

    /// Create an [Error] object having the same value as the specified
    /// [original] object.
    //! Error(const Error& original) = default;

    /// Destroy this object.
    //! ~Error() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! Error& operator=(const Error& rhs) = default;

    // ACCESSORS

    /// Return the domain of this error.
    [[nodiscard]] constexpr Domain domain() const noexcept;

    /// Return the kind of this error.
    [[nodiscard]] constexpr Kind kind() const noexcept;

    /// Return the reason of this error as a [t_REASON]. The behavior is
    /// undefined unless
    /// [domain() == ErrorReasonTraits<t_REASON>::k_DOMAIN].
    template <ErrorReasonType t_REASON>
        requires std::same_as<decltype(ErrorReasonTraits<t_REASON>::k_DOMAIN),
                              const Error::Domain>
    [[nodiscard]] constexpr t_REASON reason_as() const noexcept;

    /// Return the reason of this error as its 16-bit value, whatever its
    /// domain.
    [[nodiscard]] constexpr std::uint16_t reason() const noexcept;

    /// Return the context of this error as an unsigned 32-bit number.
    [[nodiscard]] constexpr std::uint32_t context() const noexcept;

    /// Return the context of this error read as a signed 32-bit number, the
    /// inverse of [make_with_signed_context].
    [[nodiscard]] constexpr std::int32_t signed_context() const noexcept;

    /// Return the packed 64-bit value of this error, the inverse of
    /// [from_raw].
    [[nodiscard]] constexpr std::uint64_t raw() const noexcept;

    // HIDDEN FRIENDS

    /// Return [true] if the specified [lhs] and [rhs] errors have the same
    /// value, and [false] otherwise. Two errors have the same value if their
    /// [raw()] values are equal.
    friend constexpr bool operator==(Error lhs, Error rhs) noexcept = default;
};

                     // ---------------------------------
                     // ErrorReasonTraits specializations
                     // ---------------------------------

/// Associate [Error::CoreReason] with the [e_CORE] error domain.
template <> struct ErrorReasonTraits<Error::CoreReason> {
    static constexpr Error::Domain k_DOMAIN = Error::Domain::e_CORE;
};

/// Associate [Error::VulkanReason] with the [e_VULKAN] error domain.
template <> struct ErrorReasonTraits<Error::VulkanReason> {
    static constexpr Error::Domain k_DOMAIN = Error::Domain::e_VULKAN;
};

/// Associate [Error::MetalReason] with the [e_METAL] error domain.
template <> struct ErrorReasonTraits<Error::MetalReason> {
    static constexpr Error::Domain k_DOMAIN = Error::Domain::e_METAL;
};

/// Return the specified [error] wrapped in [std::unexpected], ready to be
/// returned from a function returning a [Result] or a [Status].
[[nodiscard("A returned error value shall not be ignored by the caller.")]]
constexpr std::unexpected<Error> make_unexpected(const Error error) noexcept;

// An [Error] is a plain 64-bit value: copied like one, destroyed for free.
static_assert(sizeof(Error) == sizeof(std::uint64_t));
static_assert(alignof(Error) == alignof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<Error>);
static_assert(std::is_trivially_destructible_v<Error>);

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

                         // -----------
                         // class Error
                         // -----------

// PRIVATE CREATORS

inline constexpr Error::Error(const std::uint64_t raw) noexcept
: d_raw(raw)
{
}

// CLASS METHODS

template <ErrorReasonType t_REASON>
    requires std::same_as<decltype(ErrorReasonTraits<t_REASON>::k_DOMAIN),
                          const Error::Domain>
inline constexpr Error Error::make(const Kind          kind,
                                   const t_REASON      reason,
                                   const std::uint32_t context) noexcept
{
    const std::uint64_t raw_value =
        static_cast<std::uint64_t>(ErrorReasonTraits<t_REASON>::k_DOMAIN) |
        (static_cast<std::uint64_t>(kind) << 8u) |
        (static_cast<std::uint64_t>(
             static_cast<std::uint16_t>(std::to_underlying(reason)))
         << 16u) |
        (static_cast<std::uint64_t>(context) << 32u);

    return Error(raw_value);
}

template <ErrorReasonType t_REASON>
    requires std::same_as<decltype(ErrorReasonTraits<t_REASON>::k_DOMAIN),
                          const Error::Domain>
inline constexpr Error
Error::make_with_signed_context(const Kind         kind,
                                const t_REASON     reason,
                                const std::int32_t context) noexcept
{
    // [bit_cast] rather than a numeric conversion: the intent is to keep the
    // same 32 bits, and it is the exact inverse of [signed_context()].
    return make(kind, reason, std::bit_cast<std::uint32_t>(context));
}

inline constexpr Error Error::from_raw(const std::uint64_t raw) noexcept
{
    return Error(raw);
}

// ACCESSORS

constexpr Error::Domain Error::domain() const noexcept
{
    return static_cast<Domain>(d_raw & 0xFFu);
}

constexpr Error::Kind Error::kind() const noexcept
{
    return static_cast<Kind>((d_raw >> 8u) & 0xFFu);
}

template <ErrorReasonType t_REASON>
    requires std::same_as<decltype(ErrorReasonTraits<t_REASON>::k_DOMAIN),
                          const Error::Domain>
constexpr t_REASON Error::reason_as() const noexcept
{
    assert(domain() == ErrorReasonTraits<t_REASON>::k_DOMAIN);

    return static_cast<t_REASON>(reason());
}

constexpr std::uint16_t Error::reason() const noexcept
{
    return static_cast<std::uint16_t>((d_raw >> 16u) & 0xFFFFu);
}

constexpr std::uint32_t Error::context() const noexcept
{
    return static_cast<std::uint32_t>((d_raw >> 32u));
}

constexpr std::int32_t Error::signed_context() const noexcept
{
    return std::bit_cast<std::int32_t>(context());
}

constexpr std::uint64_t Error::raw() const noexcept
{
    return d_raw;
}

                         // --------------
                         // FREE FUNCTIONS
                         // --------------

inline constexpr std::unexpected<Error>
make_unexpected(const Error error) noexcept
{
    return std::unexpected<Error>(error);
}

}  // close namespace error

/// [Result] is an alias for [std::expected] with [error::Error] as the error
/// type: what a fallible function returns when it has a value.
template <class t_TYPE> using Result = std::expected<t_TYPE, error::Error>;

/// [Status] is an alias for [Result] with no value: what a fallible function
/// returns when it has nothing else to return.
using Status = Result<void>;

}  // close namespace engine::core

// ============================================================================
//                             ERROR PROPAGATION
// ============================================================================

// [__LINE__] is itself a macro. Pasting it directly would produce the literal
// token [gamegineErrorTmp___LINE__], so one extra level of indirection is
// needed to force it to expand to its value before the paste happens.
#define GAMEGINE_ERROR_CAT_(a, b) a##b
#define GAMEGINE_ERROR_CAT(a, b) GAMEGINE_ERROR_CAT_(a, b)
#define GAMEGINE_ERROR_TMP GAMEGINE_ERROR_CAT(gamegineErrorTmp_, __LINE__)

/// Evaluate the specified [EXPRESSION], a [Result] or a [Status], and return
/// its error from the enclosing function if it failed. Any value it holds is
/// discarded.
#define GAMEGINE_ERROR_TRY(EXPRESSION)                                        \
    do {                                                                      \
        auto&& GAMEGINE_ERROR_TMP = (EXPRESSION);                             \
        if (!GAMEGINE_ERROR_TMP) [[unlikely]] {                               \
            return ::engine::core::error::make_unexpected(                    \
                GAMEGINE_ERROR_TMP.error());                                  \
        }                                                                     \
    } while (false)

/// Evaluate the specified [EXPRESSION], a [Result], and return its error from
/// the enclosing function if it failed; otherwise run the specified
/// [DECLARATION], initialized from the value:
/// ```
/// GAMEGINE_ERROR_TRY_ASSIGN(auto window, Window::create(config));
/// ```
/// [DECLARATION] declares a new variable, so its type needs no default
/// constructor: the usual case, since fallible types are built by factories
/// returning a [Result] (D9). Note that the macro expands to several
/// statements and cannot be wrapped in [do { } while (false)], because the
/// declaration must outlive it: use it as a statement in a braced block,
/// never as the lone body of an unbraced [if], [for] or [while].
// clang-tidy's bugprone-macro-parentheses wants [(DECLARATION)], but
// [DECLARATION] is a whole declaration such as [auto window], and
// [(auto window) = ...] is not valid C++. Suppressed for this macro only.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define GAMEGINE_ERROR_TRY_ASSIGN(DECLARATION, EXPRESSION)                    \
    auto&& GAMEGINE_ERROR_TMP = (EXPRESSION);                                 \
    if (!GAMEGINE_ERROR_TMP) [[unlikely]] {                                   \
        return ::engine::core::error::make_unexpected(                        \
            GAMEGINE_ERROR_TMP.error());                                      \
    }                                                                         \
    DECLARATION = std::move(*GAMEGINE_ERROR_TMP)
// NOLINTEND(bugprone-macro-parentheses)

#endif
