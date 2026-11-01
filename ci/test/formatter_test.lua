-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the clang-format version check of 'ci-format'. A runner with the
-- wrong clang-format reports correct files as unformatted; the check turns
-- that into one message saying which version is wrong and how to get the
-- right one.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("formatter", {rootdir = path.join(os.projectdir(), "ci", "lua")})

function run(t)
    local wanted = formatter.required_major()

    -- ---------------------------------------------- accepted versions ---

    testing.absent(t, "the pinned major version is accepted",
                   formatter.version_problem(wanted .. ".1.0"))
    testing.absent(t, "any minor and patch of it is accepted",
                   formatter.version_problem(wanted .. ".0.7"))

    -- ---------------------------------------------- rejected versions ---

    -- Ubuntu 24.04's apt package, the version that made CI fail.
    local problem = formatter.version_problem("18.1.3")
    testing.check(t, "an older major version is rejected", problem ~= nil)
    testing.check(t, "the message names the version found",
                  problem ~= nil and problem:find("18.1.3", 1, true) ~= nil,
                  tostring(problem))
    testing.check(t, "the message names the version wanted",
                  problem ~= nil
                      and problem:find("clang-format " .. wanted, 1, true)
                          ~= nil,
                  tostring(problem))
    testing.check(t, "the message says how to install it",
                  problem ~= nil and problem:find("pip install", 1, true)
                      ~= nil,
                  tostring(problem))

    testing.check(t, "a newer major version is rejected too",
                  formatter.version_problem((wanted + 1) .. ".0.0") ~= nil)
    testing.check(t, "a version that is not a number is rejected",
                  formatter.version_problem("unknown") ~= nil)
    testing.check(t, "a missing version is rejected",
                  formatter.version_problem(nil) ~= nil)
    testing.check(t, "an empty version is rejected",
                  formatter.version_problem("") ~= nil)
    testing.check(t, "a major version that merely starts with the wanted "
                  .. "digits is rejected",
                  formatter.version_problem(wanted .. "0.1.0") ~= nil)
end
