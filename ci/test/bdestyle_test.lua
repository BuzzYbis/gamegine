-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the BDE pass that runs after clang-format on every format --
-- ci-format, the editor's format-on-save and the workspace task. It
-- rewrites source files, so the cases that matter most are the ones it must
-- NOT touch: page-wide separators other than INLINE DEFINITIONS,
-- single-line section markers, comments that merely mention a tag.
--
-- Rules are those of doc/CodingStandards(fromBDE_almost).pdf; the rule
-- number is given with each case.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("bdestyle", {rootdir = path.join(os.projectdir(), "ci", "lua")})

local function banner(indent, rule, title)
    local lead = string.rep(" ", indent) .. "// "
    local line = lead .. string.rep(rule, #title)
    return line .. "\n" .. lead .. title .. "\n" .. line .. "\n"
end

local function lines_of(text)
    local result = {}
    for l in (text .. "\n"):gmatch("([^\n]*)\n") do
        table.insert(result, l)
    end
    return result
end

function run(t)
    -- ------------------------------------------------ 11.6 RETURN ---

    local text = bdestyle.apply(
        "    if (x) {\n        return -1;  // RETURN\n    }\n")
    local line = lines_of(text)[2]
    testing.equal(t, "11.6.1: RETURN ends in column 79", #line, 79)
    testing.check(t, "the code before it is unchanged",
                  line:find("^        return %-1;  ") ~= nil, line)
    testing.equal(t, "aligning twice changes nothing", bdestyle.apply(text),
                  text)

    -- 11.6.3: no room on the line, so a line of its own.
    local long = "        return to_string_reason("
                 .. "error.reason_as<Error::VulkanReason>());"
    text = bdestyle.apply(long .. "  // RETURN\n")
    local l = lines_of(text)
    testing.equal(t, "11.6.3: the statement keeps its line", l[1], long)
    testing.equal(t, "11.6.3: the tag gets its own line, ending in 79",
                  #l[2], 79)
    testing.check(t, "and that line holds nothing but the tag",
                  l[2]:match("^%s+// RETURN$") ~= nil, l[2])
    testing.equal(t, "a tag on its own line stays put when run again",
                  bdestyle.apply(text), text)

    -- What clang-format does to a tag it cannot fit: a line of its own at
    -- the code's indentation. The pass right-justifies it.
    text = bdestyle.apply(long .. "\n        // RETURN\n")
    testing.equal(t, "a clang-format-indented tag is right-justified",
                  #lines_of(text)[2], 79)

    -- And if the statement has room after all, the tag moves back up.
    text = bdestyle.apply("        return f();\n        // RETURN\n")
    testing.equal(t, "a tag with room moves onto its statement",
                  #lines_of(text)[1], 79)
    testing.equal(t, "leaving no separate line", #lines_of(text), 2)

    text = bdestyle.apply("        return f();  // why\n        // RETURN\n")
    testing.equal(t, "a line with its own comment does not take the tag",
                  #lines_of(text)[2], 79)

    -- 11.6.2: a statement over several lines is tagged on its last line.
    text = bdestyle.apply("        return a\n            + b;  // RETURN\n")
    testing.equal(t, "11.6.2: the last line of a statement is tagged",
                  #lines_of(text)[2], 79)
    testing.equal(t, "and the first is left alone", lines_of(text)[1],
                  "        return a")

    local mention = "    // Explain why this is not a // RETURN\n"
    testing.equal(t, "a comment mentioning the tag is untouched",
                  bdestyle.apply(mention), mention)

    local note = "    x = 1;  // NOTE\n"
    testing.equal(t, "other trailing comments are untouched",
                  bdestyle.apply(note), note)

    -- --------------------------------------------- 7.4.2 IMPLICIT ---

    text = bdestyle.apply("    Handle(int value);  // IMPLICIT\n")
    testing.equal(t, "7.4.2: IMPLICIT is placed like RETURN",
                  #lines_of(text)[1], 79)

    -- ---------------------------------------------- 6.1.3 banners ---

    testing.equal(t, "6.1.3: a definition banner is indented 25 spaces",
                  bdestyle.apply(banner(0, "=", "class Error")),
                  banner(25, "=", "class Error"))
    testing.equal(t, "6.8.3: an implementation banner is indented alike",
                  bdestyle.apply(banner(0, "-", "class Error")),
                  banner(25, "-", "class Error"))
    testing.equal(t, "moving it twice changes nothing",
                  bdestyle.apply(banner(25, "=", "class Error")),
                  banner(25, "=", "class Error"))

    text = bdestyle.apply("// ----------------------\n// free functions\n"
                          .. "// ----------------------\n")
    testing.equal(t, "6.1.3: rules are exactly as long as the title", text,
                  banner(25, "-", "free functions"))

    -- A name of 20 characters or more is centred instead.
    local wide_name = "class ErrorReasonNamesSpecial"   -- name: 23
    testing.equal(t, "6.1.3: a long class name is centred",
                  bdestyle.apply(banner(0, "=", wide_name)),
                  banner(math.floor((79 - 3 - #wide_name) / 2), "=",
                         wide_name))
    testing.equal(t, "a 19-character name still gets 25 spaces",
                  bdestyle.apply(banner(0, "=", "struct AbcdefghijKlmnopqrs")),
                  banner(25, "=", "struct AbcdefghijKlmnopqrs"))

    local section = "ErrorReasonNames specialisations"  -- 32
    testing.equal(t, "a long non-class title is centred on the whole title",
                  bdestyle.apply(banner(0, "-", section)),
                  banner(math.floor((79 - 3 - #section) / 2), "-", section))

    -- ------------------------------------- 4.5.1 INLINE DEFINITIONS ---

    local full = "// " .. string.rep("=", 76) .. "\n"
    text = bdestyle.apply("// " .. string.rep("=", 70) .. "\n"
                          .. "//    INLINE DEFINITIONS\n"
                          .. "// " .. string.rep("=", 70) .. "\n")
    l = lines_of(text)
    testing.equal(t, "4.5.1: the banner extends to column 79", #l[1], 79)
    testing.equal(t, "4.5.1: the I of INLINE is in column 29",
                  l[2]:find("INLINE", 1, true), 29)
    testing.equal(t, "both rules match", l[3], l[1])

    local other = full .. "//                             ERROR PROPAGATION\n"
                  .. full
    testing.equal(t, "other page-wide separators are untouched",
                  bdestyle.apply(other), other)

    local marker = "// ------------------------------------------ cost -----\n"
    testing.equal(t, "a single-line section marker is untouched",
                  bdestyle.apply(marker), marker)

    local mixed = "// ====\n// class Error\n// ----\n"
    testing.equal(t, "rules of different kinds are not a banner",
                  bdestyle.apply(mixed), mixed)

    -- -------------------------------------------------- 12.3 headings ---

    local usage = "///Usage\n///-----\n"
    testing.equal(t, "12.3.1: a correct heading is untouched",
                  bdestyle.apply(usage), usage)
    testing.equal(t, "12.3.1: clang-format's space after '///' is removed",
                  bdestyle.apply("/// Usage\n///-----\n"), usage)
    testing.equal(t, "12.3.1: an underline of the wrong length is fixed",
                  bdestyle.apply("/// Usage\n///---\n"), usage)

    -- 12.3.2: one space between dashes, the last under the last character.
    -- An even-length title needs a leading space for that.
    local even = "///Example 1: Printing an Error\n"
                 .. "/// - - - - - - - - - - - - - -\n"
    testing.equal(t, "12.3.2: a sub-heading ends under its last character",
                  bdestyle.apply(even), even)
    testing.equal(t, "12.3.2: clang-format's extra spaces are removed",
                  bdestyle.apply("/// Example 1: Printing an Error\n"
                                 .. "///  - - - - - - - - - - - - - -\n"),
                  even)
    local odd = "///Example 2: Odds\n///- - - - - - - -\n"
    testing.equal(t, "12.3.2: an odd-length title needs no leading space",
                  bdestyle.apply("/// Example 2: Odds\n///- - -\n"), odd)
    testing.equal(t, "headings are stable when run again",
                  bdestyle.apply(bdestyle.apply(even .. odd)), even .. odd)

    local indented = "    ///Thread Safety\n    ///-------------\n"
    testing.equal(t, "an indented heading keeps its indentation",
                  bdestyle.apply("    /// Thread Safety\n    ///----\n"),
                  indented)

    local sentence = "/// Return the name.\n/// - first item\n"
    testing.equal(t, "a sentence followed by a list item is untouched",
                  bdestyle.apply(sentence), sentence)
    local dash = "/// Some text\n/// -\n"
    testing.equal(t, "a one-dash line is not an underline",
                  bdestyle.apply(dash), dash)
    local rule = "// Title\n// -----\n"
    testing.equal(t, "a '//' line is not a documentation heading",
                  bdestyle.apply(rule), rule)

    -- ------------------------------------------------------- files ---

    testing.equal(t, "a missing final newline stays missing",
                  bdestyle.apply("int x;"), "int x;")
    testing.equal(t, "an empty file stays empty", bdestyle.apply(""), "")
end
