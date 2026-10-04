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
