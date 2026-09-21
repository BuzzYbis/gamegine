-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- style.lua -- terminal output that is readable on any background.
--
-- Not the terminal's palette, and not xmake's colours. Both delegate the
-- decision: xmake's ${green} is 38;2;0;255;0, pure RGB at full brightness,
-- and the 16-colour codes name a slot the theme resolves however it likes.
-- Either way the result depends on the reader's setup, and on a light theme
-- both produce text you cannot see.
--
-- These are FIXED colours, chosen by measurement rather than by eye. A colour
-- is legible on white and on black only if its relative luminance sits in a
-- narrow band; outside it, one background or the other wins. Each value below
-- was picked for its WORST-CASE WCAG contrast -- the weaker of its two
-- ratios -- because that is what decides whether it can be read at all.
--
--   role   colour     vs white   vs black   worst
--   ok     #2E8B2E      4.33       4.85      4.33
--   bad    #D1242F      5.24       4.00      4.00
--   warn   #9A6700      4.87       4.31      4.31
--   note   #1F6FEB      4.63       4.53      4.53
--   dim    #6E7781      4.55       4.62      4.55
--
-- 4.58 is the ceiling. Contrast against white falls as a colour lightens and
-- contrast against black rises, so the best any colour can manage on both at
-- once is where the two curves cross. Everything here is within 0.3 of it;
-- there is no better choice available, only different hues.
--
-- 'dim' is a colour rather than the ANSI dim attribute (ESC[2m). That
-- attribute lightens the foreground, which on a light theme moves the text
-- toward the background -- exactly the wrong direction.
--
-- Two rules follow:
--
--   o Colour is never the only signal. Every line that means "this failed"
--     also says so in words, so the meaning survives a monochrome terminal,
--     a log file, and a reader who cannot distinguish red from green.
--
--   o Lines always close, so a forgotten reset cannot bleed into the shell.

local CODES = {
    reset = "0",
    bold  = "1",
    ok    = "38;2;46;139;46",    -- #2E8B2E  worst-case contrast 4.33
    bad   = "38;2;209;36;47",    -- #D1242F  worst-case contrast 4.00
    warn  = "38;2;154;103;0",    -- #9A6700  worst-case contrast 4.31
    note  = "38;2;31;111;235",   -- #1F6FEB  worst-case contrast 4.53
    dim   = "38;2;110;119;129",  -- #6E7781  worst-case contrast 4.55
}

-- Honour NO_COLOR (https://no-color.org) and dumb terminals. A caller that
-- pipes output into a file or a pager gets plain text.
function _enabled()
    if os.getenv("NO_COLOR") then
        return false
    end
    if os.getenv("TERM") == "dumb" then
        return false
    end
    return true
end

-- Substitute ${ok}, ${bad}, ${warn}, ${note}, ${bold}, ${dim} and ${reset}
-- with escape sequences, or with nothing when colour is disabled.
function render(text)
    local colour = _enabled()
    return (tostring(text):gsub("%${(%w+)}", function (name)
        local code = CODES[name]
        if not code then
            return "${" .. name .. "}"
        end
        if not colour then
            return ""
        end
        return "\27[" .. code .. "m"
    end))
end

-- Format, colour and print a line. Named "say" rather than "printf"
-- because the sandbox already has a printf and the import would be shadowed.
function say(fmt, ...)
    local args = {...}
    local text = (#args > 0) and tostring(fmt):format(...) or tostring(fmt)
    local rendered = render(text)

    -- Always close the line. A call site that forgets ${reset} would
    -- otherwise bleed its colour into everything printed afterwards --
    -- including the shell prompt once the command exits. Making this
    -- automatic is cheaper than remembering it at 67 call sites.
    if _enabled() and rendered:find("\27[", 1, true) then
        rendered = rendered .. "\27[0m"
    end
    print(rendered)
end

-- Map a status word to its role, so every task colours the same status the
-- same way. Colour reinforces the word; it never replaces it.
local STATUS_ROLE = {
    OK                = "ok",
    PASS              = "ok",
    PINNED_VERIFIED   = "ok",
    FAIL              = "bad",
    MISMATCH          = "bad",
    UNPINNED          = "bad",
    PROBE_UNAVAILABLE = "bad",
    UNAVAILABLE       = "warn",
    UNVERIFIABLE_HERE = "warn",
    PINNED_RECORDED   = "warn",
    NOT_IMPLEMENTED   = "warn",
    NOT_RUN           = "dim",
    NOT_YET_DUE       = "dim",
}

-- Wrap a status word in its role's colour, padded to 'width' so columns line
-- up whatever the status. Returns text ready for say().
function statusword(text, width)
    local role = STATUS_ROLE[tostring(text)] or "dim"
    return ("${%s}%-" .. tostring(width or 0) .. "s${reset}")
           :format(role, tostring(text))
end

-- Colour a boolean by what it means: true is a pass, false is not.
function flag(value)
    return value and "${ok}true${reset}" or "${warn}false${reset}"
end

-- A status word with a fixed width, so columns line up whatever the status.
-- The word carries the meaning; the colour only reinforces it.
function status(kind, word)
    local token = CODES[kind] and kind or "note"
    return ("${%s}%-7s${reset}"):format(token, word or kind)
end
