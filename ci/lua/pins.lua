-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- pins.lua -- verify the pinned inputs in ci/pins.json against the machine.
--
-- This implements gate B0.4 ("Slang, meshoptimizer and clusterlod.h versions
-- pinned and recorded in every manifest", status: required). The check is
-- deliberately unforgiving in one direction only: a version that is absent
-- from pins.json fails, and a version that is present but cannot be probed is
-- reported as recorded-not-verified rather than being treated as proven.

import("probe")
import("core.base.json")

local RELEASES = {"R0", "R1", "R2", "R3", "R4"}

function _release_index(id)
    for i, r in ipairs(RELEASES) do
        if r == id then
            return i
        end
    end
    return nil
end

-- Read ci/pins.json.
function load(pinsfile)
    pinsfile = pinsfile or path.join(os.projectdir(), "ci", "pins.json")
    if not os.isfile(pinsfile) then
        raise("pins file not found: %s", pinsfile)
    end
    return json.loadfile(pinsfile), pinsfile
end

-- Verify a vendored file against its recorded sha256. This is how a
-- single-header dependency is pinned: the version string names the upstream
-- commit, and the hash proves the bytes in third_party/ are the bytes that
-- commit produced. A vendored file edited in place is a silent fork, and this
-- is what catches it.
function _verify_vendored(spec)
    local file = path.join(os.projectdir(), spec.path)
    if not os.isfile(file) then
        return probe.unavailable("vendored file is missing", spec.path)
    end
    -- The measured hash is returned whether or not it agrees, so that a
    -- disagreement is classified as MISMATCH -- a vendored file that was
    -- edited -- rather than as an unavailable probe. The two call for
    -- different responses and must not look alike in a report.
    return probe.ok(hash.sha256(file), spec.path)
end

-- Verify that the build system declares the same pin. A dependency fetched by
-- xmake is pinned in the package definition, so pins.json and that file must
-- agree; otherwise the manifest records one version and the build uses
-- another, which is exactly the drift ci.md section 6 exists to prevent.
function _verify_declares(spec)
    local file = path.join(os.projectdir(), spec.file)
    if not os.isfile(file) then
        return probe.unavailable("declaring file is missing", spec.file)
    end
    local text = io.readfile(file) or ""
    for _, needle in ipairs(spec.must_contain or {}) do
        if not text:find(needle, 1, true) then
            return probe.unavailable(
                ("%s does not mention %s"):format(spec.file, needle),
                spec.file)
        end
    end
    return probe.ok(spec.expect, spec.file)
end

-- Run the probe described by a pins.json entry and return a probe record.
function _run_probe(spec)
    if not spec then
        return nil
    end
    local opt = {stream = spec.stream, lines = true}
    if spec.pattern then
        return probe.match(spec.program, spec.args, spec.pattern, opt)
    end
    return probe.run(spec.program, spec.args, opt)
end

-- Verify every pinned entry due at or before 'release'.
--
-- Returns a results table and a boolean saying whether the pin contract holds.
-- Statuses:
--   PINNED_VERIFIED   pinned, probed, and the probe agrees
--   PINNED_RECORDED   pinned, but nothing on this machine can report it
--   MISMATCH          pinned, probed, and the probe disagrees  -> fails
--   UNPINNED          no version recorded at all               -> fails
--   NOT_YET_DUE       required from a later release            -> informational
--   PROBE_UNAVAILABLE pinned, probe defined, tool absent        -> fails
--   UNVERIFIABLE_HERE same, but the caller passed tools_optional -> reported
--
-- opt.tools_optional downgrades a version probe whose TOOL is absent from a
-- failure to a reported status. The cloud push tier passes it: a hosted
-- runner has no Slang install, and failing there would say 'the pin is wrong'
-- when the truth is 'this machine cannot check it'. File-based pins -- the
-- vendored hash and the build-system declaration -- are checked strictly
-- everywhere, because they need no tool. The GPU runners and the gate run
-- without it, where the toolchain genuinely must be present.
--
-- opt.allow_unpinned downgrades UNPINNED from a failure to a reported status.
-- The push tier passes it and the gate tier does not: B0.4 is a release-gate
-- condition (gates.md section 4), so an unfilled pin must be visible on every
-- push without turning the branch red for the whole of R0, while a pin that
-- disagrees with the machine fails everywhere, always.
function verify(opt)
    opt = opt or {}
    local pins, pinsfile = load(opt.pinsfile)
    local release = opt.release or pins.release
    local due = _release_index(release) or 1

    local results = {}
    local ok = true

    for name, entry in pairs(pins.pinned or {}) do
        local entry_due = _release_index(entry.required_from or "R0") or 1
        -- json.null is truthy userdata in Lua, so it is normalised away
        -- here rather than at every use site.
        local pinned_version = entry.version
        if pinned_version == json.null then
            pinned_version = nil
        end

        local r = {
            name          = name,
            pinned        = pinned_version,
            required_from = entry.required_from,
            source        = entry.source,
            note          = entry.note,
        }

        if entry_due > due then
            r.status = "NOT_YET_DUE"
        elseif pinned_version == nil then
            r.status   = "UNPINNED"
            r.detail   = entry.note
                         or "no version recorded in ci/pins.json"
            r.blocking = not opt.allow_unpinned
            if r.blocking then
                ok = false
            end
        else
            local expected = pinned_version
            local record
            if entry.vendored then
                record   = _verify_vendored(entry.vendored)
                expected = entry.vendored.sha256
                r.vendored_path = entry.vendored.path
            elseif entry.declares then
                local spec = table.clone(entry.declares)
                spec.expect = pinned_version
                record = _verify_declares(spec)
                r.declared_in = entry.declares.file
            else
                record = _run_probe(entry.probe)
            end
            if record == nil then
                r.status = "PINNED_RECORDED"
                r.detail = "no probe defined; the pinned value is asserted "
                           .. "by ci/pins.json and recorded in the manifest"
            elseif record.status ~= "OK" then
                -- A missing vendored file or a missing package definition is
                -- always a failure: no tool is needed to look at a file.
                local tool_probe = (entry.vendored == nil
                                    and entry.declares == nil)
                if tool_probe and opt.tools_optional then
                    r.status   = "UNVERIFIABLE_HERE"
                    r.detail   = ("%s; pin recorded but not checked on this "
                                  .. "machine"):format(record.reason)
                    r.blocking = false
                else
                    r.status = "PROBE_UNAVAILABLE"
                    r.detail = record.reason
                    ok = false
                end
                r.probe_source = record.source
            elseif record.value ~= expected then
                r.status   = "MISMATCH"
                r.detected = record.value
                r.detail   = ("pinned %s, machine has %s")
                             :format(expected, record.value)
                r.probe_source = record.source
                ok = false
            else
                r.status       = "PINNED_VERIFIED"
                r.detected     = record.value
                r.probe_source = record.source
            end
        end
        table.insert(results, r)
    end

    table.sort(results, function (a, b) return a.name < b.name end)

    -- Recorded inputs never fail; they are captured so a driver or SDK move
    -- is visible in the manifest that explains a timing change.
    local recorded = {}
    for name, entry in pairs(pins.recorded or {}) do
        local record = _run_probe(entry.probe)
            or probe.unavailable("no probe defined", "ci/pins.json")
        table.insert(recorded, {
            name   = name,
            status = record.status,
            value  = record.value,
            reason = record.reason,
            source = record.source,
            note   = entry.note,
        })
    end
    table.sort(recorded, function (a, b) return a.name < b.name end)

    return {
        schema_version = 1,
        pins_file      = pinsfile,
        release        = release,
        checked_at_utc = os.date("!%Y-%m-%dT%H:%M:%SZ"),
        pinned         = results,
        recorded       = recorded,
        gate           = "B0.4",
        allow_unpinned = opt.allow_unpinned or false,
        tools_optional = opt.tools_optional or false,
        passed         = ok,
    }, ok
end
