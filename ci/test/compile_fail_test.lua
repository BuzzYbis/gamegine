-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the guarantees the engine makes AT COMPILE TIME: a log message made
-- with the wrong argument, a text with a stray brace, a ring of a capacity
-- that is not a power of two, a literal "{:x}" for an error. A unit test
-- cannot check these, since the code under test does not compile, so nothing
-- else notices if one of these checks quietly stops rejecting.
--
-- Each snippet in ci/test/compile_fail/ has two forms in one file. Compiled
-- as it is, it makes the mistake, and must fail with the reason its first
-- line names. Compiled with -DGOOD, it is the same code without the mistake,
-- and must compile: otherwise the failure could be a typo in the snippet, and
-- the test would pass for the wrong reason.
--
-- Every compiler found is used, because the messages and some of the checks
-- differ between them. One that cannot build the engine's own foundation
-- header is named in the output and left out, since its failures would say
-- nothing about the snippets: clang 18 with libstdc++ 13, the pair on Ubuntu
-- 24.04, has no std::expected, because clang before 19 reports a smaller
-- __cpp_concepts than libstdc++ asks for. The test fails if none is left.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})

local INCLUDE = path.join(os.projectdir(), "gamegine", "include")
local SNIPPETS = path.join(os.projectdir(), "ci", "test", "compile_fail")

-- Return the C++ compilers on this machine, each once: clang++, g++ and the
-- versioned g++-N of a Homebrew or distribution GCC, and 'c++' if it is the
-- only one. A name that is another's under a different guise (g++ is Apple's
-- clang on macOS) is skipped, found by its version line.
local function compilers()
    local found, seen = {}, {}
    local names = {"clang++", "g++", "g++-16", "g++-15", "g++-14", "g++-13",
                   "g++-12", "c++"}
    for _, name in ipairs(names) do
        local info = os.tmpfile()
        local code = os.execv(name, {"--version"},
                              {try = true, stdout = info,
                               stderr = os.nuldev()})
        local first = (io.readfile(info) or ""):match("^[^\n]*")
        os.tryrm(info)
        if code == 0 and first and not seen[first] then
            seen[first] = true
            table.insert(found, name)
        end
    end
    return found
end

-- Compile 'file' with 'compiler', with the valid twin selected if 'good', and
-- return the exit code and the compiler's messages.
local function compile(compiler, file, good)
    local messages = os.tmpfile()
    local argv = {"-std=c++23", "-fsyntax-only", "-fno-exceptions",
                  "-Wall", "-Wextra", "-Werror",
                  "-I", INCLUDE, "-I", SNIPPETS}
    if good then
        table.insert(argv, "-DGOOD")
    end
    table.insert(argv, file)
    local code = os.execv(compiler, argv,
                          {try = true, stdout = os.nuldev(),
                           stderr = messages})
    local text = io.readfile(messages) or ""
    os.tryrm(messages)
    return code, text
end

-- Return whether 'compiler' builds the engine's foundation header, and if it
-- does not, the first line of what it said.
local function builds_engine(compiler)
    local source, messages = os.tmpfile(), os.tmpfile()
    io.writefile(source, "#include <engine/core/core_error.h>\nint main() {}\n")
    local code = os.execv(compiler,
                          {"-std=c++23", "-fsyntax-only", "-x", "c++",
                           "-I", INCLUDE, source},
                          {try = true, stdout = os.nuldev(),
                           stderr = messages})
    local text = io.readfile(messages) or ""
    os.tryrm(source)
    os.tryrm(messages)
    return code == 0, text:match("[^\n]*error:[^\n]*") or text:match("^[^\n]*")
end

function run(t)
    local found = compilers()
    testing.check(t, "a C++ compiler is installed", #found > 0,
                  "tried clang++, g++ and c++")

    local available = {}
    for _, compiler in ipairs(found) do
        local ok, first_line = builds_engine(compiler)
        if ok then
            table.insert(available, compiler)
        else
            print(("       %s is left out: it cannot build core_error.h (%s)")
                  :format(compiler, first_line or "no message"))
        end
    end
    testing.check(t, "a C++ compiler can build the engine's headers",
                  #available > 0 or #found == 0,
                  "none of " .. table.concat(found, ", ") .. " could")

    local snippets = os.files(path.join(SNIPPETS, "*.cpp"))
    table.sort(snippets)
    testing.check(t, "there are snippets to compile", #snippets > 0,
                  SNIPPETS)

    for _, compiler in ipairs(available) do
        for _, file in ipairs(snippets) do
            local name = path.basename(file)
            local label = ("%s, %s"):format(name, compiler)
            local expected = io.readfile(file):match("^// EXPECT%-ERROR: ([^\n]+)")
            testing.check(t, label .. ": names the error it expects",
                          expected ~= nil, "no '// EXPECT-ERROR:' first line")

            local good_code, good_text = compile(compiler, file, true)
            testing.check(t, label .. ": the valid twin compiles",
                          good_code == 0, good_text:sub(1, 400))

            local bad_code, bad_text = compile(compiler, file, false)
            testing.check(t, label .. ": the mistake is rejected",
                          bad_code ~= 0, "it compiled")
            testing.check(t, label .. ": and for the reason given",
                          expected ~= nil
                              and bad_text:find(expected, 1, true) ~= nil,
                          "expected '" .. tostring(expected) .. "' in: "
                              .. bad_text:sub(1, 400))
        end
    end
end
