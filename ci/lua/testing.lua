-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- testing.lua -- a very small assertion helper for the CI Lua tests.
--
-- Deliberately not a framework. These tests guard parsers and validators that
-- have to keep working on machines nobody is looking at, and the whole point
-- is that they are cheap enough that nobody deletes them.

function new(name)
    return {name = name, passed = 0, failures = {}}
end

function check(t, label, condition, detail)
    if condition then
        t.passed = t.passed + 1
    else
        table.insert(t.failures, {label = label, detail = detail or ""})
    end
end

function equal(t, label, actual, expected)
    check(t, label, actual == expected,
          ("expected %s, got %s"):format(tostring(expected), tostring(actual)))
end

-- Asserts a value is absent. Distinguished from equal(nil) because "absent"
-- and "zero" must never be confused in a measurement.
function absent(t, label, actual)
    check(t, label, actual == nil,
          ("expected absent, got %s"):format(tostring(actual)))
end
