-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- shaders.lua -- the Slang toolchain: compile, classify, validate and hash.
--
-- D3 makes Slang the single shader source language, and R0 freezes how it is
-- built (r0-architecture.md sections 4 and 5). Every kernel under shaders/
-- is compiled ahead of time, once per target; nothing is compiled at run
-- time. This module is the one implementation, behind two entry points:
--
--   xmake build shaders   the 'gamegine.slang' rule in xmake.lua. Incremental,
--                         and what anything that loads kernels depends on.
--   xmake ci-shaders      the CI task. A full compile that needs no package
--                         resolution, so it runs on a bare hosted runner.
--
-- Two implementations that could disagree is how the artifact CI checked
-- stops being the artifact the harness loads.
--
-- The directory a kernel lives in decides what a failure means
-- (r0-benchmarks.md section 4):
--
--   baseline/   Required. Must compile for every target; any failure is a
--               build error (gate B0.1).
--   probes/     Conditional capability probes. "Unsupported is a valid
--               result, not a failure" (gate B0.2): a probe a target rejects
--               is recorded UNSUPPORTED with the diagnostics, and the build
--               goes on.
--   common/     Modules imported by kernels. Never compiled on their own.
--
-- Per-target outcomes:
--
--   COMPILED     every stage accepted it; the artifact exists
--   UNSUPPORTED  probes only: the kernel declares the target unsupported, or
--                Apple's toolchain rejected the MSL
--   UNVALIDATED  MSL emitted, but Apple's toolchain is not available here.
--                Not a pass: nothing has shown the GPU would accept it
--   FAILED       anything else. A build error, for probes too
--
-- The escape hatch (architecture.md, D3): when Apple rejects the MSL Slang
-- emits for a kernel, a hand-written <kernel>.metal beside <kernel>.slang
-- replaces it for the MSL target only. It must say why on a '//@escape'
-- line. The Slang attempt stays in the record, the Vulkan side stays in
-- Slang, and an escape Slang no longer needs is an error: the failure mode
-- to avoid is a slow drift back into two hand-written shader sets.

import("core.base.bytes")
import("core.base.json")
import("core.project.config")
import("lib.detect.find_program")
import("pins")
import("probe")

-- Where kernels live, relative to the project root.
local SOURCE_ROOT = "shaders"

-- Top-level directory under the source root -> kernel class.
local CLASSES = {
    baseline = "baseline",
    probes   = "probe",
    common   = "module",
}

-- The targets, keyed by the name used everywhere else: the 'shader-targets'
-- option, the output tree, the index and the CI matrix.
--
-- 'artifact' is what the backend loads, and what the shader hash covers. For
-- MSL that is the metallib, not the text Slang emits: it is where Apple's
-- toolchain has had its say, and it is byte-identical wherever it is built
-- from the same MSL (the .air in between is not; it embeds the input path).
local TARGETS = {
    spirv = {
        slang    = "spirv",
        backend  = "vulkan",
        emits    = "spv",
        artifact = "spv",
        -- One module holds every entry point of a kernel, and the harness
        -- looks them up by name. Without this a kernel with a single entry
        -- point has it renamed 'main'.
        flags    = {"-fvk-use-entrypoint-name"},
    },
    msl = {
        slang    = "metal",
        backend  = "metal",
        emits    = "metal",
        artifact = "metallib",
        flags    = {},
    },
}
local TARGET_ORDER = {"spirv", "msl"}

-- Flags every kernel is compiled with. They are part of the toolchain
-- contract: recorded in the index, and changing one changes the shader hash.
local SLANG_FLAGS = {
    -- The capability profile is declared in the source: every entry point
    -- carries one [require(<target>, ...)] per target it supports, and this
    -- makes Slang hold the body to it. The check runs whatever target is
    -- being compiled, so a call Metal cannot honour fails the SPIR-V build
    -- on Linux as well.
    --
    -- No -profile is passed. Under this flag a profile turns every
    -- capability beyond it into an error unless it is repeated on the
    -- command line, which would move the declaration out of the source.
    "-restrictive-capability-check",
    -- The shader side of the -Werror policy.
    "-warnings-as-errors", "all",
    -- math.typ section 1: column-major storage, column vectors.
    "-matrix-layout-column-major",
}

-- Apple's compiler. The MSL version is explicit because the atomic results
-- B0 records must name it (r0-architecture.md section 6). The deployment
-- target is explicit because it otherwise follows the host OS, and the
-- metallib bytes -- so the shader hash -- would depend on which macOS ran
-- the build. MSL 3.1 needs macOS 14.
local MSL_STD   = "metal3.1"
local MACOS_MIN = "14.0"
local METAL_FLAGS = {"-std=" .. MSL_STD, "-mmacosx-version-min=" .. MACOS_MIN}

-- Slang diagnostics that mean "this entry point does not support this
-- target", and nothing else. 36107 is what an entry point whose [require]
-- set leaves the target out produces. An undeclared capability (36104), a
-- declared target that a dependency cannot meet (36108) or a syntax error is
-- a defect in the kernel, and FAILED even for a probe. Kept narrow on
-- purpose: a broken probe must not be able to pass as an unsupported one.
local DECLARED_UNSUPPORTED = {[36107] = true}

-- ------------------------------------------------------------ targets ---

function _target_rank(name)
    for i, n in ipairs(TARGET_ORDER) do
        if n == name then
            return i
        end
    end
    return #TARGET_ORDER + 1
end

-- The targets built when none are configured: what this host's backend
-- loads. SPIR-V for Vulkan on Linux, MSL for Metal on macOS.
function default_targets()
    return os.host() == "macosx" and {"msl"} or {"spirv"}
end

-- Parse a comma-separated target list. Empty means the host default.
function parse_targets(value)
    if value == nil or value == "" then
        return default_targets()
    end
    local list, seen = {}, {}
    for _, name in ipairs(value:split(",")) do
        name = name:trim()
        if not TARGETS[name] then
            raise("unknown shader target '%s'; known targets are %s", name,
                  table.concat(TARGET_ORDER, ", "))
        end
        if not seen[name] then
            seen[name] = true
            table.insert(list, name)
        end
    end
    table.sort(list, function (a, b)
        return _target_rank(a) < _target_rank(b)
    end)
    return list
end

-- The target whose artifacts a backend loads, or nil.
function target_for_backend(backend)
    for _, name in ipairs(TARGET_ORDER) do
        if TARGETS[name].backend == backend then
            return name
        end
    end
    return nil
end

-- ------------------------------------------------------------ kernels ---

-- Describe the kernel in 'sourcefile'. opt.root is the source root, default
-- shaders/. Returns nil and a reason for a file outside baseline/, probes/
-- and common/: its directory decides whether a failure blocks, so a kernel
-- anywhere else would have no defined meaning.
function kernel(sourcefile, opt)
    opt = opt or {}
    local projectdir = os.projectdir()
    local root = path.absolute(opt.root or SOURCE_ROOT, projectdir)
    local file = path.absolute(sourcefile, projectdir)
    local relative = path.relative(file, root):gsub("\\", "/")
    local dir = relative:match("^([^/]+)/")
    local class = dir and CLASSES[dir]
    if not class then
        return nil, ("%s is not under baseline/, probes/ or common/ of %s")
                    :format(path.relative(file, projectdir),
                            path.relative(root, projectdir))
    end
    return {
        name   = (relative:gsub("%.slang$", "")),
        class  = class,
        -- Passed to slangc relative to the project root, which is also its
        -- working directory. Slang writes the path it was given into the
        -- MSL's #line directives; an absolute one would make the output,
        -- and every hash over it, depend on where the checkout lives.
        source = path.relative(file, projectdir):gsub("\\", "/"),
        root   = path.relative(root, projectdir):gsub("\\", "/"),
    }
end

-- Every kernel under the source root, sorted by name, without the modules;
-- plus the reasons for any .slang file that is not where a kernel may be.
function discover(opt)
    opt = opt or {}
    local root = path.absolute(opt.root or SOURCE_ROOT, os.projectdir())
    local kernels, misplaced = {}, {}
    for _, file in ipairs(os.files(path.join(root, "**.slang"))) do
        local k, reason = kernel(file, opt)
        if not k then
            table.insert(misplaced, reason)
        elseif k.class ~= "module" then
            table.insert(kernels, k)
        end
    end
    table.sort(kernels, function (a, b) return a.name < b.name end)
    return kernels, misplaced
end

-- ------------------------------------------------ capability profiles ---

-- Blank out comments, keeping every newline so line numbers survive.
function _strip_comments(text)
    local out = {}
    local i, n = 1, #text
    local in_string = false
    while i <= n do
        local c = text:sub(i, i)
        if in_string then
            table.insert(out, c)
            if c == "\\" then
                table.insert(out, text:sub(i + 1, i + 1))
                i = i + 1
            elseif c == "\"" then
                in_string = false
            end
            i = i + 1
        elseif c == "\"" then
            in_string = true
            table.insert(out, c)
            i = i + 1
        elseif text:sub(i, i + 1) == "//" then
            local stop = text:find("\n", i, true) or (n + 1)
            table.insert(out, string.rep(" ", stop - i))
            i = stop
        elseif text:sub(i, i + 1) == "/*" then
            local close = text:find("*/", i + 2, true)
            local stop = close and (close + 1) or n
            table.insert(out, (text:sub(i, stop):gsub("[^\n]", " ")))
            i = stop + 1
        else
            table.insert(out, c)
            i = i + 1
        end
    end
    return table.concat(out)
end

-- Index of the bracket matching the one at 'pos', scanning in 'step'
-- direction (1 forward from '[', -1 backward from ']'), or nil.
function _match_bracket(text, pos, step)
    local depth = 0
    local i = pos
    while i >= 1 and i <= #text do
        local c = text:sub(i, i)
        if c == "[" then
            depth = depth + step
        elseif c == "]" then
            depth = depth - step
        end
        if depth == 0 then
            return i
        end
        i = i + step
    end
    return nil
end

-- The entry points in 'text' whose attributes carry no [require(...)], as a
-- list of {name, line}.
--
-- Without a declaration Slang infers the capability set from the body, and
-- a Metal-only or Vulkan-only call surfaces only when that target is
-- compiled -- on the other machine, possibly much later. The declaration is
-- what makes the check run everywhere, so its absence is itself an error.
-- An entry point is a function carrying a [shader("<stage>")] attribute.
function undeclared_entry_points(text)
    text = _strip_comments(text)
    local found = {}
    local from = 1
    while true do
        local s = text:find("%[%s*shader%s*%(", from)
        if not s then
            break
        end
        local close = _match_bracket(text, s, 1)
        if not close then
            break
        end
        from = close + 1

        -- The attribute block is every bracketed group adjacent to the
        -- [shader(...)] one, before and after it.
        local groups = {text:sub(s, close)}
        local i = s - 1
        while i >= 1 do
            local ws = text:sub(1, i):find("%s*$")
            i = ws - 1
            if i < 1 or text:sub(i, i) ~= "]" then
                break
            end
            local open = _match_bracket(text, i, -1)
            if not open then
                break
            end
            table.insert(groups, text:sub(open, i))
            i = open - 1
        end
        local j = close + 1
        while j <= #text do
            j = text:find("%S", j) or (#text + 1)
            if text:sub(j, j) ~= "[" then
                break
            end
            local stop = _match_bracket(text, j, 1)
            if not stop then
                break
            end
            table.insert(groups, text:sub(j, stop))
            j = stop + 1
        end

        local declared = false
        for _, g in ipairs(groups) do
            if g:find("^%[%s*require%s*%(") then
                declared = true
                break
            end
        end
        if not declared then
            local name = text:match("([%a_][%w_]*)%s*%(", j) or "?"
            local _, newlines = text:sub(1, s):gsub("\n", "")
            table.insert(found, {name = name, line = newlines + 1})
        end
    end
    return found
end

-- ---------------------------------------------------------- diagnostics ---

-- The numeric ids of Slang's error diagnostics in 'text', in order.
function diagnostic_ids(text)
    local ids = {}
    for id in (text or ""):gmatch("error%s+(%d+):") do
        table.insert(ids, tonumber(id))
    end
    return ids
end

-- True when every error in 'ids' means the kernel declares the target
-- unsupported. An empty list is not: a failure with no diagnostic has no
-- reason attached, and a missing reason is never evidence.
function declares_unsupported(ids)
    if #ids == 0 then
        return false
    end
    for _, id in ipairs(ids) do
        if not DECLARED_UNSUPPORTED[id] then
            return false
        end
    end
    return true
end

-- Parse a Makefile-style dependency file, as 'slangc -depfile' writes it:
-- "<output>: <dep> <dep> ...", with '\' continuing lines and escaping spaces.
function parse_depfile(text)
    local deps = {}
    if not text then
        return deps
    end
    text = text:gsub("\\\r?\n", " ")
    local _, colon = text:find(":%s")
    if not colon then
        return deps
    end
    local current = {}
    local i = colon + 1
    while i <= #text do
        local c = text:sub(i, i)
        if c == "\\" and text:sub(i + 1, i + 1) == " " then
            table.insert(current, " ")
            i = i + 2
        else
            if c:match("%s") then
                if #current > 0 then
                    table.insert(deps, table.concat(current))
                    current = {}
                end
            else
                table.insert(current, c)
            end
            i = i + 1
        end
    end
    if #current > 0 then
        table.insert(deps, table.concat(current))
    end
    return deps
end

-- ------------------------------------------------------------ toolchain ---

-- Where the build writes: <builddir>/shaders.
function outputdir()
    return path.absolute(path.join(config.builddir() or "build", "shaders"),
                         os.projectdir())
end

-- Locate the programs the targets need and record their versions. Every
-- field is a probe record, so a missing tool is UNAVAILABLE with its reason
-- rather than an absent key.
--
-- slangc is looked up on PATH, exactly as 'xmake ci-pins' finds it, so the
-- compiler that is pinned and the compiler that builds are the same one.
function toolchain(targets)
    local tc = {targets = targets}

    -- find_program's default check runs '--version', which slangc rejects.
    -- 'force' skips its cache: that cache keeps a miss too, and provenance
    -- must describe this machine now, not whenever it was first probed.
    tc.slangc_program = find_program("slangc", {check = "-v", force = true})
    local spec = pins.read().pinned.slang
    if tc.slangc_program then
        tc.slangc = probe.match(tc.slangc_program, spec.probe.args,
                                spec.probe.pattern,
                                {stream = spec.probe.stream})
    else
        tc.slangc = probe.unavailable("slangc is not on PATH. It ships "
                                      .. "with the Vulkan SDK; ci/pins.json "
                                      .. "names the version", "slangc -v")
    end
    tc.slang_pin = spec.version
    tc.slang_pin_matches = probe.available(tc.slangc)
                           and tc.slangc.value == spec.version

    local apple = false
    for _, name in ipairs(targets) do
        apple = apple or TARGETS[name].artifact == "metallib"
    end
    if apple then
        tc.msl_std   = MSL_STD
        tc.macos_min = MACOS_MIN
        if os.host() ~= "macosx" then
            local reason = "Apple's metal and metallib run only on macOS. "
                           .. "The MSL is emitted here but only a macOS "
                           .. "host can show it compiles"
            tc.metal    = probe.unavailable(reason, "xcrun -sdk macosx metal")
            tc.metallib = probe.unavailable(reason,
                                            "xcrun -sdk macosx metallib")
        else
            -- On a runner without the Metal Toolchain component this fails
            -- with Xcode's own instructions, which become the reason.
            tc.metal = probe.run("xcrun", {"-sdk", "macosx", "metal",
                                           "--version"})
            tc.metallib = probe.run("xcrun", {"-sdk", "macosx", "-f",
                                              "metallib"})
            tc.macos_sdk = probe.run("xcrun", {"-sdk", "macosx",
                                               "--show-sdk-version"})
        end
    end
    return tc
end

-- Raise unless 'tc' can compile at all. A missing compiler is a broken
-- machine, not a kernel result, and must not be reported as one.
function require_toolchain(tc)
    if not tc.slangc_program or not probe.available(tc.slangc) then
        raise("slangc is not usable: %s (%s)", tc.slangc.reason or "?",
              tc.slangc.source or "?")
    end
end

-- True when Apple's toolchain can validate MSL on this machine.
function can_validate_msl(tc)
    return probe.available(tc.metal) and probe.available(tc.metallib)
end

-- What a compile's output depends on besides its input files. The rule
-- rebuilds when any of it changes: a compiler bump is a new baseline
-- (ci.md section 6), so its output must not be reused.
function fingerprint(targetname, tc)
    local values = {targetname, tc.slangc and tc.slangc.value or "",
                    table.concat(SLANG_FLAGS, " "),
                    table.concat(TARGETS[targetname].flags, " ")}
    if TARGETS[targetname].artifact == "metallib" then
        table.insert(values, tc.metal and tc.metal.value or "")
        table.insert(values, table.concat(METAL_FLAGS, " "))
    end
    return values
end

-- ------------------------------------------------------------- compiling ---

-- The files one kernel produces for one target. Artifacts and reflection sit
-- in <outputdir>/<target>/; records, dependency files and the .air
-- intermediate in <outputdir>/.work/<target>/.
function paths(k, targetname, dir)
    dir = dir or outputdir()
    local t = TARGETS[targetname]
    local base = path.join(dir, targetname, k.name)
    local work = path.join(dir, ".work", targetname, k.name)
    return {
        emitted    = base .. "." .. t.emits,
        artifact   = base .. "." .. t.artifact,
        reflection = base .. ".reflection.json",
        record     = work .. ".json",
        depfile    = work .. ".d",
        air        = work .. ".air",
    }
end

-- The hand-written MSL escape for kernel 'k': the absolute path of
-- <kernel>.metal beside its source, or nil when there is none.
function escape_file(k)
    local file = path.absolute(k.source:gsub("%.slang$", ".metal"),
                               os.projectdir())
    return os.isfile(file) and file or nil
end

-- The reason an escape gives on its '//@escape <reason>' line, or nil.
function escape_reason(text)
    local reason = (text or ""):match("//@escape[ \t]+([^\n]+)")
    return reason and reason:trim() or nil
end

-- Where an escape's '<name> [[buffer(n)]]' parameters disagree with the
-- bindings Slang reflected for the same kernel. A loader binds by the Slang
-- names and indices, so an escape that numbers its buffers differently
-- would read one buffer as another without any error. Every reflected
-- buffer must appear at least once, at its reflected index.
function escape_binding_mismatches(text, bindings)
    local mismatches = {}
    text = _strip_comments(text or "")
    for _, b in ipairs(bindings or {}) do
        local seen = false
        local pattern = "%f[%w_]" .. b.name
                        .. "%s*%[%[%s*buffer%s*%(%s*(%d+)%s*%)%s*%]%]"
        for index in text:gmatch(pattern) do
            seen = true
            if tonumber(index) ~= b.index then
                table.insert(mismatches,
                    ("'%s' is buffer(%s) in the escape but buffer(%d) in "
                     .. "Slang's reflection"):format(b.name, index, b.index))
            end
        end
        if not seen then
            table.insert(mismatches,
                ("'%s' (buffer(%d) in Slang's reflection) is not a "
                 .. "[[buffer]] parameter of the escape"):format(b.name,
                                                                 b.index))
        end
    end
    return mismatches
end

-- The input files of a compile: the source, plus whatever it imported the
-- last time it was compiled, plus its MSL escape for the MSL target. A new
-- import is itself an edit to the source, so the list from the previous
-- compile is always sufficient.
function inputs(k, targetname, dir)
    local files = {path.absolute(k.source, os.projectdir())}
    local depfile = paths(k, targetname, dir).depfile
    if os.isfile(depfile) then
        for _, dep in ipairs(parse_depfile(io.readfile(depfile))) do
            if dep ~= files[1] then
                table.insert(files, dep)
            end
        end
    end
    local escape = TARGETS[targetname].artifact == "metallib"
                   and escape_file(k)
    if escape then
        table.insert(files, escape)
    end
    return files
end

-- Run one toolchain step from the project root and capture everything.
function _step(tool, program, argv)
    local outfile, errfile = os.tmpfile(), os.tmpfile()
    local code
    try
    {
        function ()
            code = os.execv(program, argv, {curdir = os.projectdir(),
                                            stdout = outfile,
                                            stderr = errfile, try = true})
        end,
        catch
        {
            function ()
                code = nil
            end
        }
    }
    local output = ((io.readfile(outfile) or "")
                    .. (io.readfile(errfile) or "")):trim()
    os.tryrm(outfile)
    os.tryrm(errfile)
    return {
        tool    = tool,
        command = probe.command_string(program, argv),
        exit    = code or -1,
        output  = output ~= "" and output or nil,
    }
end

function _relative(file, dir)
    return (path.relative(file, dir):gsub("\\", "/"))
end

-- What a loader needs from Slang's reflection: the entry points, with the
-- thread-group size where Slang reports one, and the binding of every
-- global parameter by name. A harness binds by name through this table
-- rather than by counting declarations, so reordering a kernel's buffers
-- cannot silently swap them. Slang reports no stage and no thread-group
-- size for mesh entry points; the loader must know those itself.
function _reflection(reflectionfile)
    local entries  = json.mark_as_array({})
    local bindings = json.mark_as_array({})
    if not os.isfile(reflectionfile) then
        return entries, bindings
    end
    local reflection = json.loadfile(reflectionfile)
    for _, ep in ipairs(reflection.entryPoints or {}) do
        local threads = ep.threadGroupSize
        table.insert(entries, {name = ep.name, stage = ep.stage,
                               threads = threads and json.mark_as_array(
                                   table.join({}, threads)) or nil})
    end
    for _, p in ipairs(reflection.parameters or {}) do
        local b = p.binding or {}
        table.insert(bindings, {name = p.name, kind = b.kind,
                                index = b.index, space = b.space})
    end
    return entries, bindings
end

-- Put the MSL at 'emitted' through Apple's metal and metallib. Returns the
-- failing tool's name, or nil when the metallib was produced.
function _apple(record, emitted, files)
    local metal = _step("metal", "xcrun",
        table.join({"-sdk", "macosx", "metal"}, METAL_FLAGS,
                   {"-c", emitted, "-o", files.air}))
    table.insert(record.steps, metal)
    if metal.exit ~= 0 then
        return "metal"
    end
    local metallib = _step("metallib", "xcrun",
        {"-sdk", "macosx", "metallib", files.air, "-o", files.artifact})
    table.insert(record.steps, metallib)
    if metallib.exit ~= 0 then
        return "metallib"
    end
    return nil
end

function _finish(record, status, files, fields)
    record.status = status
    for key, value in pairs(fields or {}) do
        record[key] = value
    end
    os.mkdir(path.directory(files.record))
    io.writefile(files.record, json.encode(record))
    return record
end

-- Compile kernel 'k' for 'targetname'. Returns the record, which is also
-- written next to the other build state so the index can be assembled
-- without recompiling what did not change.
--
-- opt.toolchain  the result of toolchain(); required
-- opt.outputdir  default outputdir()
--
-- Never raises for a kernel problem: the record says what happened, and the
-- caller decides what a FAILED record does to the build.
function compile(k, targetname, opt)
    local tc = opt.toolchain
    local dir = opt.outputdir or outputdir()
    local t = TARGETS[targetname]
    local files = paths(k, targetname, dir)
    local record = {
        kernel = k.name,
        class  = k.class,
        source = k.source,
        target = targetname,
        steps  = json.mark_as_array({}),
    }

    -- A failed compile must never leave the previous run's artifact in
    -- place, looking current.
    for _, key in ipairs({"emitted", "artifact", "reflection", "air",
                          "depfile"}) do
        os.tryrm(files[key])
    end
    os.mkdir(path.directory(files.emitted))
    os.mkdir(path.directory(files.record))

    local text = io.readfile(path.absolute(k.source, os.projectdir())) or ""
    local undeclared = undeclared_entry_points(text)
    if #undeclared > 0 then
        local names = {}
        for _, e in ipairs(undeclared) do
            table.insert(names, ("%s (line %d)"):format(e.name, e.line))
        end
        return _finish(record, "FAILED", files, {
            rejected_by = "capability-declaration",
            reason = "entry point(s) without a [require(...)] capability "
                     .. "declaration: " .. table.concat(names, ", ")
                     .. ". Declare one [require(<target>, ...)] per target "
                     .. "the entry point supports (shaders/README.md)"})
    end

    local argv = {k.source, "-target", t.slang, "-o", files.emitted,
                  "-depfile", files.depfile,
                  "-reflection-json", files.reflection, "-I", k.root}
    table.join2(argv, SLANG_FLAGS)
    table.join2(argv, t.flags)
    local slang = _step("slangc", tc.slangc_program, argv)
    table.insert(record.steps, slang)
    local escape = t.artifact == "metallib" and escape_file(k)
    if slang.exit ~= 0 and escape then
        -- An escape replaces MSL that Slang emitted and Apple rejected. With
        -- no Slang MSL there is nothing to replace, and the escape would be a
        -- second, unchecked source for the kernel.
        return _finish(record, "FAILED", files, {
            rejected_by = "slangc",
            reason = ("slangc emitted no MSL, so the escape %s has nothing "
                      .. "to replace; fix the kernel or delete the escape")
                     :format(_relative(escape, os.projectdir()))})
    end
    if slang.exit ~= 0 then
        if declares_unsupported(diagnostic_ids(slang.output)) then
            return _finish(record,
                           k.class == "probe" and "UNSUPPORTED" or "FAILED",
                           files, {
                rejected_by = "slangc",
                reason = k.class == "probe"
                    and ("the kernel's capability declaration excludes %s")
                        :format(targetname)
                    or ("a baseline kernel must support every target, and "
                        .. "this one's capability declaration excludes %s "
                        .. "(gate B0.1)"):format(targetname)})
        end
        return _finish(record, "FAILED", files, {
            rejected_by = "slangc",
            reason = "slangc rejected the kernel"})
    end

    local deps = json.mark_as_array({})
    for _, dep in ipairs(parse_depfile(io.readfile(files.depfile))) do
        table.insert(deps, _relative(dep, os.projectdir()))
    end
    record.dependencies = deps
    record.entry_points, record.bindings = _reflection(files.reflection)
    record.reflection   = _relative(files.reflection, dir)

    if t.artifact ~= t.emits then
        record.emitted = {path   = _relative(files.emitted, dir),
                          sha256 = hash.sha256(files.emitted)}
        local escape = escape_file(k)
        if escape then
            record.escape = {source = _relative(escape, os.projectdir()),
                             sha256 = hash.sha256(escape),
                             reason = escape_reason(io.readfile(escape))}
        end
        if not can_validate_msl(tc) then
            local missing = probe.available(tc.metal) and tc.metallib
                            or tc.metal
            return _finish(record, "UNVALIDATED", files,
                           {reason = missing.reason})
        end

        local rejected_by = _apple(record, files.emitted, files)
        if not rejected_by and escape then
            return _finish(record, "FAILED", files, {
                rejected_by = "escape",
                reason = ("Apple accepts the MSL Slang emits for this "
                          .. "kernel now, so its escape %s is no longer "
                          .. "needed; delete it"):format(record.escape.source)})
        end
        if rejected_by and escape then
            -- The Slang attempt is kept: that Slang could not do it is
            -- itself a result (B0.2), and the reason the escape exists.
            record.slang_result = {
                status = k.class == "probe" and "UNSUPPORTED" or "FAILED",
                rejected_by = rejected_by,
                reason = ("Apple's %s rejected the MSL Slang emitted")
                         :format(rejected_by)}
            if not record.escape.reason then
                return _finish(record, "FAILED", files, {
                    rejected_by = "escape",
                    reason = ("%s gives no '//@escape <reason>' line; an "
                              .. "escape is an exception and must say why")
                             :format(record.escape.source)})
            end
            local mismatches = escape_binding_mismatches(
                io.readfile(escape), record.bindings)
            if #mismatches > 0 then
                return _finish(record, "FAILED", files, {
                    rejected_by = "escape",
                    reason = ("%s does not bind like the Slang kernel: %s")
                             :format(record.escape.source,
                                     table.concat(mismatches, "; "))})
            end
            os.tryrm(files.air)
            rejected_by = _apple(record, escape, files)
            if rejected_by then
                rejected_by = rejected_by .. " (escape)"
            end
        end
        if rejected_by then
            return _finish(record,
                           k.class == "probe" and "UNSUPPORTED" or "FAILED",
                           files, {
                rejected_by = rejected_by,
                reason = ("Apple's %s rejected the MSL")
                         :format(rejected_by)})
        end
    end

    return _finish(record, "COMPILED", files, {
        artifact = _relative(files.artifact, dir),
        sha256   = hash.sha256(files.artifact)})
end

-- A one-paragraph account of a record that did not compile, for a build log.
function describe(record)
    local lines = {("%s [%s] %s: %s"):format(record.kernel, record.target,
                                             record.status,
                                             record.reason or "")}
    for _, s in ipairs(record.steps or {}) do
        if s.exit ~= 0 then
            table.insert(lines, "  $ " .. s.command)
            if s.output then
                table.insert(lines, (s.output:gsub("\n", "\n    ")))
            end
        end
    end
    return table.concat(lines, "\n")
end

-- --------------------------------------------------------------- hashing ---

-- The shader hash of one target: sha256 over a canonical listing of every
-- kernel with the sha256 of the artifact the backend loads, or UNSUPPORTED.
-- It changes when any loaded byte changes, when a kernel is added, removed
-- or renamed, and when a probe flips between compiled and unsupported.
--
-- There is no hash while any kernel is FAILED, UNVALIDATED or missing: that
-- set was not loadable, and a hash that claims otherwise is a pass nobody
-- earned. The value is always per target, because a run loads one target's
-- set and its manifest must record exactly that one.
function hash_records(targetname, records)
    local source = "ci/lua/shaders.lua hash_records()"
    if #records == 0 then
        return probe.unavailable("no kernels to hash; shaders/ has none yet",
                                 source)
    end
    local sorted = table.join({}, records)
    table.sort(sorted, function (a, b) return a.kernel < b.kernel end)
    local lines = {"gamegine shader set v1", "target " .. targetname}
    for _, r in ipairs(sorted) do
        if r.status == "COMPILED" then
            table.insert(lines, r.kernel .. " " .. r.sha256)
        elseif r.status == "UNSUPPORTED" then
            table.insert(lines, r.kernel .. " UNSUPPORTED")
        else
            return probe.unavailable(
                ("%s is %s, so the %s set is not what a backend could load")
                :format(r.kernel, r.status, targetname), source)
        end
    end
    return probe.ok(hash.sha256(bytes(table.concat(lines, "\n") .. "\n")),
                    source)
end

-- Recompute every target's shader hash from the artifact bytes on disk,
-- rather than from the digests the index recorded. Returns target -> probe
-- record. A hash that disagrees with the index means an artifact changed
-- after the build that listed it, and is UNAVAILABLE: the index and the
-- files no longer describe the same set.
function rehash(result, dir)
    dir = dir or outputdir()
    local hashes = {}
    for _, t in ipairs(result.targets) do
        local records = {}
        local missing
        for _, entry in ipairs(result.kernels) do
            local r = table.copy(entry.results[t])
            if r.status == "COMPILED" then
                local file = path.join(dir, r.artifact)
                if os.isfile(file) then
                    r.sha256 = hash.sha256(file)
                else
                    missing = missing or file
                end
            end
            table.insert(records, r)
        end
        local h = hash_records(t, records)
        local recorded = result.shader_hash[t]
        if missing then
            h = probe.unavailable(("%s is listed in the index but missing; "
                                   .. "rebuild the shaders"):format(missing),
                                  h.source)
        elseif h.status == "OK" and recorded and recorded.value ~= h.value then
            h = probe.unavailable("the artifacts changed after the index "
                                  .. "was written; rebuild the shaders",
                                  h.source)
        end
        hashes[t] = h
    end
    return hashes
end

-- ----------------------------------------------------------------- index ---

function _load_record(k, targetname, dir)
    local file = paths(k, targetname, dir).record
    if not os.isfile(file) then
        return nil
    end
    local record = json.loadfile(file)
    -- A record left by a kernel that has since moved directory, or by a
    -- different source root, describes something else.
    if record.source ~= k.source then
        return nil
    end
    return record
end

-- Assemble the index of the compiled set from the records on disk: every
-- kernel, every target, the toolchain that built them and the shader hash
-- per target. This file is what the harness reads to find kernels, and what
-- a run manifest's shader_hash is taken from.
function index(kernels, tc, opt)
    opt = opt or {}
    local dir = opt.outputdir or outputdir()
    local result = {
        schema_version = 1,
        generator      = opt.generator,
        generated_at   = os.date("!%Y-%m-%dT%H:%M:%SZ"),
        host           = os.host(),
        source_root    = opt.root or SOURCE_ROOT,
        targets        = tc.targets,
        toolchain      = {
            slangc            = tc.slangc,
            slang_pin         = tc.slang_pin,
            slang_pin_matches = tc.slang_pin_matches,
            metal             = tc.metal,
            metallib          = tc.metallib,
            macos_sdk         = tc.macos_sdk,
            msl_std           = tc.msl_std,
            macos_min         = tc.macos_min,
        },
        flags = {slangc = SLANG_FLAGS, metal = METAL_FLAGS, targets = {}},
        shader_hash = {},
        -- Kernels shipping hand-written MSL, by class. architecture.md
        -- section 10: more than one baseline escape means D3's cost model
        -- is wrong. A probe's escape measures the hardware apart from
        -- Slang, and is reported but not counted against it.
        escapes = {baseline = json.mark_as_array({}),
                   probe    = json.mark_as_array({})},
        kernels = json.mark_as_array({}),
    }

    local bytarget = {}
    for _, name in ipairs(tc.targets) do
        bytarget[name] = {}
        result.flags.targets[name] = json.mark_as_array(
            table.join({}, TARGETS[name].flags))
    end
    for _, k in ipairs(kernels) do
        local entry = {name = k.name, class = k.class, source = k.source,
                       results = {}}
        for _, name in ipairs(tc.targets) do
            local record = _load_record(k, name, dir) or {
                kernel = k.name, class = k.class, source = k.source,
                target = name, status = "NOT_BUILT",
                reason = "no compile record; the build stopped before this "
                         .. "kernel"}
            entry.results[name] = record
            table.insert(bytarget[name], record)
            if record.escape and record.status == "COMPILED" then
                table.insert(result.escapes[k.class], k.name)
            end
        end
        table.insert(result.kernels, entry)
    end
    for _, name in ipairs(tc.targets) do
        result.shader_hash[name] = hash_records(name, bytarget[name])
    end
    return result
end

function indexfile(dir)
    return path.join(dir or outputdir(), "index.json")
end

function write_index(result, dir)
    local file = indexfile(dir)
    os.mkdir(path.directory(file))
    io.writefile(file, json.encode(result))
    return file
end

function load_index(dir)
    local file = indexfile(dir)
    if not os.isfile(file) then
        return nil, ("%s does not exist; build the shaders first "
                     .. "('xmake build shaders' or 'xmake ci-shaders')")
                    :format(file)
    end
    return json.loadfile(file)
end

-- Remove what the index does not list from the target directories it
-- covers, so that a deleted or renamed kernel's artifact cannot be picked up
-- by anything that walks the tree.
function prune(result, dir)
    dir = dir or outputdir()
    -- Every file a listed kernel may own, the record included: the record is
    -- how an unchanged kernel gets indexed without being recompiled.
    local keep = {}
    for _, entry in ipairs(result.kernels) do
        for name, _ in pairs(entry.results) do
            for _, file in pairs(paths({name = entry.name}, name, dir)) do
                keep[path.absolute(file)] = true
            end
        end
    end
    local removed = 0
    for _, name in ipairs(result.targets) do
        for _, root in ipairs({path.join(dir, name),
                               path.join(dir, ".work", name)}) do
            for _, file in ipairs(os.files(path.join(root, "**"))) do
                if not keep[path.absolute(file)] then
                    os.rm(file)
                    removed = removed + 1
                end
            end
        end
    end
    return removed
end

-- ---------------------------------------------------------------- report ---

-- Count records by class and status: summary[class][status] = n.
function summarize(result)
    local summary = {}
    for _, entry in ipairs(result.kernels) do
        for _, r in pairs(entry.results) do
            summary[entry.class] = summary[entry.class] or {}
            local s = summary[entry.class]
            s[r.status] = (s[r.status] or 0) + 1
        end
    end
    return summary
end

-- The shader hash a run manifest records: the hash of the set its backend
-- loads, recomputed from the artifacts. Returns a probe record, UNAVAILABLE
-- with the reason when there is none to give. This is what a manifest
-- writer calls to fill shader_hash.
function manifest_hash(backend, dir)
    local source = indexfile(dir)
    local targetname = target_for_backend(backend)
    if not targetname then
        return probe.unavailable(
            ("backend '%s' loads no compiled shader set"):format(
                tostring(backend)), source)
    end
    local result, err = load_index(dir)
    if not result then
        return probe.unavailable(err, source)
    end
    local record = rehash(result, dir)[targetname]
    if not record then
        return probe.unavailable(
            ("the %s target was not built; configure it with "
             .. "'xmake f --shader-targets=%s'"):format(targetname,
                                                        targetname), source)
    end
    return record
end
