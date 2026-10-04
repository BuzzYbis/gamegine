// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_format.h                                                      -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_FORMAT_H
#define INCLUDED_ENGINE_CORE_FORMAT_H

//@PURPOSE: Provide a bounded line writer and [std::formatter] parser bases.
//
//@CLASSES:
//  engine::core::format::LineWriter: cursor appending text to a buffer
//  engine::core::format::EmptySpecParser: [parse] accepting [{}] only
//  engine::core::format::AlternateFlagParser: [parse] accepting [{}], [{:#}]
//
//@SEE_ALSO: core_error_format
//
//@DESCRIPTION: [LineWriter] appends text into a buffer its caller owns. It
// never writes past the buffer, never allocates, never fails, and writes no
// terminating NUL: an append that does not fit is cut at the buffer's end,
// and [is_truncated] says so once, at the end of the line. The buffer past
// [size()] is scratch: [append_continued] copies whole words and may leave
// a few characters there. Numbers go through [std::to_chars]: no locale, no
// allocation, no format string.
//
// [EmptySpecParser] and [AlternateFlagParser] are bases for [std::formatter]
// specializations. They reject every other format specification through
// [reject_format_spec], so a literal [{:x}] is a compile error, and one
// built at run time aborts.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Writing a Line into a Fixed Buffer
///- - - - - - - - - - - - - - - - - - - - - - -
// ```
//  std::array<char, 64> buffer;
//  LineWriter           writer(buffer);
//  writer.append_padded("WARN", 5);
//  writer.append_decimal(42);
//  writer.append_char('\n');
//  // writer.view() == "WARN 42\n", writer.is_truncated() == false
// ```

// std
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <climits>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

namespace engine::core {
namespace format {

/// Stop a format specification that a formatter does not support: at
/// compile time for a literal format string, since this function is not
/// [constexpr], and by aborting at run time, since exceptions are off.
[[noreturn]] void reject_format_spec() noexcept;

                         // ================
                         // class LineWriter
                         // ================

/// This mechanism appends text to a buffer it does not own. Whatever does
/// not fit is cut at the buffer's end, and the writer remembers it was (see
/// [is_truncated]). It never allocates and never fails.
class LineWriter final {
  private:
    // DATA

    std::span<char> d_buffer;             // caller's storage (not owned)
    std::size_t     d_size      = 0;      // characters written so far
    bool            d_truncated = false;  // [true] once an append was cut

    // PRIVATE CLASS METHODS

    /// Copy the specified [text] to the specified [out] up to its first
    /// control character, a byte below [0x20] or [0x7f], and return that
    /// character's position, or [text.size()] if it has none. The behavior
    /// is undefined unless [text] fits in [out]. Note that [out] past the
    /// returned position may be overwritten.
    static std::size_t copy_plain(const std::span<char>  out,
                                  const std::string_view text) noexcept;

    // PRIVATE MANIPULATORS

    /// Append the specified [text] up to its first control character, cut
    /// at the buffer's end, and return that character's position, or
    /// [text.size()] if it has none or the buffer ran out. Note that the
    /// buffer past [size()] may be overwritten.
    std::size_t append_plain(const std::string_view text) noexcept;

    /// Append the specified number of [copies] of the specified character
    /// [c], cut at the buffer's end.
    void append_repeated(const char c, const std::size_t copies) noexcept;

    // NOT IMPLEMENTED

    LineWriter(const LineWriter&)            = delete;
    LineWriter& operator=(const LineWriter&) = delete;

  public:
    // CREATORS

    /// Create a [LineWriter] object appending to the specified [buffer]. The
    /// behavior is undefined unless [buffer] outlives this object.
    explicit constexpr LineWriter(std::span<char> buffer) noexcept;

    /// Destroy this object.
    //! ~LineWriter() = default;

    // MANIPULATORS

    /// Append the specified [text], cut at the buffer's end.
    void append(const std::string_view text) noexcept;

    /// Append the specified [text], which may span several lines, starting
    /// each line after the first with the specified [column] spaces so that
    /// it continues under the message. Drop one trailing newline, read
    /// ["\r\n"] as a newline, and write any other control character (below
    /// [0x20], or [0x7f]) as ['?']. Note that the buffer past [size()] may
    /// be overwritten.
    void append_continued(const std::string_view text,
                          const std::size_t      column) noexcept;

    /// Append the specified character [c].
    void append_char(const char c) noexcept;

    /// Append the specified [text], followed by fill characters up to the
    /// specified [pad_width] characters if it is shorter; a longer [text] is
    /// written whole, as [{:<N}] does. Optionally specify the [pad_fill]
    /// character, a space by default.
    void append_padded(const std::string_view text,
                       const std::size_t      pad_width,
                       const char             pad_fill = ' ') noexcept;

    /// Append the specified [text] in exactly the specified [width]
    /// characters: cut if it is longer, padded if it is shorter, as
    /// [{:<N.N}] does. Optionally specify the [pad_fill] character, a space
    /// by default. Note that cutting [text] to [width] is not truncation:
    /// only running out of buffer makes [is_truncated] return [true].
    void append_fitted(const std::string_view text,
                       const std::size_t      width,
                       const char             pad_fill = ' ') noexcept;

    /// Append the specified [value] in decimal. Optionally specify a
    /// [pad_width] to right-align it in, 0 (no padding) by default, and the
    /// [pad_fill] character, a space by default. With a ['0'] fill, a minus
    /// sign stays in front of the zeros: [-0042].
    template <class t_INTEGER>
        requires(std::integral<t_INTEGER> &&
                 !std::same_as<bool, std::remove_cv_t<t_INTEGER>> &&
                 !std::same_as<char, std::remove_cv_t<t_INTEGER>>)
    void append_decimal(const t_INTEGER   value,
                        const std::size_t pad_width = 0,
                        const char        pad_fill  = ' ') noexcept;

    /// Append the low [4 * t_DIGITS] bits of the specified [value] as exactly
    /// [t_DIGITS] lower-case hex digits, zero-padded, with no [0x] prefix.
    template <std::size_t t_DIGITS>
    void append_hex(const std::uint64_t value) noexcept;

    /// Append the specified [value] in the shortest form that reads back
    /// exactly.
    void append_float(const float value) noexcept;

    // ACCESSORS

    /// Return the number of characters written so far.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Return [true] if an append so far was cut short, and [false]
    /// otherwise.
    [[nodiscard]] bool is_truncated() const noexcept;

    /// Return the characters written so far.
    [[nodiscard]] std::string_view view() const noexcept;
};

                         // ======================
                         // struct EmptySpecParser
                         // ======================

/// This base of a [std::formatter] specialization accepts the empty format
/// specification, [{}], and nothing else.
struct EmptySpecParser {
    // CLASS METHODS

    /// Parse the format specification in the specified [context], and return
    /// the iterator to its closing ['}']. Any specification but an empty one
    /// reaches [reject_format_spec].
    static constexpr std::format_parse_context::iterator
    parse(std::format_parse_context& context);

    // CREATORS

    /// Create an [EmptySpecParser] object.
    //! EmptySpecParser() = default;

    /// Create an [EmptySpecParser] object having the same value as the
    /// specified [original] object.
    //! EmptySpecParser(const EmptySpecParser& original) = default;

    /// Destroy this object.
    //! ~EmptySpecParser() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! EmptySpecParser& operator=(const EmptySpecParser& rhs) = default;
};

                         // =========================
                         // class AlternateFlagParser
                         // =========================

/// This base of a [std::formatter] specialization accepts [{}] and the
/// alternate form [{:#}], and remembers which one it parsed.
class AlternateFlagParser {
  private:
    // DATA

    bool d_alternate = false;  // [true] if the specification was [#]

  public:
    // CREATORS

    /// Create an [AlternateFlagParser] object, with the alternate flag unset.
    //! AlternateFlagParser() = default;

    /// Create an [AlternateFlagParser] object having the same value as the
    /// specified [original] object.
    //! AlternateFlagParser(const AlternateFlagParser& original) = default;

    /// Destroy this object.
    //! ~AlternateFlagParser() = default;

    // MANIPULATORS

    /// Assign to this object the value of the specified [rhs] object, and
    /// return a reference providing modifiable access to this object.
    //! AlternateFlagParser&
    //! operator=(const AlternateFlagParser& rhs) = default;

    /// Parse the format specification in the specified [context], record
    /// whether it is [#], and return the iterator to its closing ['}']. Any
    /// specification but an empty one or [#] reaches [reject_format_spec].
    constexpr std::format_parse_context::iterator
    parse(std::format_parse_context& context);

    // ACCESSORS

    /// Return [true] if the parsed specification was [#], and [false]
    /// otherwise.
    [[nodiscard]] constexpr bool is_alternate() const noexcept;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

// Deliberately not [constexpr], and that is the whole mechanism: a literal
// format string is checked by running [parse] in a constant expression, and
// a call to a non-[constexpr] function there fails to compile, with an error
// naming this function.
inline void reject_format_spec() noexcept
{
    std::abort();
}

                         // ----------------
                         // class LineWriter
                         // ----------------

// PRIVATE CLASS METHODS

inline std::size_t LineWriter::copy_plain(const std::span<char>  out,
                                          const std::string_view text) noexcept
{
    static_assert(std::endian::native == std::endian::little,
                  "the first character must be the lowest byte of a word");

    // Control characters are the bytes below a space, and DEL.
    constexpr unsigned char k_SPACE = 0x20;
    constexpr unsigned char k_DEL   = 0x7F;

    // One byte value, repeated in all 8 bytes of a word:
    //   k_ONES    0x01  [below] 1 means equal to 0
    //   k_SPACES  0x20  space: control characters are below it...
    //   k_DELS    0x7f  ...and DEL; [x ^ k_DELS] turns DEL into 0
    //   k_HIGHS   0x80  the top bit of a byte, where [below] flags
    constexpr std::uint64_t k_ONES   = 0x0101'0101'0101'0101U;
    constexpr std::uint64_t k_SPACES = k_ONES * k_SPACE;
    constexpr std::uint64_t k_DELS   = k_ONES * k_DEL;
    constexpr std::uint64_t k_HIGHS  = k_ONES * 0x80U;

    // SWAR technique
    // Flag each byte of [x] below the matching byte of [limit] (top bit set).
    // Works only if every byte of [limit] is at most [0x80].
    // A borrow can also flag bytes above a real one: trust only the lowest.
    constexpr auto below = [](const std::uint64_t x,
                              const std::uint64_t limit) noexcept {
        return (x - limit) & ~x & k_HIGHS;
    };

    // Copy a word, [k_WORD_SIZE] (8) bytes, then check it: one pass, 18-44%
    // faster at -O3 on multi-line text than checking a run, then copying it
    // with [memcpy]. Bytes copied past a control character are scratch: the
    // returned position leaves them out.
    constexpr std::size_t k_WORD_SIZE = sizeof(std::uint64_t);

    std::size_t position = 0;
    for (; text.size() - position >= k_WORD_SIZE; position += k_WORD_SIZE) {
        std::uint64_t word = 0;
        std::memcpy(&word, text.data() + position, k_WORD_SIZE);
        std::memcpy(out.data() + position, &word, k_WORD_SIZE);

        // Flag control characters: bytes below space, and DEL bytes.
        const std::uint64_t flagged = below(word, k_SPACES) |
                                      below(word ^ k_DELS, k_ONES);
        if (flagged != 0) {
            // Lowest flag = bit 7 of the first control character's byte
            // (little-endian): [countr_zero] / CHAR_BIT is that byte.
            const int byte = std::countr_zero(flagged) / CHAR_BIT;
            return position + static_cast<std::size_t>(byte);         // RETURN
        }
    }

    // Last 0 to 7 bytes, one by one. An overlapping last word measured no
    // better at -O3 (faster with GCC, slower with clang), and a 4-2-1 cascade
    // needs two more paths that must pad with spaces: a zero byte counts as
    // a control character.
    for (; position < text.size(); ++position) {
        const auto c = static_cast<unsigned char>(text[position]);
        if (c < k_SPACE || c == k_DEL) {
            return position;                                          // RETURN
        }
        out[position] = text[position];
    }

    return text.size();
}

// PRIVATE MANIPULATORS

inline std::size_t
LineWriter::append_plain(const std::string_view text) noexcept
{
    // Only the part of [text] that fits in the buffer is copied.
    const std::string_view fitting = text.substr(0, d_buffer.size() - d_size);
    const std::size_t control = copy_plain(d_buffer.subspan(d_size), fitting);
    d_size += control;
    if (control < fitting.size()) {
        return control;                                               // RETURN
    }

    // No control character in what fits: [text] ended, or the buffer did,
    // and then the rest is cut, whatever it holds.
    if (fitting.size() < text.size()) {
        d_truncated = true;
    }
    return text.size();
}

inline void LineWriter::append_repeated(const char        c,
                                        const std::size_t copies) noexcept
{
    const std::size_t room = d_buffer.size() - d_size;
    if (copies > room) {
        d_truncated = true;
    }

    const std::size_t to_write = std::min(copies, room);
    if (to_write != 0) {
        std::memset(d_buffer.data() + d_size, c, to_write);
        d_size += to_write;
    }
}

// CREATORS

inline constexpr LineWriter::LineWriter(std::span<char> buffer) noexcept
: d_buffer(buffer)
{
}

// MANIPULATORS

inline void LineWriter::append(const std::string_view text) noexcept
{
    const std::size_t room = d_buffer.size() - d_size;
    if (text.size() > room) {
        d_truncated = true;
    }

    // [memcpy] on an empty span's null [data()] is undefined even for a count
    // of zero, so the call is guarded.
    const std::size_t to_write = std::min(text.size(), room);
    if (to_write != 0) {
        std::memcpy(d_buffer.data() + d_size, text.data(), to_write);
        d_size += to_write;
    }
}

inline void LineWriter::append_continued(const std::string_view text,
                                         const std::size_t column) noexcept
{
    constexpr char             k_LF   = '\n';
    constexpr std::string_view k_CRLF = "\r\n";

    // One trailing newline ends the text, not a line of it.
    std::string_view rest = text;
    if (rest.ends_with(k_CRLF)) {
        rest.remove_suffix(k_CRLF.size());
    } else if (rest.ends_with(k_LF)) {
        rest.remove_suffix(1);
    }

    // Each pass copies the plain run up to the next control character, then
    // handles that one character.
    while (!rest.empty()) {
        const std::size_t control = append_plain(rest);
        if (control == rest.size()) {
            return;                                                   // RETURN
        }

        // [rest] now starts with the control character. In a CRLF the CR is
        // dropped: the LF ends the line on its own.
        rest.remove_prefix(control);
        if (rest.starts_with(k_CRLF)) {
            rest.remove_prefix(1);
        }
        if (rest.starts_with(k_LF)) {
            append_char(k_LF);
            append_repeated(' ', column);
        } else {
            append_char('?');
        }
        rest.remove_prefix(1);
    }
}

inline void LineWriter::append_char(const char c) noexcept
{
    append_repeated(c, 1);
}

inline void LineWriter::append_padded(const std::string_view text,
                                      const std::size_t      pad_width,
                                      const char             pad_fill) noexcept
{
    append(text);
    // Guarded: for a longer [text] the subtraction would wrap around.
    if (text.size() < pad_width) {
        append_repeated(pad_fill, pad_width - text.size());
    }
}

inline void LineWriter::append_fitted(const std::string_view text,
                                      const std::size_t      width,
                                      const char             pad_fill) noexcept
{
    // [substr] clamps: the first [width] characters, or all of a shorter
    // [text]. Padding then fills whatever is left of the width.
    append_padded(text.substr(0, width), width, pad_fill);
}

template <class t_INTEGER>
    requires(std::integral<t_INTEGER> &&
             !std::same_as<bool, std::remove_cv_t<t_INTEGER>> &&
             !std::same_as<char, std::remove_cv_t<t_INTEGER>>)
inline void LineWriter::append_decimal(const t_INTEGER   value,
                                       const std::size_t pad_width,
                                       const char        pad_fill) noexcept
{
    // The widest value has [digits10 + 1] digits, and a minus sign may lead.
    constexpr std::size_t k_MAX_SIZE =
        std::numeric_limits<t_INTEGER>::digits10 + 2;

    std::array<char, k_MAX_SIZE> digits{};
    const std::to_chars_result   result = std::to_chars(
        digits.data(), digits.data() + digits.size(), value);
    std::string_view text(digits.data(),
                          static_cast<std::size_t>(result.ptr -
                                                   digits.data()));

    if (text.size() < pad_width) {
        const std::size_t padding = pad_width - text.size();
        if (pad_fill == '0' && text.front() == '-') {
            append_char('-');
            text.remove_prefix(1);
        }
        append_repeated(pad_fill, padding);
    }
    append(text);
}

template <std::size_t t_DIGITS>
inline void LineWriter::append_hex(const std::uint64_t value) noexcept
{
    static_assert(t_DIGITS >= 1 && t_DIGITS <= 16,
                  "a 64-bit value has between 1 and 16 hex digits");

    constexpr std::string_view k_DIGITS = "0123456789abcdef";

    // Fill from the right, one hex digit (4 bits) at a time.
    std::array<char, t_DIGITS> text{};
    std::uint64_t              rest = value;
    for (std::size_t i = t_DIGITS; i > 0; --i) {
        text[i - 1] = k_DIGITS[static_cast<std::size_t>(rest & 0xFU)];
        rest >>= 4U;
    }

    append(std::string_view(text.data(), text.size()));
}

inline void LineWriter::append_float(const float value) noexcept
{
    // The shortest form that reads back exactly never exceeds 15 characters
    // ([-1.00002075e-36]), checked over all 2^32 floats.
    constexpr std::size_t k_MAX_SIZE = 15;

    std::array<char, k_MAX_SIZE> digits{};
    const std::to_chars_result   result = std::to_chars(
        digits.data(), digits.data() + digits.size(), value);

    append(std::string_view(
        digits.data(), static_cast<std::size_t>(result.ptr - digits.data())));
}

// ACCESSORS

inline std::size_t LineWriter::size() const noexcept
{
    return d_size;
}

inline bool LineWriter::is_truncated() const noexcept
{
    return d_truncated;
}

inline std::string_view LineWriter::view() const noexcept
{
    return std::string_view(d_buffer.data(), d_size);
}

                         // ----------------------
                         // struct EmptySpecParser
                         // ----------------------

// CLASS METHODS

inline constexpr std::format_parse_context::iterator
EmptySpecParser::parse(std::format_parse_context& context)
{
    // [begin()] points just past the [':'], or at the ['}'] if there is none.
    const std::format_parse_context::iterator it = context.begin();
    if (it != context.end() && *it != '}') {
        reject_format_spec();
    }

    return it;
}

                         // -------------------------
                         // class AlternateFlagParser
                         // -------------------------

// MANIPULATORS

inline constexpr std::format_parse_context::iterator
AlternateFlagParser::parse(std::format_parse_context& context)
{
    std::format_parse_context::iterator it = context.begin();
    if (it != context.end() && *it == '#') {
        d_alternate = true;
        ++it;
    }
    if (it != context.end() && *it != '}') {
        reject_format_spec();
    }

    return it;
}

// ACCESSORS

inline constexpr bool AlternateFlagParser::is_alternate() const noexcept
{
    return d_alternate;
}

}  // close namespace format
}  // close namespace engine::core

#endif
