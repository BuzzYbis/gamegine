// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_format.t.cpp                                                  -*-C++-*-

//@PURPOSE: Verify that 'LineWriter' never writes past its buffer, says when
// it cut something, and writes each value exactly; and that the two parser
// bases accept only what they document.
//
//@DESCRIPTION: 'LineWriter' is what every log line goes through, straight
// into the logger's output batch, so its promises are checked at the edges:
//
//: o Nothing past the buffer. Guard bytes around a window catch an
//:   off-by-one in either direction, and a whole word 'append_continued'
//:   copies across the buffer's end.
//:
//: o Truncation is reported, and stays reported: 'is_truncated' is asked
//:   once, at the end of a line.
//:
//: o Each 'append_*' writes exactly the documented text, including the
//:   extremes of every integer type.
//:
//: o 'append_continued' agrees with a byte-by-byte reference on awkward
//:   input, with control characters at every position of an eight-byte
//:   word, so its word-at-a-time copy can never drift from the simple one.

#include <engine/core/core_format.h>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

using engine::core::format::AlternateFlagParser;
using engine::core::format::EmptySpecParser;
using engine::core::format::LineWriter;

/// Filler the writer never writes, so an untouched byte is visible.
constexpr char k_poison = '#';

/// Return what 'append' does to an empty writer over 'size' bytes, as a
/// string, for tests that only care about the text.
template <class t_APPEND>
std::string written_by(const std::size_t size, const t_APPEND& append)
{
    std::string buffer(size, k_poison);
    LineWriter  writer(buffer);
    append(writer);
    return std::string(writer.view());
}

/// The obvious implementation of 'append_continued', one byte at a time, to
/// compare the real one against.
std::string reference_continued(std::string_view  text,
                                const std::size_t column)
{
    if (!text.empty() && text.back() == '\n') {
        text.remove_suffix(1);
        if (!text.empty() && text.back() == '\r') {
            text.remove_suffix(1);
        }
    }
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == '\n') {
            result += '\n';
            result.append(column, ' ');
        } else if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            // dropped: the '\n' that follows ends the line
        } else if (c < 0x20U || c == 0x7FU) {
            result += '?';
        } else {
            result += static_cast<char>(c);
        }
    }
    return result;
}

// -------------------------------------------------------------- bounds -----

TEST(CoreFormatLineWriter, AppendWritesAndCounts)
{
    std::array<char, 16> buffer{};
    LineWriter           writer(buffer);
    writer.append("ab");
    writer.append("cd");

    EXPECT_EQ(writer.view(), "abcd");
    EXPECT_EQ(writer.size(), 4U);
    EXPECT_FALSE(writer.is_truncated());
}

TEST(CoreFormatLineWriter, NothingIsWrittenPastTheBuffer)
{
    // One guard byte on each side of the four the writer is given.
    std::array<char, 6> storage{};
    storage.fill(k_poison);
    // Named first: 'LineWriter writer(std::span(storage)...)' is parsed by
    // GCC as a function declaration taking a parameter called 'storage'.
    const std::span<char> window = std::span(storage).subspan(1, 4);
    LineWriter            writer(window);

    writer.append("abcdefgh");
    writer.append_char('x');
    writer.append_padded("", 10);
    writer.append_fitted("renderer_thread", 8);
    writer.append_decimal(123456789);
    writer.append_hex<16>(0);
    writer.append_continued("a\nb\nc", 8);

    EXPECT_EQ(writer.view(), "abcd");
    EXPECT_EQ(storage.front(), k_poison);
    EXPECT_EQ(storage.back(), k_poison);
}

TEST(CoreFormatLineWriter, TruncationIsReportedAndStaysReported)
{
    std::array<char, 4> buffer{};
    LineWriter          writer(buffer);

    writer.append("abc");
    EXPECT_FALSE(writer.is_truncated());
    writer.append("de");
    EXPECT_TRUE(writer.is_truncated());
    EXPECT_EQ(writer.view(), "abcd");

    // Nothing more fits, and nothing un-truncates the line.
    writer.append("");
    EXPECT_TRUE(writer.is_truncated());
}

TEST(CoreFormatLineWriter, ExactFitIsNotTruncation)
{
    std::array<char, 4> buffer{};
    LineWriter          writer(buffer);
    writer.append("abcd");

    EXPECT_EQ(writer.view(), "abcd");
    EXPECT_FALSE(writer.is_truncated());
}

TEST(CoreFormatLineWriter, EmptyBufferAcceptsNothing)
{
    LineWriter writer(std::span<char>{});
    writer.append("");
    EXPECT_FALSE(writer.is_truncated());

    writer.append("a");
    EXPECT_TRUE(writer.is_truncated());
    EXPECT_EQ(writer.size(), 0U);
}

TEST(CoreFormatLineWriter, CannotFail)
{
    // The logger calls these for every record it writes. The text is passed
    // as a 'string_view' already: converting a literal is not 'noexcept'
    // (it measures the string), and would hide what is being checked.
    const std::string_view text;
    static_assert(noexcept(std::declval<LineWriter&>().append(text)));
    static_assert(noexcept(std::declval<LineWriter&>().append_continued(text,
                                                                        0)));
    static_assert(noexcept(std::declval<LineWriter&>().append_padded(text,
                                                                     0)));
    static_assert(noexcept(std::declval<LineWriter&>().append_fitted(text,
                                                                     0)));
    static_assert(noexcept(std::declval<LineWriter&>().append_decimal(0)));
    static_assert(noexcept(std::declval<LineWriter&>().append_hex<8>(0)));
    static_assert(noexcept(std::declval<LineWriter&>().append_float(0.0F)));
    SUCCEED();
}

// ------------------------------------------------------------- padding -----

TEST(CoreFormatLineWriter, PaddingOnlyAdds)
{
    // The width is a minimum, as in '{:<N}': a longer text is written whole.
    EXPECT_EQ(written_by(16,
                         [](LineWriter& w) { w.append_padded("WARN", 5); }),
              "WARN ");
    EXPECT_EQ(written_by(16,
                         [](LineWriter& w) { w.append_padded("WARN", 4); }),
              "WARN");
    EXPECT_EQ(written_by(16,
                         [](LineWriter& w) {
                             w.append_padded("renderer_thread", 8);
                         }),
              "renderer_thread");
    EXPECT_EQ(written_by(16,
                         [](LineWriter& w) { w.append_padded("ab", 4, '.'); }),
              "ab..");
}

TEST(CoreFormatLineWriter, FittedTextIsExactlyItsWidth)
{
    // Cut if longer, padded if shorter, as in '{:<N.N}'.
    EXPECT_EQ(written_by(16,
                         [](LineWriter& w) { w.append_fitted("WARN", 5); }),
              "WARN ");
    EXPECT_EQ(written_by(16,
                         [](LineWriter& w) {
                             w.append_fitted("renderer_thread", 8);
                         }),
              "renderer");
    EXPECT_EQ(written_by(16,
                         [](LineWriter& w) { w.append_fitted("ab", 4, '.'); }),
              "ab..");
    EXPECT_EQ(written_by(16, [](LineWriter& w) { w.append_fitted("ab", 0); }),
              "");
}

TEST(CoreFormatLineWriter, CuttingToTheWidthIsNotTruncation)
{
    std::array<char, 16> roomy{};
    LineWriter           fitted(roomy);
    fitted.append_fitted("renderer_thread", 8);
    EXPECT_EQ(fitted.view(), "renderer");
    EXPECT_FALSE(fitted.is_truncated());

    // Running out of buffer still is.
    std::array<char, 4> small{};
    LineWriter          cut(small);
    cut.append_fitted("renderer_thread", 8);
    EXPECT_EQ(cut.view(), "rend");
    EXPECT_TRUE(cut.is_truncated());
}

TEST(CoreFormatLineWriter, PaddingAgreesWithStdFormat)
{
    // For ASCII text 'append_padded' is '{:<N}' and 'append_fitted' is
    // '{:<N.N}', at every width around each text's length.
    const std::array<std::string_view, 4> texts = {
        "",
        "a",
        "WARN",
        "renderer_thread",
    };
    for (const std::string_view text : texts) {
        for (std::size_t width = 0; width <= 20; ++width) {
            EXPECT_EQ(written_by(32,
                                 [&](LineWriter& w) {
                                     w.append_padded(text, width);
                                 }),
                      std::format("{:<{}}", text, width))
                << '"' << text << "\" in " << width;
            EXPECT_EQ(written_by(32,
                                 [&](LineWriter& w) {
                                     w.append_fitted(text, width);
                                 }),
                      std::format("{:<{}.{}}", text, width, width))
                << '"' << text << "\" in " << width;
        }
    }
}

// ------------------------------------------------------------- decimal -----

TEST(CoreFormatLineWriter, DecimalWritesEveryExtreme)
{
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_decimal(0); }), "0");
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) {
                             w.append_decimal(
                                 std::numeric_limits<std::uint64_t>::max());
                         }),
              "18446744073709551615");
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) {
                             w.append_decimal(
                                 std::numeric_limits<std::int64_t>::min());
                         }),
              "-9223372036854775808");
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) {
                             w.append_decimal(static_cast<signed char>(-128));
                         }),
              "-128");
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) {
                             w.append_decimal(static_cast<unsigned char>(255));
                         }),
              "255");
}

TEST(CoreFormatLineWriter, DecimalIsRightAlignedInItsColumn)
{
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_decimal(12, 4); }),
              "  12");
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) { w.append_decimal(42, 6, '0'); }),
              "000042");
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) { w.append_decimal(12345, 3); }),
              "12345");
}

TEST(CoreFormatLineWriter, ZeroFillGoesAfterTheSign)
{
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) { w.append_decimal(-42, 6, '0'); }),
              "-00042");
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_decimal(-42, 6); }),
              "   -42");
}

// ----------------------------------------------------------------- hex -----

TEST(CoreFormatLineWriter, HexIsExactlyItsDigitCount)
{
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_hex<8>(0xCD); }),
              "000000cd");
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) {
                             w.append_hex<16>(
                                 std::numeric_limits<std::uint64_t>::max());
                         }),
              "ffffffffffffffff");
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_hex<1>(0xF); }),
              "f");
}

TEST(CoreFormatLineWriter, HexKeepsOnlyTheLowBits)
{
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_hex<2>(0x1234); }),
              "34");
}

// --------------------------------------------------------------- float -----

/// Expect 'append_decimal' to write both limits of 't_INTEGER' exactly as
/// 'std::format' does.
template <class t_INTEGER> void expect_limits_match_std_format()
{
    const std::array<t_INTEGER, 2> limits = {
        std::numeric_limits<t_INTEGER>::min(),
        std::numeric_limits<t_INTEGER>::max(),
    };
    for (const t_INTEGER value : limits) {
        EXPECT_EQ(written_by(32,
                             [&](LineWriter& w) { w.append_decimal(value); }),
                  std::format("{}", value));
    }
}

TEST(CoreFormatLineWriter, DecimalFitsTheLimitsOfEveryType)
{
    // The digit buffer is sized to the widest value exactly: a minimum with
    // its sign, or a maximum. A buffer one byte short fails here.
    expect_limits_match_std_format<signed char>();
    expect_limits_match_std_format<unsigned char>();
    expect_limits_match_std_format<short>();
    expect_limits_match_std_format<unsigned short>();
    expect_limits_match_std_format<int>();
    expect_limits_match_std_format<unsigned int>();
    expect_limits_match_std_format<long>();
    expect_limits_match_std_format<unsigned long>();
    expect_limits_match_std_format<long long>();
    expect_limits_match_std_format<unsigned long long>();
}

TEST(CoreFormatLineWriter, FloatIsShortestExact)
{
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_float(1.5F); }),
              "1.5");
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_float(0.1F); }),
              "0.1");
    EXPECT_EQ(written_by(32, [](LineWriter& w) { w.append_float(-2.0F); }),
              "-2");
}

TEST(CoreFormatLineWriter, LongestFloatFormFits)
{
    // No float's shortest form is longer than 15 characters (checked over
    // all 2^32 of them), and the digit buffer is exactly that long. This is
    // one of the longest.
    EXPECT_EQ(written_by(32,
                         [](LineWriter& w) {
                             w.append_float(std::bit_cast<float>(0x83AA250CU));
                         }),
              "-1.00002075e-36");
}

// ----------------------------------------------------------- continued -----

TEST(CoreFormatLineWriter, ContinuedLinesStartAtTheColumn)
{
    EXPECT_EQ(
        written_by(64, [](LineWriter& w) { w.append_continued("ab\ncd", 4); }),
        "ab\n    cd");
}

TEST(CoreFormatLineWriter, OneTrailingNewlineIsDropped)
{
    EXPECT_EQ(written_by(64,
                         [](LineWriter& w) { w.append_continued("ab\n", 4); }),
              "ab");
    EXPECT_EQ(
        written_by(64, [](LineWriter& w) { w.append_continued("ab\r\n", 4); }),
        "ab");
}

TEST(CoreFormatLineWriter, CarriageReturnLineFeedIsOneNewline)
{
    EXPECT_EQ(written_by(64,
                         [](LineWriter& w) {
                             w.append_continued("ab\r\ncd", 2);
                         }),
              "ab\n  cd");
}

TEST(CoreFormatLineWriter, ControlCharactersBecomeQuestionMarks)
{
    // An escape sequence in untrusted text would otherwise reach whoever
    // 'cat's the log in a terminal.
    EXPECT_EQ(written_by(64,
                         [](LineWriter& w) {
                             w.append_continued("a\x1b[31mb\x7f\tc\rd", 0);
                         }),
              "a?[31mb??c?d");
}

TEST(CoreFormatLineWriter, NoContinuationLineCanLookLikeANewRecord)
{
    // Records start with '['; a continuation line starts with the column's
    // spaces, so it can never be mistaken for one.
    const std::string text = written_by(128, [](LineWriter& w) {
        w.append_continued("x\n[12.0] forged\n[13.0] again", 4);
    });
    std::size_t       line = text.find('\n');
    while (line != std::string::npos) {
        EXPECT_EQ(text.compare(line + 1, 4, "    "), 0) << text;
        line = text.find('\n', line + 1);
    }
}

TEST(CoreFormatLineWriter, ContinuedAgreesWithTheReference)
{
    std::string every_byte;
    for (int c = 0; c < 256; ++c) {
        every_byte += static_cast<char>(c);
    }
    const std::array<std::string, 9> inputs = {
        std::string(""),
        std::string("\n"),
        std::string("\r\n"),
        std::string("plain text, no specials at all"),
        std::string("\n\nleading and doubled\n\n"),
        std::string("\r\r\n\r"),
        std::string("vkCmdDraw(): 3 bindings\nSpec: VUID-04007\n"),
        std::string("tab\there\x1b[0m and \x7f del\r\nend\r\n"),
        every_byte,
    };
    for (const std::string& input : inputs) {
        for (const std::size_t column : {std::size_t{0}, std::size_t{7}}) {
            EXPECT_EQ(written_by(4096,
                                 [&](LineWriter& w) {
                                     w.append_continued(input, column);
                                 }),
                      reference_continued(input, column))
                << "column " << column;
        }
    }
}

TEST(CoreFormatLineWriter, ContinuedAgreesAtEveryPositionInAWord)
{
    // The copy checks eight bytes at a time. Put each control character at
    // every position of texts up to three words long, among the bytes a
    // word test could get wrong: a space or a '~' right after a control
    // character (flagged by a borrow), and bytes above 0x7f.
    const std::array<char, 6> controls =
        {'\n', '\r', '\t', '\0', '\x1f', '\x7f'};
    const std::array<char, 5> fillers = {'a', ' ', '~', '\x80', '\xff'};
    for (const char filler : fillers) {
        for (std::size_t size = 1; size <= 24; ++size) {
            for (std::size_t at = 0; at < size; ++at) {
                for (const char control : controls) {
                    std::string text(size, filler);
                    text[at] = control;
                    EXPECT_EQ(written_by(256,
                                         [&](LineWriter& w) {
                                             w.append_continued(text, 3);
                                         }),
                              reference_continued(text, 3))
                        << "size " << size << ", control at " << at;
                }
            }
        }
    }
}

TEST(CoreFormatLineWriter, ContinuedAgreesForEveryPairOfPositions)
{
    // Two control characters in one word, and "\r\n" split across two.
    const std::array<char, 3> controls = {'\n', '\r', '\x7f'};
    for (const char filler : {' ', '~'}) {
        for (std::size_t size = 2; size <= 20; ++size) {
            for (std::size_t first = 0; first < size; ++first) {
                for (std::size_t second = first + 1; second < size; ++second) {
                    for (const char a : controls) {
                        for (const char b : controls) {
                            std::string text(size, filler);
                            text[first]  = a;
                            text[second] = b;
                            EXPECT_EQ(written_by(256,
                                                 [&](LineWriter& w) {
                                                     w.append_continued(text,
                                                                        3);
                                                 }),
                                      reference_continued(text, 3))
                                << "size " << size << ", controls at " << first
                                << " and " << second;
                        }
                    }
                }
            }
        }
    }
}

TEST(CoreFormatLineWriter, ContinuedTextIsCutLikeAnythingElse)
{
    std::array<char, 8> buffer{};
    LineWriter          writer(buffer);
    writer.append_continued("abc\ndef\nghi", 4);

    EXPECT_TRUE(writer.is_truncated());
    EXPECT_EQ(writer.view(), "abc\n    ");
}

/// Expect 'append_continued' of 'text' into a buffer of 'size' bytes to write
/// the reference, cut at the buffer's end, and nothing outside the buffer.
void expect_continued_inside(const std::string& text, const std::size_t size)
{
    std::string           storage(size + 2, k_poison);
    const std::span<char> window = std::span(storage).subspan(1, size);
    LineWriter            writer(window);
    writer.append_continued(text, 3);

    const std::string expected = reference_continued(text, 3);
    EXPECT_EQ(writer.view(), std::string_view(expected).substr(0, size));
    EXPECT_EQ(writer.is_truncated(), expected.size() > size);
    EXPECT_EQ(storage.front(), k_poison);
    EXPECT_EQ(storage.back(), k_poison);
}

TEST(CoreFormatLineWriter, ContinuedWordsStayInsideTheBuffer)
{
    // The copy stores whole words and may leave scratch past 'size()', but
    // never past the buffer: buffers of every size up to five words, with a
    // control character at every position of the text, or none.
    for (std::size_t size = 0; size <= 40; ++size) {
        for (std::size_t at = 0; at <= 24; ++at) {
            for (const char control : {'\n', '\x7f'}) {
                std::string text(24, 'a');
                if (at < text.size()) {
                    text[at] = control;
                }
                SCOPED_TRACE("size " + std::to_string(size) + ", control at " +
                             std::to_string(at));
                expect_continued_inside(text, size);
            }
        }
    }
}

// ------------------------------------------------------------- parsers -----

TEST(CoreFormatParser, EmptySpecParserAcceptsOnlyAnEmptySpecification)
{
    std::format_parse_context context("}");
    EXPECT_EQ(EmptySpecParser::parse(context), context.begin());

    // Also usable where std::format uses it: in a constant expression.
    static_assert([] {
        std::format_parse_context compile_time("}");
        return *EmptySpecParser::parse(compile_time) == '}';
    }());
}

TEST(CoreFormatParser, AlternateFlagParserRecordsTheHash)
{
    std::format_parse_context plain("}");
    AlternateFlagParser       without;
    EXPECT_EQ(*without.parse(plain), '}');
    EXPECT_FALSE(without.is_alternate());

    std::format_parse_context hash("#}");
    AlternateFlagParser       with;
    EXPECT_EQ(*with.parse(hash), '}');
    EXPECT_TRUE(with.is_alternate());

    static_assert([] {
        std::format_parse_context compile_time("#}");
        AlternateFlagParser       parser;
        (void)parser.parse(compile_time);
        return parser.is_alternate();
    }());
}

// GoogleTest runs suites named '*DeathTest' first, before any threads exist.
TEST(CoreFormatParserDeathTest, UnsupportedSpecificationsAbortAtRunTime)
{
    // At compile time these are build errors; at run time -- a format string
    // built while running -- they must abort, not continue with garbage.
    EXPECT_DEATH(
        {
            std::format_parse_context context("x}");
            (void)EmptySpecParser::parse(context);
        },
        "");
    EXPECT_DEATH(
        {
            std::format_parse_context context("#}");
            (void)EmptySpecParser::parse(context);
        },
        "");
    EXPECT_DEATH(
        {
            std::format_parse_context context("#x}");
            AlternateFlagParser       parser;
            (void)parser.parse(context);
        },
        "");
}

}  // close unnamed namespace
