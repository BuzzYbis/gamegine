// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_message_format.h                                          -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_LOG_MESSAGE_FORMAT_H
#define INCLUDED_ENGINE_CORE_LOG_MESSAGE_FORMAT_H

//@PURPOSE: Provide the text form of a log message, for logs and [std::format].
//
//@CLASSES:
//  engine::core::log::LogMessageFormat: utility writing a message as a line
//  std::formatter<LogMessage>: [std::format] support for a log message
//
//@SEE_ALSO: core_log_message, core_log_message_catalog, core_format
//
//@DESCRIPTION: This component writes a [LogMessage] as one line of text: the
// name of its domain, then its text, with the argument where the text has
// [{}], shown as its message declares. For a [core] message with id 1, the
// text [streamed {} pages] and an unsigned argument, made with 412:
// ```
//  core | streamed 412 pages
//  core | streamed 412 pages <=> raw=0x0000019c00010001
// ```
// A domain or a message no enumerator has reads ["?"], and the raw value,
// when asked for, lets a corrupt message be decoded by hand. The separator
// and the raw value are written as in an error line, so both kinds of record
// read alike. [LogMessageFormat] writes the line through a [LineWriter] into a
// buffer the caller owns, which is the logger's path and never allocates. The
// [std::formatter] specialization gives the same text to [std::format]: [{}]
// for the short form, [{:#}] for the one ending with the raw value.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Printing a Log Message
///- - - - - - - - - - - - - - - - -
// ```
//  const std::string text = std::format(
//      "logged: {}", LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN));
//  // "logged: core | unknown"
// ```

// engine
#include "engine/core/core_format.h"
#include "engine/core/core_log_message.h"
#include "engine/core/core_log_message_catalog.h"

// std
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string_view>

namespace engine::core {
namespace log {

                         // =======================
                         // struct LogMessageFormat
                         // =======================

/// This utility [struct] writes a [LogMessage] as a line of text; see the
/// component documentation for the layout.
struct LogMessageFormat {
    // TYPES

    /// Enumeration used to choose whether a line ends with the raw 64-bit
    /// value of the message.
    enum class RawValue : std::uint8_t {
        e_OMIT,    // the line ends with the text
        e_APPEND,  // the raw value follows the text
    };

    // PUBLIC CLASS DATA

    /// Separator between the name of the domain and the text, as in an error
    /// line.
    static constexpr std::string_view k_SEPARATOR = " | ";

    /// Maximum line size (in bytes) that [append_message] can produce. The
    /// fixed parts take 45 (the separator, the widest argument, the raw
    /// value), which leaves 211 for the name of the domain and the text.
    static constexpr std::size_t k_MAX_LINE_SIZE = 256;

    // CLASS METHODS

    /// Append the specified [text] to the specified [writer], with the
    /// specified [argument] bits where it has [{}], written as
    /// [text.argument()] says.
    static void append_text(format::LineWriter&   writer,
                            const LogMessageText& text,
                            const std::uint32_t   argument) noexcept;

    /// Append the line of the specified [message] to the specified [writer],
    /// ending with the raw value if the specified [raw] is
    /// [RawValue::e_APPEND].
    static void append_message(format::LineWriter& writer,
                               const LogMessage    message,
                               const RawValue      raw) noexcept;

    /// Write the line of the specified [message] into the specified [buffer],
    /// ending with the raw value if the specified [raw] is
    /// [RawValue::e_APPEND], and return the number of characters written. No
    /// terminating NUL is written. A [buffer] of [k_MAX_LINE_SIZE] characters
    /// always suffices; a shorter one cuts the line.
    [[nodiscard]] static std::size_t format_line(const std::span<char> buffer,
                                                 const LogMessage      message,
                                                 const RawValue raw) noexcept;
};

}  // close namespace log
}  // close namespace engine::core

                     // =================================
                     // struct std::formatter<LogMessage>
                     // =================================

/// This [std::formatter] specialization writes a [LogMessage] as its line. It
/// accepts [{}], and [{:#}] to end the line with the raw value.
template <>
struct std::formatter<engine::core::log::LogMessage, char>
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

    /// Write the line of the specified [message] to the output of the
    /// specified [context], ending with the raw value if the specification
    /// was [{:#}], and return the iterator past the last character written.
    template <class t_CONTEXT>
    t_CONTEXT::iterator format(const engine::core::log::LogMessage message,
                               t_CONTEXT& context) const;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

namespace engine::core {
namespace log {

                         // -----------------------
                         // struct LogMessageFormat
                         // -----------------------

// CLASS METHODS

inline void
LogMessageFormat::append_text(format::LineWriter&   writer,
                              const LogMessageText& text,
                              const std::uint32_t   argument) noexcept
{
    // Two hex digits per byte of the 32-bit argument.
    constexpr std::size_t k_HEX_DIGITS = 2 * sizeof(std::uint32_t);

    writer.append(text.prefix());
    switch (text.argument()) {
    case LogMessage::Argument::e_NONE: break;
    case LogMessage::Argument::e_UNSIGNED:
        writer.append_decimal(argument);
        break;
    case LogMessage::Argument::e_SIGNED:
        writer.append_decimal(std::bit_cast<std::int32_t>(argument));
        break;
    case LogMessage::Argument::e_HEX:
        writer.append("0x");
        writer.append_hex<k_HEX_DIGITS>(argument);
        break;
    case LogMessage::Argument::e_FLOAT:
        writer.append_float(std::bit_cast<float>(argument));
        break;
    case LogMessage::Argument::e_COUNT:
        // A text with this argument does not compile; if one ever did, it is
        // shown, not hidden.
        writer.append_char('?');
        break;
    }
    writer.append(text.suffix());
}

inline void LogMessageFormat::append_message(format::LineWriter& writer,
                                             const LogMessage    message,
                                             const RawValue      raw) noexcept
{
    // Two hex digits per byte of the 64-bit raw value.
    constexpr std::size_t k_RAW_DIGITS = 2 * sizeof(std::uint64_t);

    writer.append(LogMessageCatalog::to_string_domain(message.domain()));
    writer.append(k_SEPARATOR);
    append_text(writer,
                LogMessageCatalog::text_of(message),
                message.argument());
    if (raw == RawValue::e_APPEND) {
        writer.append(" <=> raw=0x");
        writer.append_hex<k_RAW_DIGITS>(message.raw());
    }
}

inline std::size_t LogMessageFormat::format_line(const std::span<char> buffer,
                                                 const LogMessage      message,
                                                 const RawValue raw) noexcept
{
    format::LineWriter writer(buffer);
    append_message(writer, message, raw);
    return writer.size();
}

}  // close namespace log
}  // close namespace engine::core

                     // ---------------------------------
                     // struct std::formatter<LogMessage>
                     // ---------------------------------

// ACCESSORS

template <class t_CONTEXT>
inline t_CONTEXT::iterator
std::formatter<engine::core::log::LogMessage, char>::format(
    const engine::core::log::LogMessage message, t_CONTEXT& context) const
{
    using engine::core::log::LogMessageFormat;

    std::array<char, LogMessageFormat::k_MAX_LINE_SIZE> buffer{};
    engine::core::format::LineWriter                    writer(buffer);
    LogMessageFormat::append_message(writer,
                                     message,
                                     is_alternate()
                                         ? LogMessageFormat::RawValue::e_APPEND
                                         : LogMessageFormat::RawValue::e_OMIT);
    return std::ranges::copy(writer.view(), context.out()).out;
}

#endif
