-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- bench.lua -- the vg_bench harness: run a tier, emit a result bundle.
--
-- This is the R0 skeleton. It does everything a run does EXCEPT measure,
-- because there is no engine to measure yet: it resolves the fixtures, fills
-- the manifest from the pinned versions and the machine, lays out the bundle,
-- and reports every fixture NOT_RUN with the reason it could not run.
--
-- That is the whole point of standing it up now. The bundle layout and the
-- manifest schema are contracts frozen at R0 (r0-architecture.md section 5)
-- and every later result is compared against R0's, so the thing that writes
-- them wants to exist before the numbers do -- not after, when the shape is
-- already load-bearing.
--
-- Bundle assembly stays here rather than in the eventual C++ binary so there
-- is ONE implementation of the manifest. Two that can disagree is how a
-- schema stops describing its own output. When there is an engine, the
-- binary measures and hands its samples back; this still assembles.

import("probe")
import("pins")
import("machine")
import("bundle")
import("core.base.json")

-- Defaults are the documented ones: 1920x1080 native with no reconstruction,
-- a 1 px LOD error target, the 1 GiB normal memory profile.
local DEFAULTS = {
    tier            = "T1",
    profile         = nil,       -- resolved from the machine
    mode            = "warm",
    memory_profile  = "normal",
    geometry_pool   = 1024,
    resolution      = {1920, 1080},
    lod_error_px    = 1.0,
    raster_path     = "indexed_indirect",  -- mesh shaders arrive at R2 (D3b)
    release         = "R0",
    benchmark       = "B0",
}

-- Per-tier run shape, from benchmarks.md section 2.
local TIERS = {
    T1 = {runs = 1, warmup_seconds = 0,  route_seconds = 20},
    T2 = {runs = 1, warmup_seconds = 60, route_seconds = 120},
    T3 = {runs = 5, warmup_seconds = 60, route_seconds = 120},
}

function _git_revision()
    local r = probe.run("git", {"rev-parse", "HEAD"})
    return probe.available(r) and r.value or nil
end

-- json.encode drops a nil field entirely, but the schema requires most of
-- these keys to be PRESENT and null. An absent key and a null one mean
-- different things here: null is "this run did not produce it", absent is
-- "this manifest does not know the field exists".
function _null_if_nil(value)
    if value == nil then
        return json.null
    end
    return value
end

function load_fixtures(file)
    file = file or path.join(os.projectdir(), "ci", "fixtures.json")
    if not os.isfile(file) then
        return {}
    end
    return json.loadfile(file).fixtures or {}
end

-- Decide what happens to each fixture in this run. At R0 nothing is built, so
-- everything is NOT_RUN -- but each carries the reason, and the reasons are
-- the R0 to-do list stated in the evidence rather than in someone's head.
function plan_fixtures(fixtures)
    local planned = {}
    for _, f in ipairs(fixtures) do
        local entry = {
            id         = f.id,
            definition = f.definition,
            created_in = f.created_in,
        }
        if (f.build or {}).status == "built" then
            -- There is still no engine behind the harness, so even a built
            -- fixture cannot produce numbers yet. Saying so explicitly beats
            -- a bundle that looks measured.
            entry.status = "NOT_RUN"
            entry.reason = "fixture is built, but no backend is implemented "
                           .. "yet; there is nothing to render it with"
        else
            entry.status = "NOT_RUN"
            entry.reason = (f.build or {}).reason or "fixture is not built"
        end
        table.insert(planned, entry)
    end
    return planned
end

-- Assemble the run manifest. Every pinned version is carried through, which
-- is the half of gate B0.4 that says "recorded in every manifest" -- the pin
-- file proves they are pinned, this proves they travel with the result.
function build_manifest(opt, machine_manifest, pin_report, planned)
    local tier  = opt.tier or DEFAULTS.tier
    local shape = TIERS[tier] or TIERS.T1

    local versions = {}
    for _, e in ipairs(pin_report.pinned or {}) do
        versions[e.name] = e.pinned
    end

    -- Why the whole run is NOT_RUN, said once, in the words of whatever
    -- actually blocked it.
    local blocked = {}
    for _, f in ipairs(planned) do
        if f.status ~= "PASS" then
            table.insert(blocked, ("%s: %s"):format(f.id, f.reason))
        end
    end

    local profile = opt.profile
                    or (machine_manifest.profile or {}).id
    local role    = (machine_manifest.machine or {}).role or "development"

    return {
        release   = opt.release or DEFAULTS.release,
        benchmark = opt.benchmark or DEFAULTS.benchmark,
        tier      = tier,
        status    = "NOT_RUN",
        status_reason = #blocked > 0
            and ("no fixture could run. " .. table.concat(blocked, "; "))
            or "no fixtures are registered",
        not_implemented_clears_by = json.null,

        source_revision     = _null_if_nil(_git_revision()),
        asset_manifest_hash = json.null,
        generator_seed      = json.null,
        cooker_version      = json.null,
        shader_hash         = json.null,

        slang_version         = _null_if_nil(versions.slang),
        clusterlod_version    = _null_if_nil(versions.clusterlod_h),
        meshoptimizer_version = _null_if_nil(versions.meshoptimizer),

        feature_set_release          = opt.release or DEFAULTS.release,
        profile_a_trails_by_releases = 1,   -- D2: Metal trails by one release

        profile           = profile or "L",
        backend           = _null_if_nil(opt.backend),
        machine_role      = role,
        evidence_eligible = machine_manifest.evidence_eligible == true,
        machine_manifest  = "machine.json",

        actual_physical_ram_gb = json.null,
        actual_gpu_core_count  = json.null,
        gpu_power_limit_w      = json.null,

        memory_profile         = opt.memory_profile or DEFAULTS.memory_profile,
        geometry_pool_mib      = opt.geometry_pool or DEFAULTS.geometry_pool,
        cooked_geometry_gib    = json.null,
        oversubscription_ratio = json.null,

        resolution   = opt.resolution or DEFAULTS.resolution,
        lod_error_px = opt.lod_error_px or DEFAULTS.lod_error_px,
        raster_path  = opt.raster_path or DEFAULTS.raster_path,
        mode         = opt.mode or DEFAULTS.mode,

        runs            = opt.runs or shape.runs,
        warmup_seconds  = shape.warmup_seconds,
        route_seconds   = shape.route_seconds,

        measured_frame_p95_ms      = json.null,
        measured_frame_p99_ms      = json.null,
        measured_gpu_p95_ms        = json.null,
        measured_cpu_active_p95_ms = json.null,

        command         = opt.command or "xmake bench",
        started_at_utc  = opt.started_at,
        finished_at_utc = os.date("!%Y-%m-%dT%H:%M:%SZ"),
    }
end

function _interpretation(manifest, planned)
    local lines = {
        ("# %s / %s -- %s"):format(manifest.benchmark, manifest.tier,
                                   manifest.status),
        "",
        ("Generated by `%s` on %s."):format(manifest.command,
                                            manifest.finished_at_utc),
        "",
    }

    if not manifest.evidence_eligible then
        table.insert(lines,
            "> **This bundle is not evidence.** It was produced on a machine "
            .. "registered as `" .. manifest.machine_role .. "` in "
            .. "ci/machines.json. Runs there exercise the harness; they do "
            .. "not substantiate a claim.")
        table.insert(lines, "")
    end

    table.insert(lines, "## Fixtures")
    table.insert(lines, "")
    table.insert(lines, "| Fixture | Status | Why |")
    table.insert(lines, "| --- | --- | --- |")
    for _, f in ipairs(planned) do
        table.insert(lines, ("| %s | `%s` | %s |")
                            :format(f.id, f.status, f.reason or ""))
    end

    table.insert(lines, "")
    table.insert(lines, "## Pinned inputs")
    table.insert(lines, "")
    table.insert(lines, ("- Slang `%s`"):format(
        tostring(manifest.slang_version)))
    table.insert(lines, ("- meshoptimizer `%s`"):format(
        tostring(manifest.meshoptimizer_version)))
    table.insert(lines, ("- clusterlod.h `%s`"):format(
        tostring(manifest.clusterlod_version)))

    table.insert(lines, "")
    table.insert(lines,
        "No measurement was taken. Nothing in this bundle is a number, and "
        .. "nothing in it should be quoted as one.")
    table.insert(lines, "")
    return table.concat(lines, "\n")
end

-- Run a tier and write the bundle to 'opt.out'. Returns the bundle path and
-- the validation report.
function run(opt)
    opt = opt or {}
    opt.started_at = os.date("!%Y-%m-%dT%H:%M:%SZ")

    local out = opt.out
    if not out then
        raise("no output directory: pass --out")
    end
    if not path.is_absolute(out) then
        out = path.join(os.projectdir(), out)
    end

    local tier = opt.tier or DEFAULTS.tier
    if not TIERS[tier] then
        raise("unknown tier %q; expected T1, T2 or T3", tier)
    end

    -- Lay the bundle out first, so a failure half way leaves a directory
    -- that is recognisably incomplete rather than plausibly complete.
    os.mkdir(out)
    for _, d in ipairs({"samples", "correctness", "logs"}) do
        os.mkdir(path.join(out, d))
    end

    local log = {}
    -- 'select' is not available in the xmake sandbox, so the variadic case
    -- is distinguished by counting the packed arguments.
    local function note(fmt, ...)
        local args = {...}
        local text = (#args > 0) and fmt:format(...) or fmt
        table.insert(log, ("[%s] %s"):format(os.date("!%H:%M:%SZ"), text))
    end

    note("vg_bench harness, R0 skeleton")
    note("tier=%s out=%s", tier, out)

    local machine_manifest = machine.capture({
        profile    = opt.profile,
        skip_power = opt.skip_power,
    })
    io.writefile(path.join(out, "machine.json"),
                 json.encode(machine_manifest))
    note("machine: %s (role=%s, evidence_eligible=%s)",
         (machine_manifest.machine or {}).id or "?",
         (machine_manifest.machine or {}).role or "?",
         tostring(machine_manifest.evidence_eligible))

    local pin_report = pins.verify({allow_unpinned = true,
                                    tools_optional = true})
    note("pins: %d recorded", #(pin_report.pinned or {}))

    local fixtures = load_fixtures(opt.fixtures)
    local planned  = plan_fixtures(fixtures)
    note("fixtures: %d planned, 0 runnable", #planned)

    local manifest = build_manifest(opt, machine_manifest, pin_report, planned)

    io.writefile(path.join(out, "manifest.json"), json.encode(manifest))

    -- Aggregate carries explicit nulls rather than zeros. A p95 of 0 ms is a
    -- measurement; a p95 of null is the absence of one, and the difference
    -- matters to every later comparison.
    io.writefile(path.join(out, "aggregate.json"), json.encode({
        schema_version = 1,
        tier           = tier,
        runs           = manifest.runs,
        fixtures       = planned,
        frame_ms       = {p50 = json.null, p95 = json.null, p99 = json.null},
        gpu_ms         = {p50 = json.null, p95 = json.null, p99 = json.null},
        cpu_active_ms  = {p50 = json.null, p95 = json.null, p99 = json.null},
        memory         = {allocated_bytes = json.null, used_bytes = json.null},
        note           = "no measurement was taken; every statistic is null "
                         .. "because none was produced, not because it was "
                         .. "zero",
    }))

    -- Header-only sample files: the columns are part of the contract too, and
    -- a consumer written against them now keeps working when rows appear.
    io.writefile(path.join(out, "samples", "frames.csv"),
                 "run,frame,cpu_active_ms,gpu_ms,frame_ms,"
                 .. "selected_triangles,visible_pixels\n")
    io.writefile(path.join(out, "samples", "streaming.csv"),
                 "run,t_ms,requested_bytes,read_bytes,uploaded_bytes,"
                 .. "resident_mib,pinned_mib,queue_depth\n")

    io.writefile(path.join(out, "interpretation.md"),
                 _interpretation(manifest, planned))

    note("bundle written")
    io.writefile(path.join(out, "logs", "bench.log"),
                 table.concat(log, "\n") .. "\n")

    local report, ok = bundle.validate(out)
    return out, report, ok, manifest
end
