-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- sources.lua -- enumerate the first-party sources CI is allowed to police.
--
-- third_party is excluded deliberately. The build already keeps it out of the
-- strict warning policy (the 'imgui' target sets warnings to none), and
-- reformatting vendored code makes every future upstream merge a conflict.

-- Trees that are ours. A tree that does not exist yet is skipped silently,
-- because R0 legitimately starts with most of them empty.
local FIRST_PARTY = {
    "gamegine",
    "tests",
    "benchmarks",
    "tools",
    "shaders",
}

local EXTENSIONS = {"**.cpp", "**.h", "**.hpp", "**.mm", "**.inl"}

-- Translation units only. A header is a first-party source for formatting and
-- for a syntax check, but it is not something that can be compiled and
-- archived: a static library target with zero .cpp files fails at 'ar', not
-- at the compiler. The build job keys off this rather than off the header
-- count, so a headers-only state leaves it inert instead of red.
local TRANSLATION_UNITS = {"**.cpp", "**.mm"}

-- Suffix conventions, so tooling can tell a component apart from its test:
--   <component>.cpp     library source
--   <component>.t.cpp   unit test
--   <component>.b.cpp   benchmark
--   <component>.m.cpp   an executable's main
local SUFFIXES = {test = ".t.cpp", benchmark = ".b.cpp", main = ".m.cpp"}

-- Return the absolute paths of every first-party C++ source and header.
function list(opt)
    opt = opt or {}
    local roots = opt.roots or FIRST_PARTY
    local files = {}
    local seen  = {}
    for _, dir in ipairs(roots) do
        local absolute = path.join(os.projectdir(), dir)
        if os.isdir(absolute) then
            for _, pattern in ipairs(opt.extensions or EXTENSIONS) do
                for _, f in ipairs(os.files(path.join(absolute, pattern))) do
                    if not seen[f] then
                        seen[f] = true
                        table.insert(files, f)
                    end
                end
            end
        end
    end
    table.sort(files)
    return files
end

-- Return the absolute paths of every first-party translation unit.
function translation_units(opt)
    opt = opt or {}
    return list({roots = opt.roots, extensions = TRANSLATION_UNITS})
end

-- Classify a path by its suffix convention: "test", "benchmark", "main" or
-- "source".
function classify(file)
    for kind, suffix in pairs(SUFFIXES) do
        if file:endswith(suffix) then
            return kind
        end
    end
    return "source"
end

-- The configured first-party roots, for reporting.
function roots()
    return FIRST_PARTY
end
