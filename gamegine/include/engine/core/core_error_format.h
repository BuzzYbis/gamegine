// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_error_format.h                                                -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_ERROR_FORMAT_H
#define INCLUDED_ENGINE_CORE_ERROR_FORMAT_H

//@PURPOSE: Provide the text form of an [Error], for logs and [std::format].
//
//@CLASSES:
//  engine::core::error::ErrorFormat: utility writing an [Error] as a line
//  std::formatter<ErrorDescriptor>: [std::format] support for a descriptor
//  std::formatter<Error>: [std::format] support for an error
//
//@SEE_ALSO: core_error, core_error_catalog, core_format
//
//@DESCRIPTION: This component writes an [Error] as one line of text:
// ```
//  vulkan | unknown | unknown | ctx=0x000000cd
//  vulkan | unknown | unknown | ctx=0x000000cd <=> raw=0x000000cd00000002
// ```
// The domain, kind and reason are named (["?"] for a value no enumerator
// has), the context is always eight hex digits, and the raw value, when
// asked for, lets a corrupt error be decoded by hand. [ErrorFormat] writes
// the line through a [LineWriter] into a buffer the caller owns, which is
// the logger's path and never allocates. The two [std::formatter]
// specializations give the same text to [std::format]: [{}] for the short
// form, [{:#}] for the one ending with the raw value.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Printing an Error
/// - - - - - - - - - - - - - -
// ```
//  const std::string text = std::format("create failed: {}", error);
//  // "create failed: vulkan | unknown | unknown | ctx=0x000000cd"
// ```

// engine
#include "engine/core/core_error.h"
#include "engine/core/core_error_catalog.h"
#include "engine/core/core_format.h"

// std
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string_view>

namespace engine::core {
namespace error {

// ==================
// struct ErrorFormat
// ==================

/// This utility [struct] writes an [Error] as a line of text; see the
/// component documentation for the layout.
struct ErrorFormat {
    // TYPES

    /// Enumeration used to choose whether a line ends with the raw 64-bit
    /// value of the error.
    enum class RawValue : std::uint8_t {
        e_OMIT,    // the line ends with the context
        e_APPEND,  // the raw value follows the context
    };

    // PUBLIC CLASS DATA

    /// Separator between the fields of a line.
    static constexpr std::string_view k_SEPARATOR = " | ";

    /// Maximum line size (in bytes) that [append_error] can produce. The
    /// fixed parts take 50 (three separators, the context, the raw value),
    /// which leaves 206 for the three names.
    static constexpr std::size_t k_MAX_LINE_SIZE = 256;

    // CLASS METHODS

    /// Append the three names of the specified [descriptor], separated by
    /// [k_SEPARATOR], to the specified [writer].
    static void append_descriptor(format::LineWriter&    writer,
                                  const ErrorDescriptor& descriptor) noexcept;

    /// Append the line of the specified [error] to the specified [writer],
    /// ending with the raw value if the specified [raw] is
    /// [RawValue::e_APPEND].
    static void append_error(format::LineWriter& writer,
                             const Error         error,
                             const RawValue      raw) noexcept;

    /// Write the line of the specified [error] into the specified [buffer],
    /// ending with the raw value if the specified [raw] is
    /// [RawValue::e_APPEND], and return the number of characters written.
    /// No terminating NUL is written. A [buffer] of [k_MAX_LINE_SIZE]
    /// characters always suffices; a shorter one cuts the line.
    [[nodiscard]] static std::size_t format_line(const std::span<char> buffer,
                                                 const Error           error,
                                                 const RawValue raw) noexcept;
};

}  // close namespace error
}  // close namespace engine::core

// ======================================
// struct std::formatter<ErrorDescriptor>
// ======================================

/// This [std::formatter] specialization writes an [ErrorDescriptor] as its
/// three names, [domain | kind | reason]. It accepts [{}] only.
template <>
struct std::formatter<engine::core::error::ErrorDescriptor, char>
: engine::core::format::EmptySpecParser {
    // CREATORS

    /// Create a [formatter] object.
    //! formatter() = default;

    /// Create a [formatter] object having the same value as the specified
    /// [original] object.
    //! formatter(const formatter& original) = default;

    /// Destroy this object.
    //! ~formatter() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! formatter& operator=(const formatter& rhs) = default;

    // ACCESSORS

    /// Write the three names of the specified [descriptor] to the output of
    /// the specified [context], and return the iterator past the last
    /// character written.
    template <class t_CONTEXT>
    t_CONTEXT::iterator
    format(const engine::core::error::ErrorDescriptor& descriptor,
           t_CONTEXT&                                  context) const;
};

// ============================
// struct std::formatter<Error>
// ============================

/// This [std::formatter] specialization writes an [Error] as its line. It
/// accepts [{}], and [{:#}] to end the line with the raw value.
template <>
struct std::formatter<engine::core::error::Error, char>
: engine::core::format::AlternateFlagParser {
    // CREATORS

    /// Create a [formatter] object, with the alternate flag unset.
    //! formatter() = default;

    /// Create a [formatter] object having the same value as the specified
    /// [original] object.
    //! formatter(const formatter& original) = default;

    /// Destroy this object.
    //! ~formatter() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! formatter& operator=(const formatter& rhs) = default;

    // ACCESSORS

    /// Write the line of the specified [error] to the output of the specified
    /// [context], ending with the raw value if the specification was [{:#}],
    /// and return the iterator past the last character written.
    template <class t_CONTEXT>
    t_CONTEXT::iterator format(const engine::core::error::Error error,
                               t_CONTEXT&                       context) const;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

namespace engine::core {
namespace error {

// ------------------
// struct ErrorFormat
// ------------------

// CLASS METHODS

inline void
ErrorFormat::append_descriptor(format::LineWriter&    writer,
                               const ErrorDescriptor& descriptor) noexcept
{
    writer.append(descriptor.domain());
    writer.append(k_SEPARATOR);
    writer.append(descriptor.kind());
    writer.append(k_SEPARATOR);
    writer.append(descriptor.reason());
}

inline void ErrorFormat::append_error(format::LineWriter& writer,
                                      const Error         error,
                                      const RawValue      raw) noexcept
{
    // Two hex digits per byte of the 32-bit context and the 64-bit raw value.
    constexpr std::size_t k_CONTEXT_DIGITS = 2 * sizeof(std::uint32_t);
    constexpr std::size_t k_RAW_DIGITS     = 2 * sizeof(std::uint64_t);

    append_descriptor(writer, ErrorDescriptor::make(error));

    writer.append(k_SEPARATOR);
    writer.append("ctx=0x");
    writer.append_hex<k_CONTEXT_DIGITS>(error.context());
    if (raw == RawValue::e_APPEND) {
        writer.append(" <=> raw=0x");
        writer.append_hex<k_RAW_DIGITS>(error.raw());
    }
}

inline std::size_t ErrorFormat::format_line(const std::span<char> buffer,
                                            const Error           error,
                                            const RawValue        raw) noexcept
{
    format::LineWriter writer(buffer);
    append_error(writer, error, raw);
    return writer.size();
}

}  // close namespace error
}  // close namespace engine::core

// --------------------------------------
// struct std::formatter<ErrorDescriptor>
// --------------------------------------

// ACCESSORS

template <class t_CONTEXT>
inline t_CONTEXT::iterator
std::formatter<engine::core::error::ErrorDescriptor, char>::format(
    const engine::core::error::ErrorDescriptor& descriptor,
    t_CONTEXT&                                  context) const
{
    std::array<char, engine::core::error::ErrorFormat::k_MAX_LINE_SIZE>
                                     buffer{};
    engine::core::format::LineWriter writer(buffer);
    engine::core::error::ErrorFormat::append_descriptor(writer, descriptor);

    return std::ranges::copy(writer.view(), context.out()).out;
}

// ----------------------------
// struct std::formatter<Error>
// ----------------------------

// ACCESSORS

template <class t_CONTEXT>
inline t_CONTEXT::iterator
std::formatter<engine::core::error::Error, char>::format(
    const engine::core::error::Error error, t_CONTEXT& context) const
{
    using engine::core::error::ErrorFormat;

    std::array<char, ErrorFormat::k_MAX_LINE_SIZE> buffer{};
    engine::core::format::LineWriter               writer(buffer);
    ErrorFormat::append_error(writer,
                              error,
                              is_alternate() ? ErrorFormat::RawValue::e_APPEND
                                             : ErrorFormat::RawValue::e_OMIT);
    return std::ranges::copy(writer.view(), context.out()).out;
}

#endif
