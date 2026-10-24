// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_format.h                                                  -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_LOG_FORMAT_H
#define INCLUDED_ENGINE_CORE_LOG_FORMAT_H

//@PURPOSE: Format log records into complete log lines.
//
//@CLASSES:
//  engine::core::log::LogFormat: utility operations writing a log record
//
//@SEE_ALSO: core_log, core_log_record, core_error_format,
//           core_log_message_format, core_format
//
//@DESCRIPTION: This component provides [LogFormat], which writes one record
// as one line:
// ```
//  [   12.345678] WARN  render   core | unknown | unknown | ctx=0x000000cd
//  [   12.346102] INFO  stream   vulkan | unknown
// ```
// The timestamp is seconds since the logger started, with microseconds,
// computed in integers; the seconds field is four characters wide and simply
// grows past 9999 s. The severity and thread columns have fixed widths, and a
// longer thread name is cut, so one long name cannot shift every column. The
// prefix is 28 characters while the seconds are below 10000, and the message
// starts a space after it, in column 29, on every line.
//
// A record is never more than one line, so a line starting with ['['] always
// begins a record. A record that does not fit in the writer is cut, and the
// writer says so.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Writing a Record
///- - - - - - - - - - - - - -
// ```
//  std::array<char, LogFormat::k_MAX_LINE_SIZE> buffer;
//  LineWriter writer(buffer);
//  LogFormat::append_record(writer, record, "render", clock);
//  // writer.view() is one line, ending with a newline
// ```

// engine
#include "engine/core/core_clock.h"
#include "engine/core/core_error_format.h"
#include "engine/core/core_format.h"
#include "engine/core/core_log_message_format.h"
#include "engine/core/core_log_record.h"

// std
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace engine::core {
namespace log {

/// This [struct] provides a namespace for utility operations that write a
/// complete log record as text.
struct LogFormat final {
    // PUBLIC CLASS DATA

    /// Width of the severity column: the longest names (TRACE, DEBUG, ERROR,
    /// FATAL) are 5 characters.
    static constexpr std::size_t k_SEVERITY_WIDTH = 5;

    /// Width of the thread column; a longer name is cut.
    static constexpr std::size_t k_THREAD_WIDTH = 8;

    /// Longest record: an error line (256) plus the prefix, with room to
    /// spare.
    static constexpr std::size_t k_MAX_LINE_SIZE = 512;

    // CLASS METHODS

    /// Return the name of the specified [severity], or ["?"] for a value no
    /// enumerator names.
    [[nodiscard]] static constexpr std::string_view
    severity_name(const Severity severity) noexcept;

    /// Append the time of the specified [ticks], measured from the start of
    /// the specified [clock], to the specified [writer]: seconds and
    /// microseconds between square brackets, as in ["[   12.345678]"].
    static void append_timestamp(format::LineWriter&        writer,
                                 const std::uint64_t        ticks,
                                 const tic::TickConversion& clock) noexcept;

    /// Append the timestamp of the specified [ticks] converted with the
    /// specified [clock], the name of the specified [severity], and the
    /// specified [thread] name, each in its column and a space apart, to the
    /// specified [writer]: everything a line holds before its message, which
    /// starts a space later.
    static void append_prefix(format::LineWriter&        writer,
                              const std::uint64_t        ticks,
                              const Severity             severity,
                              const std::string_view     thread,
                              const tic::TickConversion& clock) noexcept;

    /// Append one complete record to the specified [writer], ending with a
    /// newline: the prefix of the specified [record] converted with the
    /// specified [clock] and labelled with the specified [thread], a space,
    /// then its error through [ErrorFormat::append_error] or its message
    /// through [LogMessageFormat::append_message], according to its type. A
    /// type no factory makes is written as ['?'], and the line is complete
    /// all the same. A record that does not fit is cut, and
    /// [writer.is_truncated()] says so. This is the one place a type read at
    /// run time becomes a choice: the logger thread pops mixed records.
    static void append_record(format::LineWriter&        writer,
                              const LogRecord&           record,
                              const std::string_view     thread,
                              const tic::TickConversion& clock) noexcept;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

                         // ----------------
                         // struct LogFormat
                         // ----------------

// CLASS METHODS

inline constexpr std::string_view
LogFormat::severity_name(const Severity severity) noexcept
{
    constexpr std::array<std::string_view,
                         static_cast<std::size_t>(Severity::e_COUNT)>
        k_NAMES = {
            "TRACE",
            "DEBUG",
            "INFO",
            "WARN",
            "ERROR",
            "FATAL",
    };

    // [append_prefix] pads a severity name and never cuts it, so a longer name
    // would push every later column to the right.
    constexpr auto fits_column = [](std::string_view name) {
        return name.size() <= k_SEVERITY_WIDTH;
    };
    static_assert(std::ranges::all_of(k_NAMES, fits_column));

    const auto index = static_cast<std::size_t>(std::to_underlying(severity));
    return index < k_NAMES.size() ? k_NAMES[index] : std::string_view("?");
}

inline void
LogFormat::append_timestamp(format::LineWriter&        writer,
                            const std::uint64_t        ticks,
                            const tic::TickConversion& clock) noexcept
{
    const std::uint64_t timestamp = clock.ns_elapsed_since_start(ticks);
    writer.append_char('[');
    writer.append_decimal(timestamp / 1'000'000'000U, 4);
    writer.append_char('.');
    writer.append_decimal(timestamp % 1'000'000'000U / 1'000U, 6, '0');
    writer.append_char(']');
}

inline void LogFormat::append_prefix(format::LineWriter&        writer,
                                     const std::uint64_t        ticks,
                                     const Severity             severity,
                                     const std::string_view     thread,
                                     const tic::TickConversion& clock) noexcept
{
    append_timestamp(writer, ticks, clock);
    writer.append_char(' ');
    writer.append_padded(severity_name(severity), k_SEVERITY_WIDTH);
    writer.append_char(' ');
    writer.append_fitted(thread, k_THREAD_WIDTH);
}

inline void LogFormat::append_record(format::LineWriter&        writer,
                                     const LogRecord&           record,
                                     const std::string_view     thread,
                                     const tic::TickConversion& clock) noexcept
{
    // Every line is its prefix, a space, a payload and a newline; only the
    // payload depends on the type, so it is the one thing each case supplies.
    const auto line = [&](const auto& append_payload) {
        append_prefix(
            writer, record.ticks(), record.severity(), thread, clock);
        writer.append_char(' ');
        append_payload();
        writer.append_char('\n');
    };

    switch (record.type()) {
    case LogRecordType::e_ERROR:
        line([&] {
            error::ErrorFormat::append_error(
                writer, record.error(), error::ErrorFormat::RawValue::e_OMIT);
        });
        break;
    case LogRecordType::e_MESSAGE:
        line([&] {
            LogMessageFormat::append_message(
                writer, record.message(), LogMessageFormat::RawValue::e_OMIT);
        });
        break;
    case LogRecordType::e_COUNT:
    default:
        // No factory makes another type, so none reaches the ring; if one
        // ever did, it is shown, not hidden, on a line of its own.
        line([&] { writer.append_char('?'); });
        break;
    }
}

}  // close namespace log
}  // close namespace engine::core

#endif
