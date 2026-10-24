-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- coverage.lua -- source-based coverage of first-party code.
--
-- Builds the unit tests a second time with clang's source-based coverage
-- (-fprofile-instr-generate -fcoverage-mapping), runs them, merges the raw
-- profile, and asks llvm-cov about gamegine/ only -- never about GoogleTest
-- or the standard library, whose numbers would drown the ones that matter.
--
-- It is a separate build rather than a flag on the normal one on purpose.
-- The instrumented clang and llvm-cov must come from the SAME LLVM, because
-- the profile format changes between releases; on macOS that means
-- Homebrew's LLVM, not Apple's clang, which is what the regular build uses.
--
-- A number here says every line RAN, not that every result was CHECKED. The
-- mutation testing behind the unit tests is the stronger evidence; this is a
-- map of what nothing exercises at all.

import("probe")
import("sources")

-- Where an LLVM toolchain with clang, llvm-profdata and llvm-cov may live.
-- PATH first, so Linux distributions and CI images work as they are.
local LLVM_DIRS = {"", "/opt/homebrew/opt/llvm/bin/", "/usr/local/opt/llvm/bin/"}

-- The code whose coverage is reported.
local REPORTED = {"gamegine/include", "gamegine/src"}

-- Return the output directory for coverage artefacts.
function outdir()
    return path.join(os.projectdir(), "build", "coverage")
end

-- Locate one LLVM installation that has all three tools. Mixing clang from
-- one release with llvm-cov from another produces an unreadable profile, so
-- the three are taken from the same directory or not at all.
function _find_llvm()
    for _, dir in ipairs(LLVM_DIRS) do
        local ok = true
        for _, tool in ipairs({"clang++", "llvm-profdata", "llvm-cov"}) do
            if probe.run(dir .. tool, {"--version"}).status ~= "OK" then
                ok = false
                break
            end
        end
        if ok then
            return {
                cxx      = dir .. "clang++",
                profdata = dir .. "llvm-profdata",
                cov      = dir .. "llvm-cov",
            }
        end
    end
    return nil
end

-- Locate the GoogleTest xmake installed: headers and the two libraries.
function _find_gtest()
    local root = os.getenv("XMAKE_GLOBALDIR")
                 or path.join(os.getenv("HOME") or "", ".xmake")
    local headers = os.files(path.join(root, "packages", "g", "gtest", "*",
                                       "*", "include", "gtest", "gtest.h"))
    if #headers == 0 then
        return nil
    end
    local include = path.directory(path.directory(headers[1]))
    local libdir = path.join(path.directory(include), "lib")
    if not os.isfile(path.join(libdir, "libgtest.a")) then
        return nil
    end
    return {include = include, libdir = libdir}
end

-- Build the instrumented tests, run them and produce a merged profile.
--
-- Returns a context table for report() and html(), or nil and a reason. The
-- reason is written for the person running it: coverage that could not be
-- measured is reported as unavailable, never as a number.
function measure()
    local llvm = _find_llvm()
    if not llvm then
        return nil, "no LLVM toolchain with clang++, llvm-profdata and "
                    .. "llvm-cov in one place (tried PATH, "
                    .. "/opt/homebrew/opt/llvm/bin, /usr/local/opt/llvm/bin). "
                    .. "On macOS: brew install llvm"
    end

    local gtest = _find_gtest()
    if not gtest then
        return nil, "GoogleTest is not installed yet; run 'xmake ci-unit' "
                    .. "once so xmake fetches it"
    end

    local tests = {}
    for _, unit in ipairs(sources.translation_units()) do
        local kind = sources.classify(unit)
        if kind == "test" or kind == "source" then
            table.insert(tests, unit)
        end
    end
    local has_test = false
    for _, unit in ipairs(tests) do
        if sources.classify(unit) == "test" then
            has_test = true
        end
    end
    if not has_test then
        return nil, "no .t.cpp files yet, so nothing exercises the code"
    end

    local dir = outdir()
    os.tryrm(dir)
    os.mkdir(dir)
    local binary  = path.join(dir, "tests")
    local raw     = path.join(dir, "tests.profraw")
    local merged  = path.join(dir, "tests.profdata")

    local argv = {"-std=c++23", "-fno-exceptions", "-O0", "-g",
                  "-fprofile-instr-generate", "-fcoverage-mapping",
                  "-DGTEST_HAS_EXCEPTIONS=0",
                  "-I", path.join(os.projectdir(), "gamegine", "include"),
                  "-I", gtest.include}
    for _, unit in ipairs(tests) do
        table.insert(argv, unit)
    end
    for _, a in ipairs({"-L", gtest.libdir, "-lgtest_main", "-lgtest",
                        "-o", binary}) do
        table.insert(argv, a)
    end

    local build = probe.run(llvm.cxx, argv,
                            {stream = "both", lines = true,
                             allow_empty = true, timeout = 600000})
    if build.status ~= "OK" then
        return nil, "the instrumented build failed: " .. (build.reason or "")
    end

    -- The profile is written at exit, to the path in LLVM_PROFILE_FILE.
    local code = os.execv(binary, {"--gtest_brief=1"},
                          {envs = {LLVM_PROFILE_FILE = raw},
                           stdout = os.tmpfile(), stderr = os.tmpfile(),
                           try = true, timeout = 600000})
    if code ~= 0 then
        return nil, "the instrumented tests failed (exit "
                    .. tostring(code) .. "); run 'xmake ci-unit' to see which"
    end
    if not os.isfile(raw) then
        return nil, "the tests ran but wrote no profile"
    end

    local merge = probe.run(llvm.profdata,
                            {"merge", "-sparse", raw, "-o", merged},
                            {allow_empty = true, stream = "both"})
    if merge.status ~= "OK" then
        return nil, "llvm-profdata merge failed: " .. (merge.reason or "")
    end

    return {llvm = llvm, binary = binary, profile = merged, dir = dir}
end

-- The llvm-cov arguments shared by every query: which binary, which profile,
-- and which sources to report on.
function _query(context, subcommand, extra)
    local argv = {subcommand, context.binary,
                  "-instr-profile=" .. context.profile}
    for _, a in ipairs(extra or {}) do
        table.insert(argv, a)
    end
    for _, dir in ipairs(REPORTED) do
        local full = path.join(os.projectdir(), dir)
        if os.isdir(full) then
            table.insert(argv, full)
        end
    end
    return argv
end

-- Return the per-file summary table as text.
function report(context)
    local record = probe.run(context.llvm.cov, _query(context, "report"),
                             {lines = true, stream = "stdout"})
    if record.status ~= "OK" then
        return nil, record.reason
    end
    return record.value
end

-- Write the annotated HTML report and return the path of its index page.
function html(context)
    local out = path.join(context.dir, "html")
    local record = probe.run(context.llvm.cov,
                             _query(context, "show",
                                    {"-format=html", "-show-branches=count",
                                     "-show-line-counts-or-regions",
                                     "-output-dir=" .. out}),
                             {allow_empty = true, stream = "both"})
    if record.status ~= "OK" then
        return nil, record.reason
    end
    local index = path.join(out, "index.html")
    if not os.isfile(index) then
        return nil, "llvm-cov wrote no index.html"
    end
    return index
end
