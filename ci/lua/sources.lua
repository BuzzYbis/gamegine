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

-- Return the absolute paths of every first-party C++ source and header.
function list(opt)
    opt = opt or {}
    local roots = opt.roots or FIRST_PARTY
    local files = {}
    local seen  = {}
    for _, dir in ipairs(roots) do
        local absolute = path.join(os.projectdir(), dir)
        if os.isdir(absolute) then
            for _, pattern in ipairs(EXTENSIONS) do
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

-- The configured first-party roots, for reporting.
function roots()
    return FIRST_PARTY
end
