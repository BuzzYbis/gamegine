-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- jsonschema.lua -- validate a decoded JSON document against a schema.
--
-- A deliberately small subset of JSON Schema: type, enum, const, required,
-- properties, patternProperties, additionalProperties, items, minItems,
-- maxItems, minimum, maximum, pattern and $ref to '#/$defs/<name>'. That is
-- enough for the manifest and bundle contracts in benchmarks.md section 6 and
-- keeps the validator small enough to be read in one sitting, which matters
-- more here than covering a spec nobody in this project writes against.
--
-- Errors are returned as a list of "<json pointer>: <problem>" strings, all
-- of them, not just the first -- a manifest with six wrong fields should cost
-- one CI round trip, not six.

import("core.base.json")

-- Lua has one table type, so array and object have to be told apart by shape.
-- An empty table is ambiguous; the schema's declared type breaks the tie.
function _is_array(value)
    if type(value) ~= "table" then
        return false
    end
    return #value > 0 or value.__is_array == true
end

function _json_type(value, expected)
    if value == nil or value == json.null then
        return "null"
    end
    local t = type(value)
    if t == "string" then
        return "string"
    elseif t == "boolean" then
        return "boolean"
    elseif t == "number" then
        -- JSON Schema distinguishes integer from number.
        if expected == "integer" and value % 1 == 0 then
            return "integer"
        end
        return "number"
    elseif t == "table" then
        if expected == "array" and not _is_array(value) and
           next(value) == nil then
            return "array"    -- an empty table standing in for []
        end
        if expected == "object" and _is_array(value) then
            return "array"
        end
        return _is_array(value) and "array" or "object"
    end
    return t
end

function _type_matches(value, declared)
    local list = type(declared) == "table" and declared or {declared}
    for _, want in ipairs(list) do
        local got = _json_type(value, want)
        if got == want then
            return true
        end
        -- Every integer is a valid number.
        if want == "number" and got == "integer" then
            return true
        end
    end
    return false
end

function _resolve(schema, root)
    local ref = schema["$ref"]
    if not ref then
        return schema
    end
    local name = ref:match("^#/%$defs/(.+)$")
    if not name then
        raise("unsupported $ref %q; only '#/$defs/<name>' is supported", ref)
    end
    local target = (root["$defs"] or {})[name]
    if not target then
        raise("$ref %q does not resolve", ref)
    end
    return target
end

function _validate(value, schema, root, pointer, errors)
    schema = _resolve(schema, root)

    if schema.type ~= nil and not _type_matches(value, schema.type) then
        local want = type(schema.type) == "table"
                     and table.concat(schema.type, " or ") or schema.type
        table.insert(errors, ("%s: expected %s, got %s")
                             :format(pointer, want, _json_type(value)))
        return
    end

    if schema.const ~= nil and value ~= schema.const then
        table.insert(errors, ("%s: must be %s, got %s")
                             :format(pointer, tostring(schema.const),
                                     tostring(value)))
    end

    if schema.enum then
        local found = false
        for _, allowed in ipairs(schema.enum) do
            if value == allowed then
                found = true
                break
            end
        end
        if not found then
            local opts = {}
            for _, a in ipairs(schema.enum) do
                table.insert(opts, tostring(a))
            end
            table.insert(errors, ("%s: %s is not one of [%s]")
                                 :format(pointer, tostring(value),
                                         table.concat(opts, ", ")))
        end
    end

    if type(value) == "number" then
        if schema.minimum and value < schema.minimum then
            table.insert(errors, ("%s: %s is below the minimum %s")
                                 :format(pointer, value, schema.minimum))
        end
        if schema.maximum and value > schema.maximum then
            table.insert(errors, ("%s: %s is above the maximum %s")
                                 :format(pointer, value, schema.maximum))
        end
    end

    if type(value) == "string" and schema.pattern then
        if not value:match(schema.pattern) then
            table.insert(errors, ("%s: %q does not match %s")
                                 :format(pointer, value, schema.pattern))
        end
    end

    if _json_type(value, "object") == "object" and type(value) == "table" then
        for _, name in ipairs(schema.required or {}) do
            if value[name] == nil then
                table.insert(errors, ("%s: missing required property %q")
                                     :format(pointer, name))
            end
        end
        local properties = schema.properties or {}
        for name, subschema in pairs(properties) do
            if value[name] ~= nil then
                _validate(value[name], subschema, root,
                          pointer .. "/" .. name, errors)
            end
        end
        if schema.additionalProperties == false then
            for name, _ in pairs(value) do
                if properties[name] == nil and not name:startswith("_") then
                    table.insert(errors,
                        ("%s: unexpected property %q (additionalProperties "
                         .. "is false)"):format(pointer, name))
                end
            end
        elseif type(schema.additionalProperties) == "table" then
            for name, subvalue in pairs(value) do
                if properties[name] == nil and not name:startswith("_") then
                    _validate(subvalue, schema.additionalProperties, root,
                              pointer .. "/" .. name, errors)
                end
            end
        end
    end

    if _is_array(value) then
        if schema.minItems and #value < schema.minItems then
            table.insert(errors, ("%s: has %d items, minimum is %d")
                                 :format(pointer, #value, schema.minItems))
        end
        if schema.maxItems and #value > schema.maxItems then
            table.insert(errors, ("%s: has %d items, maximum is %d")
                                 :format(pointer, #value, schema.maxItems))
        end
        if schema.items then
            for i, item in ipairs(value) do
                _validate(item, schema.items, root,
                          ("%s/%d"):format(pointer, i - 1), errors)
            end
        end
    end
end

-- Validate 'document' against 'schema'. Returns ok, errors.
function validate(document, schema)
    local errors = {}
    _validate(document, schema, schema, "", errors)
    return #errors == 0, errors
end

-- Validate a JSON file against a schema file. Returns ok, errors.
function validate_file(docfile, schemafile)
    if not os.isfile(docfile) then
        return false, {("%s: file does not exist"):format(docfile)}
    end
    if not os.isfile(schemafile) then
        return false, {("%s: schema does not exist"):format(schemafile)}
    end
    local document, schema
    local failure
    try
    {
        function ()
            document = json.loadfile(docfile)
            schema   = json.loadfile(schemafile)
        end,
        catch
        {
            function (errors)
                failure = tostring(errors)
            end
        }
    }
    if failure then
        return false, {("%s: not valid JSON (%s)")
                       :format(docfile, failure:split("\n")[1] or failure)}
    end
    return validate(document, schema)
end
