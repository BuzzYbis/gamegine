-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- lualint.lua -- the scripting side of the warning policy.
--
-- The C++ build compiles with -Wall -Wextra -Wunused -Werror. The Lua that
-- drives CI had no equivalent, so an unused parameter or a dead import sat
-- there indefinitely. This is the smallest check that closes that gap: it
-- finds the classes of dead code a reader would flag in review and a human
-- reliably stops noticing.
--
-- Syntax itself needs no check here. Every module is loaded through
-- 'import()' by the tests, and ci/tasks.lua is parsed on every xmake
-- invocation, so a syntax error cannot reach a commit unnoticed.
--
-- Deliberately not a general linter. It reports only findings it can state
-- exactly, because a checker that cries wolf gets disabled, and a disabled
-- checker is worse than none.

-- Globals that plain Lua has and xmake's sandbox does not. Every one of
-- these was discovered the same way: at run time, in a branch that had not
-- executed yet, long after it was written. 'next' hid inside a schema
-- validator until a manifest with an empty array reached it.
--
-- The sandbox is not documented as a list, so this one is empirical: entries
-- are added when a call turns out to be nil at run time.
local SANDBOX_ABSENT = {
    "next", "select", "pcall", "xpcall", "load", "loadstring", "dofile",
    "setfenv", "getfenv", "collectgarbage", "rawlen",
}

-- Lua's frontier pattern gives a real word boundary; a plain substring search
-- would match 'opt' inside 'options' and silently pass.
function _mentions(text, name)
    return text:find("%f[%w_]" .. name:gsub("(%W)", "%%%1") .. "%f[^%w_]")
           ~= nil
end

-- Remove a trailing comment, tracking quotes so that a '--' inside a string
-- is left alone. Without this the checker reports code that is commented out,
-- and a checker that cries wolf gets disabled.
function _strip_comment(line)
    local quote = nil
    local i = 1
    while i <= #line do
        local c = line:sub(i, i)
        if quote then
            if c == "\\" then
                i = i + 1
            elseif c == quote then
                quote = nil
            end
        elseif c == '"' or c == "'" then
            quote = c
        elseif c == "-" and line:sub(i + 1, i + 1) == "-" then
            return line:sub(1, i - 1)
        end
        i = i + 1
    end
    return line
end

function _lines(text)
    local out = {}
    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        table.insert(out, line)
    end
    return out
end

-- Collect the body of a function starting at 'first', ending at the 'end'
-- that sits at the same indentation. Good enough for this codebase, which
-- never indents a top-level function.
function _body(lines, first, indent)
    local body = {}
    for i = first + 1, #lines do
        local line = lines[i]
        local stripped = line:match("^%s*(.-)%s*$")
        local this_indent = #(line:match("^(%s*)") or "")
        if stripped == "end" and this_indent == indent then
            break
        end
        table.insert(body, line)
    end
    return table.concat(body, "\n")
end

function _finding(file, line, message)
    return {file = file, line = line, message = message}
end

-- Check one file and return a list of findings.
function check_file(file)
    local text = io.readfile(file)
    if not text then
        return {}
    end
    local lines = _lines(text)
    local findings = {}
    local relative = path.relative(file, os.projectdir())

    -- Module names bound by import(), so a local shadowing one can be
    -- spotted below.
    local imported = {}
    for name in text:gmatch('import%("([^"]+)"') do
        imported[name:match("([^.]+)$")] = true
    end

    for i, raw_line in ipairs(lines) do
        local line = _strip_comment(raw_line)

        -- Unused function parameters.
        local indent, name, params =
            line:match("^(%s*)function%s+([%w_.:]+)%s*%(([^)]*)%)")
        if name then
            local body = _body(lines, i, #indent)
            for param in params:gmatch("[^,]+") do
                param = param:match("^%s*(.-)%s*$")
                if param ~= "" and param ~= "..."
                   and not _mentions(body, param) then
                    table.insert(findings, _finding(relative, i,
                        ("function %s(): parameter %q is never used")
                        :format(name, param)))
                end
            end
        end

        -- Imports nothing uses. An import is not free: it loads and executes
        -- the module.
        local module = line:match('^%s*import%("([^"]+)"')
        if module then
            local short = module:match("([^.]+)$")
            local rest = table.concat(lines, "\n", 1, i - 1)
                         .. "\n"
                         .. table.concat(lines, "\n", i + 1, #lines)
            if not rest:find("%f[%w_]" .. short .. "%s*[.(]") then
                table.insert(findings, _finding(relative, i,
                    ("imports %q but never uses %q"):format(module, short)))
            end
        end

        -- Calls to globals the sandbox does not provide. A definition of
        -- the same name in this file shadows the global and is fine.
        for _, name in ipairs(SANDBOX_ABSENT) do
            if line:find("%f[%w_]" .. name .. "%s*%(")
               and not line:find("function%s+" .. name)
               and not text:find("function%s+" .. name .. "%s*%(") then
                table.insert(findings, _finding(relative, i,
                    ("calls %q, which xmake's Lua sandbox does not provide; "
                     .. "it will be nil at run time"):format(name)))
            end
        end

        -- A local that shadows an imported module. This one broke
        -- 'ci-format' outright: a 'local style' for a clang-format argument
        -- hid the imported 'style' module for the rest of the function, and
        -- every style.say() after it became a nil call. Silent until the
        -- task ran.
        local declared = line:match("^%s*local%s+([%w_]+)%s*=")
        if declared and imported[declared] then
            table.insert(findings, _finding(relative, i,
                ("local %q shadows the imported module of the same name")
                :format(declared)))
        end

        -- File-scope constants nothing reads.
        local constant = line:match("^local%s+([A-Z_][A-Z0-9_]*)%s*=")
        if constant then
            local rest = table.concat(lines, "\n", 1, i - 1)
                         .. "\n"
                         .. table.concat(lines, "\n", i + 1, #lines)
            if not _mentions(rest, constant) then
                table.insert(findings, _finding(relative, i,
                    ("local %q is never used"):format(constant)))
            end
        end
    end

    return findings
end

-- Definition files are declarations, not code: every stub deliberately has
-- an empty body, so every parameter is deliberately unused. Checking them
-- would report the whole file.
function _is_definition_file(file)
    if file:find("/meta/", 1, true) then
        return true
    end
    local head = (io.readfile(file) or ""):sub(1, 200)
    return head:find("---@meta", 1, true) ~= nil
end

-- Check every first-party Lua file. Returns findings and a boolean.
function check(opt)
    opt = opt or {}
    local roots = opt.roots or {"ci"}
    local files = {}
    for _, root in ipairs(roots) do
        local dir = path.join(os.projectdir(), root)
        if os.isdir(dir) then
            for _, f in ipairs(os.files(path.join(dir, "**.lua"))) do
                if not _is_definition_file(f) then
                    table.insert(files, f)
                end
            end
        end
    end
    table.sort(files)

    local findings = {}
    for _, f in ipairs(files) do
        for _, finding in ipairs(check_file(f)) do
            table.insert(findings, finding)
        end
    end
    return findings, #files
end
