// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_record.h                                                  -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_LOG_RECORD_H
#define INCLUDED_ENGINE_CORE_LOG_RECORD_H

//@PURPOSE: Provide the 16-byte record the logger moves between threads.
//
//@CLASSES:
//  engine::core::log::Severity: how much a record matters
//  engine::core::log::LogRecordType: what the payload of a record holds
//  engine::core::log::LogRecord: one slot of a producer's ring
//
//@SEE_ALSO: core_log, core_log_format, core_spsc_ring, core_error,
//           core_log_message
//
//@DESCRIPTION: A [LogRecord] is two 64-bit words, trivially copyable, so a
// push is a 16-byte copy and eight records share a 128-byte cache line:
// ```
//  header   [ type : 2 ][ severity : 3 ][ ticks : 59 ]
//  payload  [                   64 bits                ]
// ```
// The thread is not in the record: each thread has its own ring, and the ring
// says who wrote it. The ticks are those of [Clock::now()], cut to 59 bits,
// which last 761 years at Apple's 24 MHz and 18 years of uptime on Linux's
// nanosecond clock.
//
// The payload depends on the type:
// ```
//  e_ERROR    Error::raw()
//  e_MESSAGE  LogMessage::raw()
// ```
// Each type has its own factory, which sets the type and takes the payload as
// the type it is, never as a bare integer: a record whose type and payload
// disagree cannot be made, and neither can one from a number. A record is
// read back the same way, by the accessor for its type, which asserts that
// the record is of that type. Two values of the type field are free for a new
// kind of record.
//
// Text that exists only at run time (a validation layer message, an operating
// system error string, a path) is not carried by a record. A record is read
// up to a millisecond after it is made, and the text it would point to may be
// gone by then. A number travels instead, an [Error] context or a
// [LogMessage] argument, and the logger thread makes the text from it.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Making a Record of an Error
/// - - - - - - - - - - - - - - - - - - -
// ```
//  const LogRecord record =
//      LogRecord::make(Severity::e_WARN, Clock::now(), error);
//  // record.type() == LogRecordType::e_ERROR, record.error() == error
// ```
//
///Example 2: Making a Record from a Number
/// - - - - - - - - - - - - - - - - - - - -
// A record carries an [Error] or a [LogMessage], nothing else, so this does
// not compile:
// ```
//  const LogRecord record = LogRecord::make(Severity::e_WARN, ticks, 42U);
// ```

// engine
#include "engine/core/core_error.h"
#include "engine/core/core_log_message.h"

// std
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace engine::core {
namespace log {

/// Enumeration used to identify how much a record matters. Fits in 3 bits.
enum class Severity : std::uint8_t {
    e_TRACE = 0,
    e_DEBUG,
    e_INFO,
    e_WARN,
    e_ERROR,
    e_FATAL,

    e_COUNT,  // number of severities: always last, never logged
};

/// Enumeration used to identify what the payload of a record holds. Fits in
/// 2 bits.
enum class LogRecordType : std::uint8_t {
    e_ERROR = 0,  // an [Error::raw()]
    e_MESSAGE,    // a [LogMessage::raw()]

    e_COUNT,  // number of types: always last, never pushed
};

                         // ===============
                         // class LogRecord
                         // ===============

/// This value-semantic type is one slot of a producer's ring: a type, a
/// severity, the tick it was made at, and a 64-bit payload that only the
/// factory for the type can set. It is trivially copyable, so a push is a
/// 16-byte copy.
class LogRecord final {
  public:
    // PUBLIC CLASS DATA

    /// Number of bits of the header that hold the ticks; the higher bits of a
    /// tick are lost.
    static constexpr std::uint32_t k_TICK_BITS = 59;

  private:
    // CLASS DATA

    /// Position in the header of the lowest bit of the severity (3 bits).
    static constexpr std::uint32_t k_SEVERITY_SHIFT = k_TICK_BITS;

    /// Position in the header of the lowest bit of the type (2 bits).
    static constexpr std::uint32_t k_TYPE_SHIFT = k_TICK_BITS + 3;

    /// Mask of the bits of the header that hold the ticks.
    static constexpr std::uint64_t k_TICK_MASK = (1ULL << k_TICK_BITS) - 1;

    static_assert(std::to_underlying(Severity::e_COUNT) <= 8,
                  "severity SHALL fit inside 3 bits.");
    static_assert(std::to_underlying(LogRecordType::e_COUNT) <= 4,
                  "log type SHALL fit inside 2 bits");

    // DATA

    std::uint64_t d_header;   // type, severity and ticks, packed
    std::uint64_t d_payload;  // raw value of the error or of the message

    // PRIVATE CLASS METHODS

    /// Return a record of the specified [type] and [severity], stamped with
    /// the low [k_TICK_BITS] bits of the specified [ticks], carrying the
    /// specified [payload]. Every factory ends here: this is the one place a
    /// bare integer becomes a payload, and it cannot be reached from outside.
    [[nodiscard]] static constexpr LogRecord
    make_raw(const LogRecordType type,
             const Severity      severity,
             const std::uint64_t ticks,
             const std::uint64_t payload) noexcept;

    // PRIVATE CREATORS

    /// Create a [LogRecord] object holding the specified [header] and
    /// [payload] words.
    constexpr LogRecord(const std::uint64_t header,
                        const std::uint64_t payload) noexcept;

  public:
    // CLASS METHODS

    /// Return a record of type [e_ERROR] and the specified [severity],
    /// stamped with the low [k_TICK_BITS] bits of the specified [ticks],
    /// carrying the specified [error].
    [[nodiscard]] static constexpr LogRecord
    make(const Severity      severity,
         const std::uint64_t ticks,
         const error::Error  error) noexcept;

    /// Return a record of type [e_MESSAGE] and the specified [severity],
    /// stamped with the low [k_TICK_BITS] bits of the specified [ticks],
    /// carrying the specified [message].
    [[nodiscard]] static constexpr LogRecord
    make(const Severity      severity,
         const std::uint64_t ticks,
         const LogMessage    message) noexcept;

    /// A record carries an [error::Error] or a [LogMessage], never anything
    /// else: a bare integer does not compile. An exact [error::Error] or
    /// [LogMessage] prefers the overloads above, which are not templates.
    template <class t_OTHER>
    static constexpr LogRecord make(Severity, std::uint64_t, t_OTHER) = delete;

    // CREATORS

    /// Create a [LogRecord] object with an unspecified value, to be
    /// overwritten. Trivial, so a ring slot costs nothing to declare.
    LogRecord() noexcept = default;

    /// Create a [LogRecord] object having the same value as the specified
    /// [original] object.
    //! LogRecord(const LogRecord& original) = default;

    /// Destroy this object.
    //! ~LogRecord() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! LogRecord& operator=(const LogRecord& rhs) = default;

    // ACCESSORS

    /// Return the type of this record.
    [[nodiscard]] constexpr LogRecordType type() const noexcept;

    /// Return the severity of this record.
    [[nodiscard]] constexpr Severity severity() const noexcept;

    /// Return the tick this record was made at, modulo 2^[k_TICK_BITS].
    [[nodiscard]] constexpr std::uint64_t ticks() const noexcept;

    /// Return the error this record carries. The behavior is undefined unless
    /// [type() == LogRecordType::e_ERROR].
    [[nodiscard]] constexpr error::Error error() const noexcept;

    /// Return the message this record carries. The behavior is undefined
    /// unless [type() == LogRecordType::e_MESSAGE].
    [[nodiscard]] constexpr LogMessage message() const noexcept;
};

// A record is two words, copied as bytes into the ring and back out.
static_assert(sizeof(LogRecord) == 16);
static_assert(std::is_trivially_copyable_v<LogRecord>);
static_assert(std::is_trivially_default_constructible_v<LogRecord>);

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

                         // ---------------
                         // class LogRecord
                         // ---------------

// PRIVATE CLASS METHODS

inline constexpr LogRecord
LogRecord::make_raw(const LogRecordType type,
                    const Severity      severity,
                    const std::uint64_t ticks,
                    const std::uint64_t payload) noexcept
{
    const std::uint64_t header = (std::uint64_t{std::to_underlying(type)}
                                  << k_TYPE_SHIFT) |
                                 (std::uint64_t{std::to_underlying(severity)}
                                  << k_SEVERITY_SHIFT) |
                                 (ticks & k_TICK_MASK);
    return LogRecord{header, payload};
}

// PRIVATE CREATORS

inline constexpr LogRecord::LogRecord(const std::uint64_t header,
                                      const std::uint64_t payload) noexcept
: d_header(header)
, d_payload(payload)
{
}

// CLASS METHODS

inline constexpr LogRecord LogRecord::make(const Severity      severity,
                                           const std::uint64_t ticks,
                                           const error::Error  error) noexcept
{
    return make_raw(LogRecordType::e_ERROR, severity, ticks, error.raw());
}

inline constexpr LogRecord LogRecord::make(const Severity      severity,
                                           const std::uint64_t ticks,
                                           const LogMessage message) noexcept
{
    return make_raw(LogRecordType::e_MESSAGE, severity, ticks, message.raw());
}

// ACCESSORS

inline constexpr LogRecordType LogRecord::type() const noexcept
{
    return static_cast<LogRecordType>(d_header >> k_TYPE_SHIFT);
}

inline constexpr Severity LogRecord::severity() const noexcept
{
    return static_cast<Severity>((d_header >> k_SEVERITY_SHIFT) & 0x7U);
}

inline constexpr std::uint64_t LogRecord::ticks() const noexcept
{
    return d_header & k_TICK_MASK;
}

inline constexpr error::Error LogRecord::error() const noexcept
{
    assert(type() == LogRecordType::e_ERROR);

    return error::Error::from_raw(d_payload);
}

inline constexpr LogMessage LogRecord::message() const noexcept
{
    assert(type() == LogRecordType::e_MESSAGE);

    return LogMessage::from_raw(d_payload);
}

}  // close namespace log
}  // close namespace engine::core

#endif
