-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- bdestyle.lua -- the BDE layout rules clang-format cannot express.
--
-- From doc/CodingStandards(fromBDE_almost).pdf:
--
--   11.6  A return other than at a function's closing brace is marked
--         '// RETURN', right-justified to column 79, at least two spaces
--         after the statement, on its last line. If the line has no room,
--         the tag goes on a line of its own, still right-justified.
--   7.4.2 A constructor that is deliberately implicit is marked
--         '// IMPLICIT'. It is placed exactly like '// RETURN'.
--   6.1.3 A class banner is indented by 25 spaces if the class name is
--   6.8.3 shorter than 20 characters, and centred otherwise; its rules are
--         exactly as long as its title. '=' rules for the definition, '-'
--         for the implementation. Other banners follow the same rule,
--         judged on their whole title.
--   4.5.1 The INLINE DEFINITIONS banner extends to column 79, with the 'I'
--         of INLINE in column 29.
--   12.3  A documentation heading is '///Heading' with no space, underlined
--         by '///' and a dash per character; each deeper level puts one
--         more space between the dashes, the last dash under the last
--         character of the heading.
--
-- clang-format undoes all of these on every run. That was tested against
-- clang-format 23 (the CLI) and Apple's 21 (the editor) with every
-- relevant option -- 'AlignTrailingComments: Kind: Leave', 'CommentPragmas',
-- 'ReflowComments: Never' -- and none keeps the tag columns in either, nor
-- the banner indentation in 23. So 'xmake format' and 'xmake ci-format' run
-- clang-format and then this pass (formatter.lua). The pair is idempotent:
-- clang-format moves these lines, this pass puts them back. The editor's
-- format-on-save runs clang-format alone and undoes them while you work;
-- 'xmake format' before a push restores them, and CI checks it did.
--
-- apply() is a pure function on text, so it is tested without files.
--
-- One rule here has no rewrite, because renaming a member means renaming its
-- uses too, and a script cannot see them all. problems() reports it:
--
--   Naming (a deviation from the standard). A data member that is a pointer,
--         or owns one, ends in '_p': 'd_producer_p' for a 'Producer*',
--         'd_shared_p' for a 'std::unique_ptr<Producer>'. The standard keeps
--         '_p' for raw pointers; here a smart pointer takes it too, so that
--         '->' on a member shows an indirection whatever the type. Class
--         data ('s_') follows the same rule. Views, optionals, arrays and
--         containers of pointers are not pointers and take no suffix.

local LAST_COLUMN   = 79
local BANNER_INDENT = 25    -- spaces before '//' (Rule 6.1.3)
local BANNER_SHORT  = 20    -- names shorter than this get BANNER_INDENT
local INLINE_COLUMN = 29    -- column of the 'I' in INLINE (Rule 4.5.1)
local MIN_GAP       = 2     -- spaces before a tag (Rule 11.6.3)

-- The trailing tags this pass places.
local TAGS = {RETURN = true, IMPLICIT = true}

-- Split 'text' into lines, and say whether it ended with a newline.
function _split(text)
    local lines = {}
    local start = 1
    while true do
        local stop = text:find("\n", start, true)
        if not stop then
            if start <= #text then
                table.insert(lines, text:sub(start))
            end
            break
        end
        table.insert(lines, text:sub(start, stop - 1))
        start = stop + 1
    end
    return lines, text:sub(-1) == "\n"
end

-- ------------------------------------------------------------ tags ---

-- Return the code and the tag of a line ending with a tag after code.
function _trailing(line)
    local code, tag = line:match("^(.-%S)%s+// (%u+)%s*$")
    if code and TAGS[tag] and not code:match("^%s*//") then
        return code, tag
    end
    return nil
end

-- Return the tag of a line holding nothing but a tag.
function _alone(line)
    local tag = line:match("^%s*// (%u+)%s*$")
    if tag and TAGS[tag] then
        return tag
    end
    return nil
end

-- Return true if 'code' has room for '// <tag>' after it.
function _fits(code, tag)
    return #code + MIN_GAP + 3 + #tag <= LAST_COLUMN
end

-- Return 'code' with '// <tag>' right-justified to column 79.
function _attach(code, tag)
    return code .. string.rep(" ", LAST_COLUMN - #code - 3 - #tag)
           .. "// " .. tag
end

-- Return a line holding only '// <tag>', right-justified to column 79.
function _tag_line(tag)
    return string.rep(" ", LAST_COLUMN - 3 - #tag) .. "// " .. tag
end

-- Return true if a tag on the next line may move up onto 'line': it ends
-- a statement or declaration and carries no comment of its own.
function _can_take(line)
    return line:match(";%s*$") ~= nil and not line:find("//", 1, true)
end

-- -------------------------------------------------------- banners ---

-- Return the rule of a '// ====' or '// ----' line, or nil.
function _rule(line)
    local body = line:match("^%s*// ([=%-]+)$")
    if body and (body:match("^=+$") or body:match("^%-+$")) then
        return body
    end
    return nil
end

-- Return the title of a banner's middle line: text right after '// ',
-- with no leading space -- which keeps page-wide separators out.
function _title(line)
    local title = line:match("^%s*// (%S.*)$")
    if title and not _rule(line) then
        return (title:gsub("%s+$", ""))
    end
    return nil
end

-- Return the indentation of a banner whose title is 'title'.
function _banner_indent(title)
    local name = title:match("^class%s+(.+)$")
                 or title:match("^struct%s+(.+)$")
                 or title:match("^union%s+(.+)$")
                 or title
    local width = 3 + #title
    if #name < BANNER_SHORT and BANNER_INDENT + width <= LAST_COLUMN then
        return BANNER_INDENT
    end
    return math.max(0, math.floor((LAST_COLUMN - width) / 2))
end

-- If lines[i..i+2] form a banner, return its three rewritten lines.
function _banner(lines, i)
    local rule = _rule(lines[i])
    local title = lines[i + 1] and _title(lines[i + 1])
    local closing = lines[i + 2] and _rule(lines[i + 2])
    if not rule or not title or closing ~= rule then
        return nil
    end
    -- A rule reaching column 79 belongs to a page-wide separator.
    if #lines[i] >= LAST_COLUMN then
        return nil
    end
    local lead = string.rep(" ", _banner_indent(title)) .. "// "
    local line = lead .. string.rep(rule:sub(1, 1), #title)
    return {line, lead .. title, line}
end

-- If lines[i..i+2] form the INLINE DEFINITIONS banner, return it rewritten.
function _inline_banner(lines, i)
    local rule = _rule(lines[i])
    local closing = lines[i + 2] and _rule(lines[i + 2])
    if not rule or closing ~= rule or not rule:match("^=") then
        return nil
    end
    if not lines[i + 1]:match("^%s*//%s+INLINE DEFINITIONS%s*$") then
        return nil
    end
    local line = "// " .. string.rep("=", LAST_COLUMN - 3)
    return {line,
            "//" .. string.rep(" ", INLINE_COLUMN - 3) .. "INLINE DEFINITIONS",
            line}
end

-- ------------------------------------------------------- headings ---

-- Return the gap between the dashes of a '///' underline -- 0 for a solid
-- line, 1 for '- - -', and so on -- or nil if 'line' is not an underline.
-- Three dashes at least, so that a '/// -' list item is never one.
function _underline_gap(line)
    local body = line:match("^%s*///%s*(%-[%- ]*)$")
    if not body then
        return nil
    end
    body = body:gsub("%s+$", "")
    local gap = #(body:match("^%-( *)") or "")
    local _, count = body:gsub("%-", "")
    local dashes = {}
    for d = 1, count do
        dashes[d] = "-"
    end
    if count < 3 or body ~= table.concat(dashes, string.rep(" ", gap)) then
        return nil
    end
    return gap
end

-- If lines[i] and lines[i+1] form a documentation heading, return both
-- lines rewritten: no space after '///' (clang-format adds one), and an
-- underline ending under the heading's last character.
function _heading(lines, i)
    local indent, title = lines[i]:match("^(%s*)///%s*(%S.-)%s*$")
    local gap = lines[i + 1] and _underline_gap(lines[i + 1])
    if not title or not gap or title:match("^[%- ]+$") then
        return nil
    end
    local marks = {}
    for column = 1, #title do
        local from_end = #title - column
        marks[column] = from_end % (gap + 1) == 0 and "-" or " "
    end
    return {indent .. "///" .. title,
            indent .. "///" .. table.concat(marks)}
end

-- ---------------------------------------------------------- naming ---

-- Words that come before a type in a declaration without being part of it.
local SPECIFIERS = {
    static = true, inline = true, mutable = true, thread_local = true,
    constexpr = true, const = true, volatile = true,
}

-- Words that start a statement that is not a declaration.
local STATEMENTS = {
    ["return"] = true, ["else"] = true, ["goto"] = true, delete = true,
    throw = true, case = true, new = true, co_return = true,
    co_yield = true, sizeof = true, using = true, typedef = true,
}

local SMART_POINTERS = {"std::unique_ptr", "std::shared_ptr", "std::weak_ptr"}

-- Return 'declared', the text in front of a member's name, without the
-- specifiers that open it, or nil if it opens something that is not a
-- declaration ('return', 'delete').
function _type_of(declared)
    local rest = declared
    while true do
        local word, tail = rest:match("^(%S+)%s*(.*)$")
        if STATEMENTS[word] then
            return nil
        end
        if SPECIFIERS[word] and tail ~= "" then
            rest = tail
        else
            break
        end
    end
    return rest
end

-- Return true if 'type' is a pointer or a smart pointer, and not something
-- that holds some: '*' ends it, or a smart pointer template spans all of it.
-- Only the characters of a type are allowed, so that '"size * d_count;"' and
-- 'x = y * d_z;' are not read as declarations.
function _is_pointer(type)
    if not type:match("^[%w_:<>,%s%*&]+$") then
        return false
    end
    local bare = type
    repeat
        local before = bare
        bare = bare:gsub("%s*const$", ""):gsub("%s*volatile$", "")
    until bare == before
    if bare:match("%*$") then
        return true
    end
    for _, smart in ipairs(SMART_POINTERS) do
        if bare:match("^" .. smart:gsub("%p", "%%%0") .. "%s*<.*>$") then
            return true
        end
    end
    return bare:match("^std::atomic%s*<[^<>]*%*%s*>$") ~= nil
end

-- Return the problems in 'text' that no rewrite can fix, in order, each as
-- {line = <number>, text = <the line>, message = <what is wrong>}. Empty
-- when there are none. A member is a line of the form '<type> d_name;',
-- 'd_name = ...;' or 'd_name{...};' ('s_' for class data); one that does not
-- fit a single line is not seen.
function problems(text)
    local result = {}
    local number = 0
    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        number = number + 1
        -- Comments need no case of their own: the '//' of one falls in front
        -- of whatever it mentions, and no type has a '/' in it.
        local declared, name = line:match(
            "^%s*(.-%S)%s+([ds]_[%w_]+)%s*[;={]")
        local type = declared and _type_of(declared)
        if type and _is_pointer(type) and not name:match("_p$") then
            table.insert(result, {
                line = number,
                text = line,
                message = ("pointer data member '%s' takes a '_p' suffix: "
                           .. "'%s_p'"):format(name, name),
            })
        end
    end
    return result
end

-- ---------------------------------------------------------- apply ---

-- Apply every rule to 'text' and return the new text. Every case has an
-- automatic fix -- a tag with no room moves to a line of its own -- so
-- there is nothing to report.
function apply(text)
    local lines, newline = _split(text)
    local out = {}
    local i = 1
    while i <= #lines do
        local block = _inline_banner(lines, i) or _banner(lines, i)
        local heading = not block and _heading(lines, i)
        if block then
            for _, l in ipairs(block) do
                table.insert(out, l)
            end
            i = i + 3
        elseif heading then
            table.insert(out, heading[1])
            table.insert(out, heading[2])
            i = i + 2
        else
            local line = lines[i]
            local code, tag = _trailing(line)
            local alone = not code and _alone(line)
            if code then
                if _fits(code, tag) then
                    table.insert(out, _attach(code, tag))
                else
                    -- Rule 11.6.3: no room, so a line of its own.
                    table.insert(out, code)
                    table.insert(out, _tag_line(tag))
                end
            elseif alone then
                local previous = out[#out]
                if previous and _can_take(previous)
                   and _fits(previous, alone) then
                    out[#out] = _attach(previous, alone)
                else
                    table.insert(out, _tag_line(alone))
                end
            else
                table.insert(out, line)
            end
            i = i + 1
        end
    end
    return table.concat(out, "\n") .. (newline and "\n" or "")
end
