-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- bench.lua -- the vg_bench harness: run a tier, emit a result bundle.
--
-- This is the R0 skeleton. It does everything a run does except measure a
-- fixture, because there is no engine to render one yet: it resolves the
-- fixtures, fills the manifest from the pinned versions and the machine, lays
-- out the bundle, and reports every fixture NOT_RUN with the reason it could
-- not run. What it does measure is the B0 device probes, through the C++
-- harness vg_bench, whose results it folds into the bundle.
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
import("shaders")
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
-- probe_runs is how many timed runs vg_bench takes per probe variant.
local TIERS = {
    T1 = {runs = 1, warmup_seconds = 0,  route_seconds = 20,  probe_runs = 5},
    T2 = {runs = 1, warmup_seconds = 60, route_seconds = 120, probe_runs = 5},
    T3 = {runs = 5, warmup_seconds = 60, route_seconds = 120, probe_runs = 20},
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

-- ------------------------------------------------------------ device probes
--
-- The B0 device probes run in the C++ harness, vg_bench, which writes CSV
-- (run.csv, probes.csv, measurements.csv). The bundle carries them under
-- samples/device-probes/, and the document B0's evidence names,
-- device-probes.json (r0-benchmarks.md section 6), is assembled from them
-- here -- one implementation of the bundle, as above.

-- The backend the device probes run on: the one asked for, else the
-- platform's own.
function probe_backend(opt)
    if opt.backend then
        return opt.backend
    end
    return os.host() == "macosx" and "metal" or "vulkan"
end

-- Parse CSV as vg_bench writes it: a header row, then one row per line,
-- a field quoted when it holds a comma, a quote or a line break. Returns
-- one table per row, keyed by the header's names.
function parse_csv(text)
    local rows, row, field = {}, {}, {}
    local quoted = false
    local i, n = 1, #text
    while i <= n do
        local ch = text:sub(i, i)
        if quoted then
            if ch == '"' and text:sub(i + 1, i + 1) == '"' then
                table.insert(field, '"')
                i = i + 1
            elseif ch == '"' then
                quoted = false
            else
                table.insert(field, ch)
            end
        elseif ch == '"' then
            quoted = true
        elseif ch == "," then
            table.insert(row, table.concat(field))
            field = {}
        elseif ch == "\n" then
            table.insert(row, table.concat(field))
            table.insert(rows, row)
            row, field = {}, {}
        elseif ch ~= "\r" then
            table.insert(field, ch)
        end
        i = i + 1
    end
    if #field > 0 or #row > 0 then
        table.insert(row, table.concat(field))
        table.insert(rows, row)
    end

    local header = table.remove(rows, 1) or {}
    local records = {}
    for _, r in ipairs(rows) do
        local record = {}
        for c, name in ipairs(header) do
            record[name] = r[c] or ""
        end
        table.insert(records, record)
    end
    return records
end

function _null_if_empty(value)
    if value == nil or value == "" then
        return json.null
    end
    return value
end

-- What vg_bench's run.csv says about the run itself rather than the device.
local RUN_KEYS = {
    generator = true, backend = true, shader_target = true, runs = true,
    shader_hash = true, shader_hash_reason = true,
}

-- The device-probes document for a directory vg_bench wrote: the device and
-- the toolchain that built the shaders, then every probe with its status
-- and the values it derived. The raw samples stay in measurements.csv.
function device_probes(dir)
    local run = {}
    for _, r in ipairs(parse_csv(io.readfile(path.join(dir, "run.csv")))) do
        run[r.key] = r.value
    end

    local device, toolchain = {}, {}
    for key, value in pairs(run) do
        local tool = key:match("^toolchain%.(.+)$")
        if tool then
            toolchain[tool] = value
        elseif not RUN_KEYS[key] then
            device[key] = value
        end
    end

    local derived = {}
    for _, m in ipairs(parse_csv(io.readfile(
            path.join(dir, "measurements.csv")))) do
        if m.sample == "" then
            derived[m.probe] = derived[m.probe] or {}
            table.insert(derived[m.probe], {variant = m.variant,
                                            metric  = m.metric,
                                            value   = tonumber(m.value),
                                            unit    = m.unit})
        end
    end

    local probes = {}
    for _, p in ipairs(parse_csv(io.readfile(path.join(dir, "probes.csv")))) do
        table.insert(probes, {
            id             = p.probe,
            kernel         = p.kernel,
            status         = p.status,
            unsupported_by = _null_if_empty(p.unsupported_by),
            reason         = _null_if_empty(p.reason),
            summary        = _null_if_empty(p.summary),
            notes          = _null_if_empty(p.notes),
            measurements   = derived[p.probe] or {},
        })
    end

    return {
        schema_version = 1,
        status         = "RAN",
        generator      = "vg_bench, assembled by xmake bench",
        backend        = run.backend,
        shader_target  = run.shader_target,
        shader_hash    = _null_if_empty(run.shader_hash),
        runs           = tonumber(run.runs),
        device         = device,
        toolchain      = toolchain,
        probes         = probes,
        samples        = "samples/device-probes/measurements.csv",
    }
end

-- Run vg_bench on 'backend' into 'dir' and return the device-probes
-- document and vg_bench's console output. When the probes cannot run -- no
-- binary, no GPU, no shader build -- the document says so: a bundle without
-- probes must not look like one whose probes found nothing.
function run_device_probes(backend, dir, runs)
    local function not_run(reason)
        return {schema_version = 1, status = "NOT_RUN", reason = reason,
                backend = backend, probes = {}}
    end

    if backend ~= "vulkan" and backend ~= "metal" then
        return not_run(("vg_bench runs on vulkan or metal, not %s")
                       :format(backend)), ""
    end
    local binaries = os.files(path.join(os.projectdir(), "build", "**",
                                        "vg_bench"))
    if #binaries == 0 then
        return not_run("vg_bench is not built; run 'xmake build vg_bench'"),
               ""
    end

    os.mkdir(dir)
    -- Exit 0 is all probes passed or unsupported, 1 is some failed: both
    -- are results. Exit 2 is vg_bench unable to start at all.
    local record = probe.run(binaries[1],
                             {"--backend=" .. backend,
                              "--shaders=" .. path.join(os.projectdir(),
                                                        "build", "shaders"),
                              "--out=" .. dir,
                              "--runs=" .. tostring(runs)},
                             {lines = true, stream = "both",
                              accept = {0, 1}, timeout = 1800000})
    if not probe.available(record) then
        return not_run(("vg_bench could not run the probes: %s")
                       :format(record.reason)), ""
    end
    return device_probes(dir), record.value
end

function _interpretation(manifest, planned, probes)
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
    table.insert(lines, "## Device probes")
    table.insert(lines, "")
    if probes and probes.status == "RAN" then
        table.insert(lines, ("vg_bench on `%s`, shader hash `%s`. "
                             .. "Details in device-probes.json, every sample "
                             .. "in samples/device-probes/.")
                            :format(tostring(probes.backend),
                                    tostring(probes.shader_hash)))
        table.insert(lines, "")
        table.insert(lines, "| Probe | Status | Result |")
        table.insert(lines, "| --- | --- | --- |")
        for _, p in ipairs(probes.probes) do
            local result = p.status == "PASS" and p.summary or p.reason
            table.insert(lines, ("| %s | `%s` | %s |")
                                :format(p.id, p.status,
                                        tostring(result ~= json.null
                                                 and result or "")))
        end
        table.insert(lines, "")
        table.insert(lines,
            "The device probes above are measurements. The fixtures are "
            .. "not: none of them ran, and nothing about them should be "
            .. "quoted as a number.")
    else
        table.insert(lines, ("Not run: %s."):format(
            probes and tostring(probes.reason) or "not requested"))
        table.insert(lines, "")
        table.insert(lines,
            "No measurement was taken. Nothing in this bundle is a number, "
            .. "and nothing in it should be quoted as one.")
    end
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

    -- The device probes, unless the caller turned them off (the CI tooling
    -- tests do: they must not need a GPU).
    local probes
    if opt.device_probes ~= false then
        local backend = probe_backend(opt)
        local output
        probes, output = run_device_probes(
            backend,
            path.join(out, "samples", "device-probes"),
            (TIERS[tier] or TIERS.T1).probe_runs)
        if output ~= "" then
            io.writefile(path.join(out, "logs", "vg_bench.log"), output)
        end
        io.writefile(path.join(out, "device-probes.json"), json.encode(probes))
        note("device probes on %s: %s", backend,
             probes.status == "RAN"
             and ("%d probe(s)"):format(#probes.probes)
             or tostring(probes.reason))

        if probes.status == "RAN" then
            manifest.backend = backend
            -- Recomputed from the artifacts on disk rather than copied from
            -- the index, so it describes the bytes vg_bench loaded. vg_bench
            -- has already failed any kernel whose artifact did not match.
            local hash = shaders.manifest_hash(backend)
            if probe.available(hash) then
                manifest.shader_hash = hash.value
            else
                note("shader hash unavailable: %s", hash.reason)
            end
        end
    end

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
                 _interpretation(manifest, planned, probes))

    note("bundle written")
    io.writefile(path.join(out, "logs", "bench.log"),
                 table.concat(log, "\n") .. "\n")

    local report, ok = bundle.validate(out)
    return out, report, ok, manifest
end
