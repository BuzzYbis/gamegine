-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- probe.lua -- run external commands and report what happened honestly.
--
-- Every probe returns a *record*, never a bare value:
--
--     {value = <any>, status = "OK", source = "<the command>"}
--     {value = nil, status = "UNAVAILABLE", reason = "...", source = "..."}
--
-- This shape exists because of one rule that runs through the whole plan:
-- an unavailable measurement is never a pass (ci.md section 4 and section 9).
-- A field that could not be probed is recorded as UNAVAILABLE with the reason
-- and the exact command that failed. It is never omitted, never defaulted and
-- never filled in from a spec sheet.

-- Every probe is bounded. A probe that can block forever can hang a nightly
-- job forever, and ci.md section 2 requires jobs to be interruptible and to
-- clean up. This is not hypothetical: 'glxinfo -B' blocks indefinitely when
-- no display server answers, which is the normal state of a headless runner.
--
-- Ten seconds is far longer than any probe here legitimately needs; it exists
-- to turn a hang into a reported UNAVAILABLE, not to race a slow machine.
local DEFAULT_TIMEOUT_MS = 10000

-- Return the first line of a possibly multi-line string, trimmed.
function _firstline(text)
    if not text then
        return ""
    end
    local line = text:match("^([^\r\n]*)") or ""
    return line:trim()
end

-- Render a command as a reproducible one-line string for the 'source' field.
function command_string(program, argv)
    local parts = {program}
    for _, a in ipairs(argv or {}) do
        table.insert(parts, a)
    end
    return table.concat(parts, " ")
end

-- Build an OK record carrying the specified 'value', produced by 'source'.
function ok(value, source)
    return {value = value, status = "OK", source = source}
end

-- Build an UNAVAILABLE record. 'reason' says why, in words a reader can act
-- on; 'source' is the command that was attempted.
function unavailable(reason, source)
    return {value = nil, status = "UNAVAILABLE", reason = reason,
            source = source}
end

-- Run 'program' with 'argv' and capture its output.
--
-- opt.stream    "stdout" (default), "stderr" or "both". Several version
--               probes -- slangc -v among them -- report on stderr.
-- opt.lines     when true the whole captured text is returned; otherwise
--               only the first line.
-- opt.accept    a table of exit codes to treat as success, default {0}.
--               Some tools report a version and exit non-zero.
-- opt.timeout   milliseconds before the child is killed, default 10000.
-- opt.allow_empty  treat empty output as success rather than UNAVAILABLE.
--
-- Never raises and never blocks indefinitely: a missing program, a non-zero
-- exit, or a timeout all become UNAVAILABLE with the reason.
function run(program, argv, opt)
    opt = opt or {}
    argv = argv or {}
    local source = command_string(program, argv)

    local timeout = opt.timeout or DEFAULT_TIMEOUT_MS
    local outfile, errfile = os.tmpfile(), os.tmpfile()
    local code
    try
    {
        function ()
            code = os.execv(program, argv,
                            {stdout = outfile, stderr = errfile, try = true,
                             timeout = timeout})
        end,
        catch
        {
            function (errors)
                code = nil
            end
        }
    }

    local out = io.readfile(outfile) or ""
    local err = io.readfile(errfile) or ""
    os.tryrm(outfile)
    os.tryrm(errfile)

    if code == nil then
        return unavailable("program not found or not executable", source)
    end

    -- os.execv returns -1 when it killed the child at the timeout.
    if code == -1 then
        return unavailable(
            ("timed out after %dms and was killed"):format(timeout), source)
    end

    local accepted = false
    for _, c in ipairs(opt.accept or {0}) do
        if code == c then
            accepted = true
            break
        end
    end
    if not accepted then
        local detail = _firstline(err ~= "" and err or out)
        if detail == "" then
            detail = "no diagnostic output"
        end
        return unavailable(("exit code %d: %s"):format(code, detail), source)
    end

    local text = out
    if opt.stream == "stderr" then
        text = err
    elseif opt.stream == "both" then
        text = out .. err
    end

    if not opt.lines then
        text = _firstline(text)
    else
        text = (text or ""):trim()
    end

    -- Silence is failure for a version probe and success for a linter, so
    -- the caller decides. clang-tidy prints nothing when it finds nothing.
    if text == "" and not opt.allow_empty then
        return unavailable("command succeeded but produced no output", source)
    end
    return ok(text, source)
end

-- Run 'program' and return the first capture of 'pattern' applied to its
-- output, or UNAVAILABLE when the pattern does not match. Use this rather
-- than post-processing a run() record, so that a tool whose output format
-- changed is reported as a failed probe instead of a wrong value.
function match(program, argv, pattern, opt)
    local record = run(program, argv, opt)
    if record.status ~= "OK" then
        return record
    end
    local captured = record.value:match(pattern)
    if not captured then
        return unavailable(
            ("output did not match %s"):format(pattern), record.source)
    end
    return ok(captured:trim(), record.source)
end

-- Read a file and return its trimmed first line as a record.
function readfile(path)
    if not os.isfile(path) then
        return unavailable("file does not exist", path)
    end
    local text = io.readfile(path)
    if not text or text:trim() == "" then
        return unavailable("file is empty", path)
    end
    return ok(_firstline(text), path)
end

-- True when the record carries a usable value.
function available(record)
    return record ~= nil and record.status == "OK" and record.value ~= nil
end
