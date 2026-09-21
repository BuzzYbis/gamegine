-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- bundle.lua -- validate the layout of a result bundle.
--
-- The layout is benchmarks.md section 6: "Each result bundle contains
-- manifest.json, frame and streaming samples as CSV or JSONL, aggregate JSON,
-- correctness images and masks, run logs, and a short Markdown
-- interpretation." Gate B0 adds five files of its own (r0-benchmarks.md
-- section 6).
--
-- Bundles are retained for the last 30 nightly runs, every weekly run, and
-- every gate run forever: gate bundles are the evidence behind published
-- claims and they outlive the machines that produced them (ci.md section 7).
-- That is why the layout is checked rather than assumed.

import("jsonschema")
import("core.base.json")

-- Files every bundle carries, whatever the gate.
local REQUIRED = {
    {path = "manifest.json",
     why  = "benchmarks.md section 6: the manifest records the actual "
            .. "command and the pinned versions"},
    {path = "aggregate.json",
     why  = "aggregate statistics; p50/p95/p99 computed on individual frame "
            .. "samples within each run"},
    {path = "interpretation.md",
     why  = "a short Markdown interpretation; a bundle nobody can read is "
            .. "not evidence"},
}

-- Directories every bundle carries. Empty is allowed at R0 -- there is one
-- trivial fixture and no streaming -- but the directory itself must exist so
-- that 'absent' and 'not produced' are distinguishable.
local REQUIRED_DIRS = {
    {path = "samples",     why = "frame and streaming samples, CSV or JSONL"},
    {path = "correctness", why = "correctness images and masks, including "
                                 .. "the archived exclusion mask"},
    {path = "logs",        why = "run logs"},
}

-- Extra evidence required by a gate run. These belong to the T3 gate bundle
-- (r0-benchmarks.md section 6 names results/B0-L and results/B0-A), not to
-- every nightly T1 run that happens to carry the same benchmark id.
local GATE_EXTRAS = {
    B0 = {
        {path = "reference-run.json",
         why  = "vk_lod_clusters revision, settings and measured figures"},
        {path = "builder-run.json",
         why  = "clusterlod.h timings, node counts, reduction curve, RSS "
                .. "against threads"},
        {path = "device-probes.json",
         why  = "every microbenchmark with full device and toolchain "
                .. "provenance"},
        {path = "ratios.json",
         why  = "the derived per-workload Apple-to-Linux ratios, with the "
                .. "workload definitions"},
        {path = "decisions.md",
         why  = "D0-D5, D7, D8 restated with their answers"},
    },
}

function _finding(severity, message, why)
    return {severity = severity, message = message, why = why}
end

-- Validate the bundle rooted at 'dir'. Returns a result table and a boolean.
--
-- A missing required file is an error. A required directory that exists but
-- is empty is a warning, not an error, because at R0 there is genuinely
-- nothing to put in it yet.
function validate(dir, opt)
    opt = opt or {}
    local findings = {}
    local result = {
        schema_version = 1,
        bundle         = dir,
        checked_at_utc = os.date("!%Y-%m-%dT%H:%M:%SZ"),
    }

    if not os.isdir(dir) then
        table.insert(findings,
            _finding("error", ("bundle directory %s does not exist"):format(dir)))
        result.findings = findings
        result.passed   = false
        return result, false
    end

    local manifest, gate
    local manifestfile = path.join(dir, "manifest.json")

    for _, entry in ipairs(REQUIRED) do
        if not os.isfile(path.join(dir, entry.path)) then
            table.insert(findings,
                _finding("error", ("missing %s"):format(entry.path), entry.why))
        end
    end

    for _, entry in ipairs(REQUIRED_DIRS) do
        local d = path.join(dir, entry.path)
        if not os.isdir(d) then
            table.insert(findings,
                _finding("error", ("missing directory %s/"):format(entry.path),
                         entry.why))
        elseif #os.files(path.join(d, "**")) == 0 then
            table.insert(findings,
                _finding("warning",
                         ("directory %s/ is empty"):format(entry.path),
                         entry.why))
        end
    end

    -- The manifest drives the rest: it names the gate whose extra evidence
    -- is required, and it is itself schema-checked.
    if os.isfile(manifestfile) then
        local schemafile = opt.schema
            or path.join(os.projectdir(), "ci", "schema",
                         "run-manifest.schema.json")
        local ok, errors = jsonschema.validate_file(manifestfile, schemafile)
        if not ok then
            for _, e in ipairs(errors) do
                table.insert(findings,
                    _finding("error", ("manifest.json%s"):format(e),
                             "run manifest schema, frozen at R0"))
            end
        end
        try
        {
            function ()
                manifest = json.loadfile(manifestfile)
                gate     = manifest.benchmark
            end,
            catch { function () end }
        }
    end

    if gate and GATE_EXTRAS[gate] and manifest and manifest.tier == "T3" then
        for _, entry in ipairs(GATE_EXTRAS[gate]) do
            if not os.isfile(path.join(dir, entry.path)) then
                table.insert(findings,
                    _finding("error",
                             ("missing %s, required by gate %s")
                             :format(entry.path, gate), entry.why))
            end
        end
    end

    -- An UNAVAILABLE or NOT_IMPLEMENTED result has to say why. This is the
    -- rule that stops a silent skip from looking like a pass.
    if manifest then
        local status = manifest.status
        if (status == "UNAVAILABLE" or status == "NOT_IMPLEMENTED")
           and (manifest.status_reason == nil
                or manifest.status_reason == json.null
                or tostring(manifest.status_reason):trim() == "") then
            table.insert(findings,
                _finding("error",
                         ("status is %s but status_reason is empty")
                         :format(status),
                         "ci.md section 4: UNAVAILABLE is not a pass, and it "
                         .. "carries its reason"))
        end
        if status == "NOT_IMPLEMENTED"
           and (manifest.not_implemented_clears_by == nil
                or manifest.not_implemented_clears_by == json.null) then
            table.insert(findings,
                _finding("error",
                         "NOT_IMPLEMENTED without not_implemented_clears_by",
                         "ci.md section 4: NOT_IMPLEMENTED carries the "
                         .. "release by which it must clear"))
        end
        -- A gate run on a development machine is not evidence, however
        -- complete the bundle looks. ci.md section 7: gate bundles are the
        -- evidence behind published claims and behind the release report,
        -- and they outlive the machines that produced them.
        if manifest.tier == "T3" and manifest.machine_role == "development"
        then
            table.insert(findings,
                _finding("error",
                         "a T3 gate bundle was produced on a development "
                         .. "machine",
                         "runs on a development machine are mock. Re-run the "
                         .. "gate protocol on the qualification hardware in "
                         .. "architecture.md section 2."))
        end

        -- The two fields must agree. A bundle claiming to be evidence while
        -- naming a development machine is the exact confusion the role
        -- exists to prevent, so it is an error rather than a warning.
        if manifest.evidence_eligible == true
           and manifest.machine_role == "development" then
            table.insert(findings,
                _finding("error",
                         "evidence_eligible is true but machine_role is "
                         .. "'development'",
                         "these contradict; a development machine produces "
                         .. "mock runs only"))
        end

        result.gate         = gate
        result.status       = status
        result.profile      = manifest.profile
        result.machine_role = manifest.machine_role
    end

    local errors = 0
    for _, f in ipairs(findings) do
        if f.severity == "error" then
            errors = errors + 1
        end
    end

    result.findings = findings
    result.errors   = errors
    result.passed   = (errors == 0)
    return result, result.passed
end
