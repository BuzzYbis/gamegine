// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log_message_catalog.h                                         -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_LOG_MESSAGE_CATALOG_H
#define INCLUDED_ENGINE_CORE_LOG_MESSAGE_CATALOG_H

//@PURPOSE: Provide the text of each log message and the name of its domain.
//
//@CLASSES:
//  engine::core::log::LogMessageText: text of a message, split at its argument
//  engine::core::log::LogMessageTexts: texts of one message enumeration
//  engine::core::log::LogMessageCatalog: text lookup for each log message
//
//@SEE_ALSO: core_domain, core_log_message, core_log_message_format
//
//@DESCRIPTION: This component gives each [LogMessage] its text: one line of
// printable ASCII with at most one [{}], where the argument goes, shown as its
// message declares (see [LogMessage::Argument]). A text is split at its [{}]
// when it is compiled, so writing a message is three appends with nothing to
// parse; a text with a control character, a stray brace, or a [{}] that does
// not match its declared argument does not compile. Each text table holds one
// text per enumerator, in enumerator order, and a [static_assert] checks the
// count, so a new enumerator without its text does not compile either. The
// domain names are [k_DOMAIN_NAMES], shared with errors. A domain or a message
// that no enumerator has reads ["?"]: [LogMessage::from_raw] accepts any 64
// bits, so a corrupt message still reads.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Splitting a Text at Its Argument
///- - - - - - - - - - - - - - - - - - - - - -
// A message declaring an unsigned argument has one [{}] where it goes:
// ```
//  constexpr LogMessageText text("streamed {} pages",
//                                LogMessage::Argument::e_UNSIGNED);
//  // text.prefix() == "streamed ", text.suffix() == " pages"
// ```
//
///Example 2: Adding a Message
///- - - - - - - - - - - - - -
// A new [core] message is three lines, each at the same position: its
// enumerator in [LogMessage::CoreMessage], its argument in
// [LogMessageIdTraits<LogMessage::CoreMessage>::k_ARGUMENTS], and its text in
// the table below, which takes the argument from there:
// ```
//  static constexpr auto k_TEXTS =
//      LogMessageText::table<LogMessage::CoreMessage>({
//          "unknown",
//          "streamed {} pages",
//      });
// ```

// engine
#include "engine/core/core_domain.h"
#include "engine/core/core_log_message.h"

// std
#include <array>
#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace engine::core {
namespace log {

                         // ====================
                         // class LogMessageText
                         // ====================

/// This value-semantic class holds the text of one log message, split at the
/// [{}] where its argument goes, and the argument its message declares. A text
/// is made at compile time only, and one that [is_valid] rejects does not
/// compile; the default text reads ["?"], for a message no enumerator has.
class LogMessageText final {
  public:
    // PUBLIC CLASS DATA

    /// Where the argument goes in a text.
    static constexpr std::string_view k_PLACEHOLDER = "{}";

  private:
    // DATA

    std::string_view     d_prefix;    // text before the argument
    std::string_view     d_suffix;    // text after the argument
    LogMessage::Argument d_argument;  // what the argument holds

    // PRIVATE CLASS METHODS

    /// Stop a malformed text at compile time: this function is not
    /// [constexpr], so reaching it in a [consteval] function fails the build,
    /// with an error naming it.
    [[noreturn]] static void reject_text() noexcept;

  public:
    // CLASS METHODS

    /// Return [true] if the specified [text] suits a message declaring the
    /// specified [argument], and [false] otherwise: a non-empty line of
    /// printable ASCII, not starting or ending with a space, whose only braces
    /// are one [{}], there if and only if [argument] is not [e_NONE].
    [[nodiscard]] static constexpr bool
    is_valid(const std::string_view     text,
             const LogMessage::Argument argument) noexcept;

    /// Return the texts of the messages of [t_MESSAGE], made from the
    /// specified [texts], one per enumerator in enumerator order, each with
    /// the argument its message declares in [LogMessageIdTraits]. A text that
    /// does not suit its argument, or one more than there are messages, does
    /// not compile.
    template <LogMessageIdType t_MESSAGE, std::size_t t_SIZE>
    static consteval std::array<LogMessageText, t_SIZE>
    table(const std::string_view (&texts)[t_SIZE]) noexcept;

    // CREATORS

    /// Create a [LogMessageText] object reading ["?"], with no argument: the
    /// text of a message no enumerator has.
    constexpr LogMessageText() noexcept;

    /// Create a [LogMessageText] object from the specified [text], for a
    /// message declaring the specified [argument]. A [text] that [is_valid]
    /// rejects does not compile.
    consteval LogMessageText(const std::string_view     text,
                             const LogMessage::Argument argument) noexcept;

    /// Create a [LogMessageText] object having the same value as the
    /// specified [original] object.
    //! LogMessageText(const LogMessageText& original) = default;

    /// Destroy this object.
    //! ~LogMessageText() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! LogMessageText& operator=(const LogMessageText& rhs) = default;

    // ACCESSORS

    /// Return the text before the argument: the whole text if there is no
    /// argument.
    [[nodiscard]] constexpr std::string_view prefix() const noexcept;

    /// Return the text after the argument: empty if there is no argument.
    [[nodiscard]] constexpr std::string_view suffix() const noexcept;

    /// Return what the argument of the message holds.
    [[nodiscard]] constexpr LogMessage::Argument argument() const noexcept;

    // HIDDEN FRIENDS

    /// Return [true] if the specified [lhs] and [rhs] texts have the same
    /// value, and [false] otherwise. Two texts have the same value if their
    /// prefixes, suffixes and arguments are equal.
    friend constexpr bool
    operator==(const LogMessageText& lhs,
               const LogMessageText& rhs) noexcept = default;
};

// [LogMessageText] is defined here, not with the other definitions at the end
// of this header: the text tables below run its [consteval] functions while
// this header compiles, and a function has to be defined before it runs.

                         // --------------------
                         // class LogMessageText
                         // --------------------

// PRIVATE CLASS METHODS

inline void LogMessageText::reject_text() noexcept
{
    std::abort();
}

// CLASS METHODS

inline constexpr bool
LogMessageText::is_valid(const std::string_view     text,
                         const LogMessage::Argument argument) noexcept
{
    if (text.empty() || text.front() == ' ' || text.back() == ' ' ||
        argument >= LogMessage::Argument::e_COUNT) {
        return false;                                                 // RETURN
    }

    // One pass, since every file including the catalog checks every text
    // again: measured 2x (clang) to 5x (GCC) faster to compile than four
    // passes (a printable check, a placeholder search, two brace counts).
    std::size_t braces          = 0;
    bool        has_placeholder = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        // Printable ASCII: one line, and nothing a terminal would act on.
        const char c = text[i];
        if (c < ' ' || c > '~') {
            return false;                                             // RETURN
        }
        if (c == '{' || c == '}') {
            ++braces;
            has_placeholder = has_placeholder ||
                              text.substr(i).starts_with(k_PLACEHOLDER);
        }
    }

    // Braces only as the one placeholder, and the placeholder only where there
    // is an argument to put in it.
    return has_placeholder == (argument != LogMessage::Argument::e_NONE) &&
           braces == (has_placeholder ? 2U : 0U);
}

template <LogMessageIdType t_MESSAGE, std::size_t t_SIZE>
inline consteval std::array<LogMessageText, t_SIZE>
LogMessageText::table(const std::string_view (&texts)[t_SIZE]) noexcept
{
    // A text past the declared arguments has no message to belong to.
    const auto& arguments = LogMessageIdTraits<t_MESSAGE>::k_ARGUMENTS;
    if (t_SIZE > arguments.size()) {
        reject_text();
    }

    std::array<LogMessageText, t_SIZE> result;
    for (std::size_t i = 0; i < t_SIZE; ++i) {
        result[i] = LogMessageText(texts[i], arguments[i]);
    }
    return result;
}

// CREATORS

inline constexpr LogMessageText::LogMessageText() noexcept
: d_prefix("?")
, d_argument(LogMessage::Argument::e_NONE)
{
}

inline consteval LogMessageText::LogMessageText(
    const std::string_view text, const LogMessage::Argument argument) noexcept
: d_prefix(text)
, d_argument(argument)
{
    if (!is_valid(text, argument)) {
        reject_text();
    }

    // Split at the placeholder, if there is one: the argument goes between.
    const std::size_t placeholder = text.find(k_PLACEHOLDER);
    if (placeholder != std::string_view::npos) {
        d_prefix = text.substr(0, placeholder);
        d_suffix = text.substr(placeholder + k_PLACEHOLDER.size());
    }
}

// ACCESSORS

inline constexpr std::string_view LogMessageText::prefix() const noexcept
{
    return d_prefix;
}

inline constexpr std::string_view LogMessageText::suffix() const noexcept
{
    return d_suffix;
}

inline constexpr LogMessage::Argument LogMessageText::argument() const noexcept
{
    return d_argument;
}

                         // ======================
                         // struct LogMessageTexts
                         // ======================

/// This traits type holds the texts of the enumerators of [t_MESSAGE]: each
/// specialization defines [k_TEXTS], one text per enumerator, in enumerator
/// order, made by [LogMessageText::table].
template <class t_MESSAGE> struct LogMessageTexts;

/// This concept is satisfied by a message enumeration whose texts are known: a
/// [LogMessageIdType] with a [LogMessageTexts] specialization.
template <class t_MESSAGE>
concept LogMessageIdWithText = LogMessageIdType<t_MESSAGE> && requires {
    LogMessageTexts<t_MESSAGE>::k_TEXTS;
};

                      // -------------------------------
                      // LogMessageTexts specializations
                      // -------------------------------

/// Give the enumerators of [LogMessage::CoreMessage] their texts.
template <> struct LogMessageTexts<LogMessage::CoreMessage> {
    static constexpr auto k_TEXTS =
        LogMessageText::table<LogMessage::CoreMessage>({
            "unknown",
        });
};

/// Give the enumerators of [LogMessage::VulkanMessage] their texts.
template <> struct LogMessageTexts<LogMessage::VulkanMessage> {
    static constexpr auto k_TEXTS =
        LogMessageText::table<LogMessage::VulkanMessage>({
            "unknown",
        });
};

/// Give the enumerators of [LogMessage::MetalMessage] their texts.
template <> struct LogMessageTexts<LogMessage::MetalMessage> {
    static constexpr auto k_TEXTS =
        LogMessageText::table<LogMessage::MetalMessage>({
            "unknown",
        });
};

// One text per enumerator: a new message without its text does not compile.
static_assert(LogMessageTexts<LogMessage::CoreMessage>::k_TEXTS.size() ==
              std::to_underlying(LogMessage::CoreMessage::e_COUNT));
static_assert(LogMessageTexts<LogMessage::VulkanMessage>::k_TEXTS.size() ==
              std::to_underlying(LogMessage::VulkanMessage::e_COUNT));
static_assert(LogMessageTexts<LogMessage::MetalMessage>::k_TEXTS.size() ==
              std::to_underlying(LogMessage::MetalMessage::e_COUNT));

                         // =======================
                         // class LogMessageCatalog
                         // =======================

/// This utility class gives each [LogMessage] the name of its domain and its
/// text. A domain or a message that no enumerator has reads ["?"].
class LogMessageCatalog final {
  private:
    // CLASS DATA

    /// Name of a domain that no enumerator has.
    inline static constexpr std::string_view k_UNNAMED = "?";

    /// Text of a message that no enumerator has: the default text.
    inline static constexpr LogMessageText k_UNNAMED_TEXT{};

    // PRIVATE CLASS METHODS

    /// Return the text at the specified [index] of the specified [texts], or
    /// [k_UNNAMED_TEXT] if [index] is out of range.
    template <std::size_t t_SIZE>
    [[nodiscard]] static constexpr LogMessageText
    lookup(const std::array<LogMessageText, t_SIZE>& texts,
           const std::size_t                         index) noexcept;

  public:
    // CLASS METHODS

    /// Return the name of the specified [domain], or ["?"] if no enumerator
    /// has its value.
    [[nodiscard("A stringified variant shall be used or "
                "printed")]] static constexpr std::string_view
    to_string_domain(const LogMessage::Domain domain) noexcept;

    /// Return the text of the specified [id], or the text ["?"] if no
    /// enumerator has its value.
    template <LogMessageIdWithText t_MESSAGE>
    [[nodiscard]] static constexpr LogMessageText
    text(const t_MESSAGE id) noexcept;

    /// Return the text of the specified [message], its id read in the message
    /// enumeration of its domain, or the text ["?"] if its domain or its id
    /// has none.
    [[nodiscard]] static constexpr LogMessageText
    text_of(const LogMessage message) noexcept;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

                         // -----------------------
                         // class LogMessageCatalog
                         // -----------------------

// PRIVATE CLASS METHODS

template <std::size_t t_SIZE>
inline constexpr LogMessageText
LogMessageCatalog::lookup(const std::array<LogMessageText, t_SIZE>& texts,
                          const std::size_t index) noexcept
{
    return index < texts.size() ? texts[index] : k_UNNAMED_TEXT;
}

// CLASS METHODS

inline constexpr std::string_view
LogMessageCatalog::to_string_domain(const LogMessage::Domain domain) noexcept
{
    const auto index = static_cast<std::size_t>(std::to_underlying(domain));
    return index < k_DOMAIN_NAMES.size() ? k_DOMAIN_NAMES[index] : k_UNNAMED;
}

template <LogMessageIdWithText t_MESSAGE>
inline constexpr LogMessageText
LogMessageCatalog::text(const t_MESSAGE id) noexcept
{
    return lookup(LogMessageTexts<t_MESSAGE>::k_TEXTS,
                  static_cast<std::size_t>(std::to_underlying(id)));
}

inline constexpr LogMessageText
LogMessageCatalog::text_of(const LogMessage message) noexcept
{
    switch (message.domain()) {
    case LogMessage::Domain::e_UNKNOWN:
    case LogMessage::Domain::e_COUNT: return k_UNNAMED_TEXT;          // RETURN
    case LogMessage::Domain::e_CORE:
        return text(message.id_as<LogMessage::CoreMessage>());        // RETURN
    case LogMessage::Domain::e_VULKAN:
        return text(message.id_as<LogMessage::VulkanMessage>());      // RETURN
    case LogMessage::Domain::e_METAL:
        return text(message.id_as<LogMessage::MetalMessage>());       // RETURN
    }

    // Not dead code, although every enumerator has a case above: [from_raw]
    // accepts any 64 bits, so [domain()] can hold a value no enumerator names.
    // Without this line that is undefined behavior, and GCC refuses to build
    // it under -Werror=return-type.
    return k_UNNAMED_TEXT;
}

}  // close namespace log
}  // close namespace engine::core

#endif
