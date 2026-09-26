// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_error_catalog.h                                               -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_ERROR_CATALOG_H
#define INCLUDED_ENGINE_CORE_ERROR_CATALOG_H

//@PURPOSE: Provide the names of the domains, kinds and reasons of errors.
//
//@CLASSES:
//  engine::core::error::ErrorReasonNames: names of one reason enumeration
//  engine::core::error::ErrorCatalog: name lookup for each part of an error
//  engine::core::error::ErrorDescriptor: the three names of one error
//
//@SEE_ALSO: core_domain, core_error, core_error_format
//
//@DESCRIPTION: This component names the parts of an [Error]: its domain,
// kind and reason. Each name table holds one name per enumerator, in
// enumerator order, and a [static_assert] checks the count, so a new
// enumerator without its name does not compile. The domain names are
// [k_DOMAIN_NAMES], kept with [Domain] and shared with log messages. A value
// that no enumerator has is named ["?"]: [Error::from_raw] accepts any 64
// bits, so a corrupt error still gets a descriptor.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Naming the Parts of an Error
///- - - - - - - - - - - - - - - - - - - -
// ```
//  const ErrorDescriptor names = ErrorDescriptor::make(error);
//  // names.domain() == "vulkan", names.kind() == "unknown", ...
// ```

// engine
#include "engine/core/core_domain.h"
#include "engine/core/core_error.h"

// std
#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace engine::core {
namespace error {

// =======================
// struct ErrorReasonNames
// =======================

/// This traits type holds the names of the enumerators of [t_REASON]: each
/// specialization defines [k_NAMES], one name per enumerator, in enumerator
/// order.
template <class t_REASON> struct ErrorReasonNames;

/// This concept is satisfied by a reason enumeration whose names are known:
/// an [ErrorReasonType] with an [ErrorReasonNames] specialization.
template <class t_REASON>
concept NamedErrorReason = ErrorReasonType<t_REASON> &&
                           requires { ErrorReasonNames<t_REASON>::k_NAMES; };

// --------------------------------
// ErrorReasonNames specializations
// --------------------------------

/// Name the enumerators of [Error::CoreReason].
template <> struct ErrorReasonNames<Error::CoreReason> {
    static constexpr auto k_NAMES = std::to_array<std::string_view>({
        "unknown",
    });
};

/// Name the enumerators of [Error::VulkanReason].
template <> struct ErrorReasonNames<Error::VulkanReason> {
    static constexpr auto k_NAMES = std::to_array<std::string_view>({
        "unknown",
    });
};

/// Name the enumerators of [Error::MetalReason].
template <> struct ErrorReasonNames<Error::MetalReason> {
    static constexpr auto k_NAMES = std::to_array<std::string_view>({
        "unknown",
    });
};

// One name per enumerator: a new reason without its name does not compile.
static_assert(ErrorReasonNames<Error::CoreReason>::k_NAMES.size() ==
              std::to_underlying(Error::CoreReason::e_COUNT));
static_assert(ErrorReasonNames<Error::VulkanReason>::k_NAMES.size() ==
              std::to_underlying(Error::VulkanReason::e_COUNT));
static_assert(ErrorReasonNames<Error::MetalReason>::k_NAMES.size() ==
              std::to_underlying(Error::MetalReason::e_COUNT));

// ==================
// class ErrorCatalog
// ==================

/// This utility class names each part of an [Error]. A value that no
/// enumerator has is named ["?"].
class ErrorCatalog final {
  private:
    // CLASS DATA

    /// Name of each [Error::Kind], in enumerator order.
    inline static constexpr auto k_KIND_NAMES =
        std::to_array<std::string_view>({
            "unknown",
        });

    /// Name of a value that no enumerator has.
    inline static constexpr std::string_view k_UNNAMED = "?";

    // One name per enumerator: a new kind without its name does not compile.
    static_assert(k_KIND_NAMES.size() ==
                  std::to_underlying(Error::Kind::e_COUNT));

    // PRIVATE CLASS METHODS

    /// Return the name at the specified [index] of the specified [names], or
    /// [k_UNNAMED] if [index] is out of range.
    template <std::size_t t_SIZE>
    [[nodiscard]] static constexpr std::string_view
    lookup(const std::array<std::string_view, t_SIZE>& names,
           const std::size_t                           index) noexcept;

  public:
    // CLASS METHODS

    /// Return the name of the specified [domain], or ["?"] if no enumerator
    /// has its value.
    [[nodiscard("A stringified variant shall be used or "
                "printed")]] static constexpr std::string_view
    to_string_domain(const Error::Domain domain) noexcept;

    /// Return the name of the specified [kind], or ["?"] if no enumerator has
    /// its value.
    [[nodiscard("A stringified variant shall be used or "
                "printed")]] static constexpr std::string_view
    to_string_kind(const Error::Kind kind) noexcept;

    /// Return the name of the specified [reason], or ["?"] if no enumerator
    /// has its value.
    template <NamedErrorReason t_REASON>
    [[nodiscard("A stringified variant shall be used or "
                "printed")]] static constexpr std::string_view
    to_string_reason(const t_REASON reason) noexcept;

    /// Return the name of the reason of the specified [error], read in the
    /// reason enumeration of its domain, or ["?"] if its domain or its reason
    /// has no name.
    [[nodiscard("A stringified variant shall be used or "
                "printed")]] static constexpr std::string_view
    to_string_reason_of(const Error error) noexcept;
};

// =====================
// class ErrorDescriptor
// =====================

/// This value-semantic class holds the three names of an error: its domain,
/// kind and reason. The names view static strings, so a descriptor is cheap
/// to copy and never dangles.
class ErrorDescriptor final {
  private:
    // DATA

    std::string_view d_domain;  // name of the domain
    std::string_view d_kind;    // name of the kind
    std::string_view d_reason;  // name of the reason

    // PRIVATE CREATORS

    /// Create an [ErrorDescriptor] object holding the specified [domain],
    /// [kind] and [reason] names.
    explicit constexpr ErrorDescriptor(const std::string_view domain,
                                       const std::string_view kind,
                                       const std::string_view reason) noexcept;

  public:
    // CLASS METHODS

    /// Return the names of the specified [error], its reason read as a
    /// [t_REASON]. The behavior is undefined unless
    /// [error.domain() == ErrorReasonTraits<t_REASON>::k_DOMAIN].
    template <NamedErrorReason t_REASON>
    [[nodiscard]] static constexpr ErrorDescriptor
    make(const Error error) noexcept;

    /// Return the names of the specified [error], a part with no name being
    /// named ["?"].
    [[nodiscard]] static constexpr ErrorDescriptor
    make(const Error error) noexcept;

    // CREATORS

    /// Create an [ErrorDescriptor] object having the same value as the
    /// specified [original] object.
    //! ErrorDescriptor(const ErrorDescriptor& original) = default;

    /// Destroy this object.
    //! ~ErrorDescriptor() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! ErrorDescriptor& operator=(const ErrorDescriptor& rhs) = default;

    // ACCESSORS

    /// Return the name of the domain.
    [[nodiscard]] constexpr std::string_view domain() const noexcept;

    /// Return the name of the kind.
    [[nodiscard]] constexpr std::string_view kind() const noexcept;

    /// Return the name of the reason.
    [[nodiscard]] constexpr std::string_view reason() const noexcept;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

// ------------------
// class ErrorCatalog
// ------------------

// PRIVATE CLASS METHODS

template <std::size_t t_SIZE>
inline constexpr std::string_view
ErrorCatalog::lookup(const std::array<std::string_view, t_SIZE>& names,
                     const std::size_t index) noexcept
{
    return index < names.size() ? names[index] : k_UNNAMED;
}

// CLASS METHODS

inline constexpr std::string_view
ErrorCatalog::to_string_domain(const Error::Domain domain) noexcept
{
    return lookup(k_DOMAIN_NAMES,
                  static_cast<std::size_t>(std::to_underlying(domain)));
}

inline constexpr std::string_view
ErrorCatalog::to_string_kind(const Error::Kind kind) noexcept
{
    return lookup(k_KIND_NAMES,
                  static_cast<std::size_t>(std::to_underlying(kind)));
}

template <NamedErrorReason t_REASON>
inline constexpr std::string_view
ErrorCatalog::to_string_reason(const t_REASON reason) noexcept
{
    return lookup(ErrorReasonNames<t_REASON>::k_NAMES,
                  static_cast<std::size_t>(std::to_underlying(reason)));
}

inline constexpr std::string_view
ErrorCatalog::to_string_reason_of(const Error error) noexcept
{
    switch (error.domain()) {
    case Error::Domain::e_UNKNOWN:
    case Error::Domain::e_COUNT: return k_UNNAMED;  // RETURN
    case Error::Domain::e_CORE:
        return to_string_reason(error.reason_as<Error::CoreReason>());
        // RETURN
    case Error::Domain::e_VULKAN:
        return to_string_reason(error.reason_as<Error::VulkanReason>());
        // RETURN
    case Error::Domain::e_METAL:
        return to_string_reason(error.reason_as<Error::MetalReason>());
        // RETURN
    }

    // Not dead code, although every enumerator has a case above: the domain
    // byte comes from [d_raw], and [Error::from_raw] accepts any 64 bits, so
    // [domain()] can hold a value no enumerator names. Without this line
    // that is undefined behavior, and GCC -- the Profile L compiler --
    // refuses to build it under -Werror=return-type. Clang accepts it
    // silently.
    return k_UNNAMED;
}

// ---------------------
// class ErrorDescriptor
// ---------------------

// PRIVATE CREATORS

inline constexpr ErrorDescriptor::ErrorDescriptor(
    std::string_view domain,
    std::string_view kind,
    std::string_view reason) noexcept
: d_domain(domain)
, d_kind(kind)
, d_reason(reason)
{
}

// CLASS METHODS

template <NamedErrorReason t_REASON>
inline constexpr ErrorDescriptor
ErrorDescriptor::make(const Error error) noexcept
{
    return ErrorDescriptor(
        ErrorCatalog::to_string_domain(error.domain()),
        ErrorCatalog::to_string_kind(error.kind()),
        ErrorCatalog::to_string_reason(error.reason_as<t_REASON>()));
}

inline constexpr ErrorDescriptor
ErrorDescriptor::make(const Error error) noexcept
{
    return ErrorDescriptor(ErrorCatalog::to_string_domain(error.domain()),
                           ErrorCatalog::to_string_kind(error.kind()),
                           ErrorCatalog::to_string_reason_of(error));
}

// ACCESSORS

inline constexpr std::string_view ErrorDescriptor::domain() const noexcept
{
    return d_domain;
}

inline constexpr std::string_view ErrorDescriptor::kind() const noexcept
{
    return d_kind;
}

inline constexpr std::string_view ErrorDescriptor::reason() const noexcept
{
    return d_reason;
}

}  // close namespace error
}  // close namespace engine::core

#endif
