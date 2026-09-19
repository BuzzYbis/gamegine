-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the schema validator. The run manifest is a contract frozen at R0
-- and every later result is compared against R0's, so a validator that
-- quietly stops rejecting bad documents is worse than none: it grants
-- confidence it has not earned.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("jsonschema", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("core.base.json")

function run(t)
    local schema = {
        type = "object",
        required = {"a", "b"},
        additionalProperties = false,
        properties = {
            a = {type = "string", enum = {"x", "y"}},
            b = {type = "integer", minimum = 1},
            c = {type = {"string", "null"}},
            d = {type = "array", items = {type = "number"}, minItems = 2},
        },
    }

    local ok = jsonschema.validate({a = "x", b = 3, d = {1, 2}}, schema)
    testing.check(t, "a valid document passes", ok)

    local bad, errors = jsonschema.validate({a = "z", b = 0, d = {1},
                                             zz = true}, schema)
    testing.check(t, "an invalid document fails", not bad)
    -- All four problems are reported at once: a manifest with four wrong
    -- fields should cost one CI round trip, not four.
    testing.equal(t, "every problem is reported", #errors, 4)

    local missing, merrors = jsonschema.validate({a = "x"}, schema)
    testing.check(t, "a missing required property fails", not missing)
    testing.check(t, "the missing property is named",
                  tostring(merrors[1]):find("b", 1, true) ~= nil)

    -- A null in a nullable field is accepted; a null in a non-nullable one is
    -- not. benchmarks.md section 6 fills most manifest fields with null at
    -- planning time, so this distinction is load-bearing.
    local nullok = jsonschema.validate({a = "x", b = 1, c = json.null}, schema)
    testing.check(t, "null passes where the type allows it", nullok)
    local nullbad = jsonschema.validate({a = json.null, b = 1}, schema)
    testing.check(t, "null fails where the type does not", not nullbad)

    -- The committed examples must validate against the real schema. This is
    -- the tripwire for an incompatible edit to the frozen contract.
    local schemafile = path.join(os.projectdir(), "ci", "schema",
                                 "run-manifest.schema.json")
    for _, name in ipairs({"run-manifest.documented.json",
                           "run-manifest.unavailable.json"}) do
        local file = path.join(os.projectdir(), "ci", "schema", "examples",
                               name)
        local valid, errs = jsonschema.validate_file(file, schemafile)
        testing.check(t, ("committed example %s validates"):format(name),
                      valid, table.concat(errs or {}, "; "))
    end
end
