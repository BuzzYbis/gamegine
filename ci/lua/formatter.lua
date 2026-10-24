-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- formatter.lua -- the formatting pipeline: clang-format, then bdestyle.
--
-- 'xmake format' and 'xmake ci-format' (check and --fix) both go through
-- format_text(), so what is fixed locally is exactly what CI checks.
-- clang-format alone undoes the BDE tags and banners (see bdestyle.lua):
-- the editor's format-on-save does that while you work, and 'xmake format'
-- before a push puts them back.

import("bdestyle")

-- The clang-format major version the sources are formatted with. Two
-- versions disagree on layout (18 and 23 indent the continuation line of a
-- wrapped call differently, for one), so a mismatched tool reports files as
-- "needs formatting" that are fine. .github/workflows/push.yml installs the
-- same major; change both together.
local CLANG_FORMAT_MAJOR = 23

-- Return the clang-format major version the sources are formatted with.
-- (A function: an xmake module exports its functions, not its variables.)
function required_major()
    return CLANG_FORMAT_MAJOR
end

-- Return nil if the specified 'version' (the text after "version" in
-- 'clang-format --version', such as "23.1.0" or "18.1.3") is the major
-- version the sources are formatted with, and otherwise a message saying
-- what is wrong and how to fix it.
function version_problem(version)
    local major = tonumber((version or ""):match("^(%d+)"))
    if major == CLANG_FORMAT_MAJOR then
        return nil
    end
    return ("clang-format %s found, but the sources are formatted with "
         .. "clang-format %d, and other versions lay code out differently. "
         .. "Install it with 'pip install clang-format==%d.*' or "
         .. "'brew install llvm', and put it first on PATH."):format(
             tostring(version), CLANG_FORMAT_MAJOR, CLANG_FORMAT_MAJOR)
end

-- Format 'text' as the contents of 'filename' and return the result, or
-- nil and a reason. 'filename' only chooses the language by extension; it
-- does not have to exist, and it is never read or written.
function format_text(text, filename)
    local extension = path.extension(filename or "")
    if extension == "" then
        extension = ".cpp"
    end
    local input = os.tmpfile() .. extension
    local output = os.tmpfile()
    io.writefile(input, text)
    local style = "--style=file:" .. path.join(os.projectdir(), ".clang-format")
    local code = os.execv("clang-format", {style, input},
                          {stdout = output, try = true})
    local formatted = code == 0 and io.readfile(output) or nil
    os.tryrm(input)
    os.tryrm(output)
    if formatted == nil then
        return nil, "clang-format failed (exit " .. tostring(code) .. ")"
    end
    return bdestyle.apply(formatted)
end

-- Return the problems in 'text' that formatting cannot fix, each as
-- {line = <number>, text = <the line>, message = <what is wrong>}: the naming
-- rule of bdestyle, which a person has to apply because a rename touches the
-- uses too. 'text' is what format_text() returns, so the line numbers are
-- those of the file once 'xmake format' has run.
function problems(text)
    return bdestyle.problems(text)
end
