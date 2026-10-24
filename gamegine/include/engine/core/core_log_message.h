// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_message.h                                                 -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_LOG_MESSAGE_H
#define INCLUDED_ENGINE_CORE_LOG_MESSAGE_H

//@PURPOSE: Provide a 64-bit log message value, checked where it is made.
//
//@CLASSES:
//  engine::core::log::LogMessageIdTraits: domain and arguments of message ids
//  engine::core::log::LogMessage: 64-bit value saying what happened
//  engine::core::log::LogMessageId: message id checked against its argument
//
//@SEE_ALSO: core_domain, core_log_message_catalog, core_log_message_format
//
//@DESCRIPTION: This component provides [LogMessage], a message of the
// catalog and its argument in 64 bits, so that a thread logs it by pushing a
// number: its text is written later, on the logger thread (see
// [LogMessageFormat]). It is the counterpart of [Error] for everything else
// worth logging, with the same layout but no kind.
//
///Layout
///------
// ```
//  bits  0 -  7  domain    subsystem logging it          [Domain]
//  bits  8 - 15  unused    always 0: a message has no kind
//  bits 16 - 31  id        which message, in the domain  [CoreMessage], ...
//  bits 32 - 63  argument  the number its text shows
// ```
// Each message enumeration belongs to one domain, and each of its messages
// declares what its argument holds: nothing, an unsigned or a signed 32-bit
// number, or a float (see [LogMessage::Argument]). Both are set by the
// enumeration's [LogMessageIdTraits] specialization.
//
///Checked Arguments
///-----------------
// An error's context is shown in hex whatever it holds, so any context suits
// any reason. A message's argument is shown as its message declares, so
// [LogMessage::make] checks it when the call is compiled: the id converts to
// a [LogMessageId] for the type of the argument, and that conversion does not
// compile for a message declaring another argument -- the check
// [std::format] makes of a format string. The id must therefore be a
// constant, which an enumerator is.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Passing a Message to the Logger Thread
///- - - - - - - - - - - - - - - - - - - - - - - - -
// A thread makes a message and pushes its [raw()] value; the logger thread
// pops the value and reads the message back:
// ```
//  const LogMessage message =
//      LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN);
//  const std::uint64_t pushed = message.raw();
//  ...
//  const LogMessage popped = LogMessage::from_raw(pushed);
//  // popped == message, popped.domain() == Domain::e_CORE
// ```
//
///Example 2: Making a Message with the Wrong Argument
///- - - - - - - - - - - - - - - - - - - - - - - - - -
// [e_UNKNOWN] declares no argument, so giving it one does not compile, and
// the error names [reject_argument]:
// ```
//  const LogMessage message =
//      LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN, 7U);
// ```

// engine
#include "engine/core/core_domain.h"

// std
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace engine::core {
namespace log {

                         // =========================
                         // struct LogMessageIdTraits
                         // =========================

/// This traits type associates a message enumeration with the domain it
/// belongs to, and each of its messages with the argument it takes: each
/// specialization defines [k_DOMAIN], and [k_ARGUMENTS], one
/// [LogMessage::Argument] per enumerator, in enumerator order. It must be
/// specialized for every message enumeration accepted by [LogMessage::make]
/// and [LogMessage::id_as].
template <class t_MESSAGE> struct LogMessageIdTraits;

/// This concept is satisfied by a message enumeration [LogMessage] accepts: a
/// scoped enumeration of [std::uint16_t] with a [LogMessageIdTraits]
/// specialization whose [k_DOMAIN] is a [Domain].
template <class t_MESSAGE>
concept LogMessageIdType = std::is_scoped_enum_v<t_MESSAGE> && requires {
    requires std::same_as<std::underlying_type_t<t_MESSAGE>, std::uint16_t>;
    { LogMessageIdTraits<t_MESSAGE>::k_DOMAIN } -> std::same_as<const Domain&>;
    LogMessageIdTraits<t_MESSAGE>::k_ARGUMENTS;
};

/// This concept is satisfied by the type of the argument a message is made
/// with: [void] for none, [std::uint32_t], [std::int32_t] or [float].
template <class t_ARGUMENT>
concept LogMessageArgumentType = std::is_void_v<t_ARGUMENT> ||
                                 std::same_as<t_ARGUMENT, std::uint32_t> ||
                                 std::same_as<t_ARGUMENT, std::int32_t> ||
                                 std::same_as<t_ARGUMENT, float>;

// Declared here for [LogMessage::make], and defined after [LogMessage], whose
// types it holds.
template <LogMessageArgumentType t_ARGUMENT> class LogMessageId;

                         // ================
                         // class LogMessage
                         // ================

/// This value-semantic type packs a log message into 64 bits: a domain, a
/// domain-specific id and a 32-bit argument (see the component documentation
/// for the layout). It is trivially copyable, and two messages have the same
/// value if their [raw()] values are equal. Discarding one is a compile error
/// under [-Werror]: a message is made to be logged.
class [[nodiscard("A log message shall not be discarded")]] LogMessage final {
  public:
    // TYPES

    /// [Domain] is an alias for [engine::core::Domain], the subsystem logging
    /// the message; errors share it.
    using Domain = ::engine::core::Domain;

    /// Enumeration used to declare what the argument of a message holds, and
    /// so how its text shows it.
    enum class Argument : std::uint8_t {
        e_NONE = 0,  // no argument: the text has no [{}]
        e_UNSIGNED,  // a [std::uint32_t], shown in decimal
        e_SIGNED,    // a [std::int32_t], shown in decimal
        e_HEX,       // a [std::uint32_t], shown as [0x] and 8 hex digits
        e_FLOAT,     // a [float], shown in its shortest exact form

        e_COUNT,  // number of arguments: always last, never declared
    };

    /// Enumeration used to identify a message of the [core] subsystem.
    enum class CoreMessage : std::uint16_t {
        e_UNKNOWN = 0,  // no more precise message

        e_COUNT,  // number of messages: always last, never logged
    };

    /// Enumeration used to identify a message of the [vulkan] subsystem.
    enum class VulkanMessage : std::uint16_t {
        e_UNKNOWN = 0,  // no more precise message

        e_COUNT,  // number of messages: always last, never logged
    };

    /// Enumeration used to identify a message of the [metal] subsystem.
    enum class MetalMessage : std::uint16_t {
        e_UNKNOWN = 0,  // no more precise message

        e_COUNT,  // number of messages: always last, never logged
    };

  private:
    // DATA

    std::uint64_t d_raw;  // domain, id and argument, packed

    // PRIVATE CLASS METHODS

    /// Return the message of the specified [domain] and [id], with the
    /// specified [argument] bits.
    static constexpr LogMessage pack(const Domain        domain,
                                     const std::uint16_t id,
                                     const std::uint32_t argument) noexcept;

    // PRIVATE CREATORS

    /// Create a [LogMessage] object holding the specified [raw] value.
    explicit constexpr LogMessage(const std::uint64_t raw) noexcept;

  public:
    // CLASS METHODS

    /// Return the message identified by the specified [id], which takes no
    /// argument. An [id] whose message declares one does not compile.
    static constexpr LogMessage make(const LogMessageId<void> id) noexcept;

    /// Return the message identified by the specified [id], with the
    /// specified [argument]. An [id] whose message declares neither
    /// [e_UNSIGNED] nor [e_HEX] does not compile.
    static constexpr LogMessage make(const LogMessageId<std::uint32_t> id,
                                     const std::uint32_t argument) noexcept;

    /// Return the message identified by the specified [id], with the
    /// specified [argument], kept bit for bit so that [signed_argument()]
    /// returns it. An [id] whose message does not declare [e_SIGNED] does not
    /// compile.
    static constexpr LogMessage make(const LogMessageId<std::int32_t> id,
                                     const std::int32_t argument) noexcept;

    /// Return the message identified by the specified [id], with the
    /// specified [argument], kept bit for bit (NaN payload included) so that
    /// [float_argument()] returns it. An [id] whose message does not declare
    /// [e_FLOAT] does not compile.
    static constexpr LogMessage make(const LogMessageId<float> id,
                                     const float argument) noexcept;

    /// Return the message whose packed value is the specified [raw], the
    /// inverse of [raw()]. Note that any 64-bit value is accepted, even one
    /// whose parts no enumerator names, or whose argument its message does
    /// not declare: this reads a message back, it does not make one.
    static constexpr LogMessage from_raw(const std::uint64_t raw) noexcept;

    // CREATORS

    /// Create a [LogMessage] object having the same value as the specified
    /// [original] object.
    //! LogMessage(const LogMessage& original) = default;

    /// Destroy this object.
    //! ~LogMessage() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! LogMessage& operator=(const LogMessage& rhs) = default;

    // ACCESSORS

    /// Return the domain of this message.
    [[nodiscard]] constexpr Domain domain() const noexcept;

    /// Return the id of this message as a [t_MESSAGE]. The behavior is
    /// undefined unless [domain() == LogMessageIdTraits<t_MESSAGE>::k_DOMAIN].
    template <LogMessageIdType t_MESSAGE>
    [[nodiscard]] constexpr t_MESSAGE id_as() const noexcept;

    /// Return the id of this message as its 16-bit value, whatever its
    /// domain.
    [[nodiscard]] constexpr std::uint16_t id() const noexcept;

    /// Return the argument of this message as an unsigned 32-bit number.
    [[nodiscard]] constexpr std::uint32_t argument() const noexcept;

    /// Return the argument of this message read as a signed 32-bit number,
    /// the inverse of [make] with an [std::int32_t].
    [[nodiscard]] constexpr std::int32_t signed_argument() const noexcept;

    /// Return the argument of this message read as a float, the inverse of
    /// [make] with a [float].
    [[nodiscard]] constexpr float float_argument() const noexcept;

    /// Return the packed 64-bit value of this message, the inverse of
    /// [from_raw].
    [[nodiscard]] constexpr std::uint64_t raw() const noexcept;

    // HIDDEN FRIENDS

    /// Return [true] if the specified [lhs] and [rhs] messages have the same
    /// value, and [false] otherwise. Two messages have the same value if
    /// their [raw()] values are equal.
    friend constexpr bool operator==(LogMessage lhs,
                                     LogMessage rhs) noexcept = default;
};

                         // ==================
                         // class LogMessageId
                         // ==================

/// This class holds the id of a message, checked when it is compiled against
/// [t_ARGUMENT], the type of the argument the message is made with ([void]
/// for none). It converts implicitly, and only at compile time, from any
/// message enumerator, so that [LogMessage::make] takes the enumerator
/// itself; an enumerator whose message declares another argument does not
/// compile.
template <LogMessageArgumentType t_ARGUMENT> class LogMessageId final {
  private:
    // DATA

    LogMessage::Domain d_domain;  // domain of the message
    std::uint16_t      d_value;   // id of the message, in its domain

    // PRIVATE CLASS METHODS

    /// Stop a message made with the wrong argument at compile time: this
    /// function is not [constexpr], so reaching it in the [consteval]
    /// constructor fails the build, with an error naming it.
    [[noreturn]] static void reject_argument() noexcept;

  public:
    // CLASS METHODS

    /// Return [true] if a message declaring the specified [argument] can be
    /// made with a [t_ARGUMENT], and [false] otherwise: an [std::uint32_t]
    /// suits [e_UNSIGNED] and [e_HEX], an [std::int32_t] [e_SIGNED], a
    /// [float] [e_FLOAT], and no argument ([void]) [e_NONE].
    [[nodiscard]] static constexpr bool
    accepts(const LogMessage::Argument argument) noexcept;

    // CREATORS

    /// Create a [LogMessageId] object holding the specified [id]. An [id]
    /// whose declared argument [accepts] rejects, or that is no message at
    /// all (an [e_COUNT]), does not compile.
    // Implicit on purpose, as [std::format_string]'s is: this conversion is
    // the check, so clang-tidy's misc-explicit-constructor is suppressed here.
    template <LogMessageIdType t_MESSAGE>
    // NOLINTNEXTLINE(misc-explicit-constructor)
    consteval LogMessageId(const t_MESSAGE id) noexcept;            // IMPLICIT

    /// Create a [LogMessageId] object having the same value as the specified
    /// [original] object.
    //! LogMessageId(const LogMessageId& original) = default;

    /// Destroy this object.
    //! ~LogMessageId() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! LogMessageId& operator=(const LogMessageId& rhs) = default;

    // ACCESSORS

    /// Return the domain of the message.
    [[nodiscard]] constexpr LogMessage::Domain domain() const noexcept;

    /// Return the id of the message as its 16-bit value, in its domain.
    [[nodiscard]] constexpr std::uint16_t value() const noexcept;
};

                     // ----------------------------------
                     // LogMessageIdTraits specializations
                     // ----------------------------------

/// Associate [LogMessage::CoreMessage] with the [e_CORE] domain, and each of
/// its messages with the argument it takes.
template <> struct LogMessageIdTraits<LogMessage::CoreMessage> {
    static constexpr Domain k_DOMAIN = Domain::e_CORE;

    static constexpr auto k_ARGUMENTS = std::to_array<LogMessage::Argument>({
        LogMessage::Argument::e_NONE,  // e_UNKNOWN
    });
};

/// Associate [LogMessage::VulkanMessage] with the [e_VULKAN] domain, and each
/// of its messages with the argument it takes.
template <> struct LogMessageIdTraits<LogMessage::VulkanMessage> {
    static constexpr Domain k_DOMAIN = Domain::e_VULKAN;

    static constexpr auto k_ARGUMENTS = std::to_array<LogMessage::Argument>({
        LogMessage::Argument::e_NONE,  // e_UNKNOWN
    });
};

/// Associate [LogMessage::MetalMessage] with the [e_METAL] domain, and each
/// of its messages with the argument it takes.
template <> struct LogMessageIdTraits<LogMessage::MetalMessage> {
    static constexpr Domain k_DOMAIN = Domain::e_METAL;

    static constexpr auto k_ARGUMENTS = std::to_array<LogMessage::Argument>({
        LogMessage::Argument::e_NONE,  // e_UNKNOWN
    });
};

// One argument per message: a new message without its argument does not
// compile.
static_assert(
    LogMessageIdTraits<LogMessage::CoreMessage>::k_ARGUMENTS.size() ==
    std::to_underlying(LogMessage::CoreMessage::e_COUNT));
static_assert(
    LogMessageIdTraits<LogMessage::VulkanMessage>::k_ARGUMENTS.size() ==
    std::to_underlying(LogMessage::VulkanMessage::e_COUNT));
static_assert(
    LogMessageIdTraits<LogMessage::MetalMessage>::k_ARGUMENTS.size() ==
    std::to_underlying(LogMessage::MetalMessage::e_COUNT));

// A [LogMessage] is a plain 64-bit value: pushed by a copy, destroyed for
// free.
static_assert(sizeof(LogMessage) == sizeof(std::uint64_t));
static_assert(alignof(LogMessage) == alignof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<LogMessage>);
static_assert(std::is_trivially_destructible_v<LogMessage>);

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

// [LogMessageId] is defined first: [LogMessage::make] calls its members, and
// in a constant expression clang needs a member of a class template defined
// before the first call to it.

                         // ------------------
                         // class LogMessageId
                         // ------------------

// PRIVATE CLASS METHODS

template <LogMessageArgumentType t_ARGUMENT>
inline void LogMessageId<t_ARGUMENT>::reject_argument() noexcept
{
    std::abort();
}

// CLASS METHODS

template <LogMessageArgumentType t_ARGUMENT>
inline constexpr bool
LogMessageId<t_ARGUMENT>::accepts(const LogMessage::Argument argument) noexcept
{
    using Argument = LogMessage::Argument;

    if constexpr (std::is_void_v<t_ARGUMENT>) {
        return argument == Argument::e_NONE;
    } else if constexpr (std::same_as<t_ARGUMENT, std::uint32_t>) {
        // One type, shown two ways.
        return argument == Argument::e_UNSIGNED || argument == Argument::e_HEX;
    } else if constexpr (std::same_as<t_ARGUMENT, std::int32_t>) {
        return argument == Argument::e_SIGNED;
    } else {
        return argument == Argument::e_FLOAT;
    }
}

// CREATORS

template <LogMessageArgumentType t_ARGUMENT>
template <LogMessageIdType t_MESSAGE>
inline consteval LogMessageId<t_ARGUMENT>::LogMessageId(
    const t_MESSAGE id) noexcept
: d_domain(LogMessageIdTraits<t_MESSAGE>::k_DOMAIN)
, d_value(std::to_underlying(id))
{
    // A value past the declared arguments, such as [e_COUNT], is no message.
    const auto& arguments = LogMessageIdTraits<t_MESSAGE>::k_ARGUMENTS;
    if (d_value >= arguments.size() || !accepts(arguments[d_value])) {
        reject_argument();
    }
}

// ACCESSORS

template <LogMessageArgumentType t_ARGUMENT>
inline constexpr LogMessage::Domain
LogMessageId<t_ARGUMENT>::domain() const noexcept
{
    return d_domain;
}

template <LogMessageArgumentType t_ARGUMENT>
inline constexpr std::uint16_t LogMessageId<t_ARGUMENT>::value() const noexcept
{
    return d_value;
}

                         // ----------------
                         // class LogMessage
                         // ----------------

// PRIVATE CLASS METHODS

inline constexpr LogMessage
LogMessage::pack(const Domain        domain,
                 const std::uint16_t id,
                 const std::uint32_t argument) noexcept
{
    const std::uint64_t raw_value = static_cast<std::uint64_t>(domain) |
                                    (static_cast<std::uint64_t>(id) << 16U) |
                                    (static_cast<std::uint64_t>(argument)
                                     << 32U);

    return LogMessage(raw_value);
}

// PRIVATE CREATORS

inline constexpr LogMessage::LogMessage(const std::uint64_t raw) noexcept
: d_raw(raw)
{
}

// CLASS METHODS

inline constexpr LogMessage
LogMessage::make(const LogMessageId<void> id) noexcept
{
    return pack(id.domain(), id.value(), 0);
}

inline constexpr LogMessage
LogMessage::make(const LogMessageId<std::uint32_t> id,
                 const std::uint32_t               argument) noexcept
{
    return pack(id.domain(), id.value(), argument);
}

inline constexpr LogMessage
LogMessage::make(const LogMessageId<std::int32_t> id,
                 const std::int32_t               argument) noexcept
{
    // [bit_cast] rather than a numeric conversion: the intent is to keep the
    // same 32 bits, and it is the exact inverse of [signed_argument()].
    return pack(id.domain(),
                id.value(),
                std::bit_cast<std::uint32_t>(argument));
}

inline constexpr LogMessage LogMessage::make(const LogMessageId<float> id,
                                             const float argument) noexcept
{
    // [bit_cast] keeps every bit, NaN payload included: the exact inverse of
    // [float_argument()].
    return pack(id.domain(),
                id.value(),
                std::bit_cast<std::uint32_t>(argument));
}

inline constexpr LogMessage
LogMessage::from_raw(const std::uint64_t raw) noexcept
{
    return LogMessage(raw);
}

// ACCESSORS

inline constexpr LogMessage::Domain LogMessage::domain() const noexcept
{
    return static_cast<Domain>(d_raw & 0xFFU);
}

template <LogMessageIdType t_MESSAGE>
inline constexpr t_MESSAGE LogMessage::id_as() const noexcept
{
    assert(domain() == LogMessageIdTraits<t_MESSAGE>::k_DOMAIN);

    return static_cast<t_MESSAGE>(id());
}

inline constexpr std::uint16_t LogMessage::id() const noexcept
{
    return static_cast<std::uint16_t>((d_raw >> 16U) & 0xFFFFU);
}

inline constexpr std::uint32_t LogMessage::argument() const noexcept
{
    return static_cast<std::uint32_t>(d_raw >> 32U);
}

inline constexpr std::int32_t LogMessage::signed_argument() const noexcept
{
    return std::bit_cast<std::int32_t>(argument());
}

inline constexpr float LogMessage::float_argument() const noexcept
{
    return std::bit_cast<float>(argument());
}

inline constexpr std::uint64_t LogMessage::raw() const noexcept
{
    return d_raw;
}

}  // close namespace log
}  // close namespace engine::core

#endif
