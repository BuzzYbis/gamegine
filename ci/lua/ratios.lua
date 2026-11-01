-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- ratios.lua -- the per-workload Apple-to-Linux ratios of gate B0.
--
-- r0-benchmarks.md section 5 asks for ratios per workload -- bandwidth,
-- raster, decode, CPU preparation -- to replace the single scalar 2x that
-- every A-profile target assumes. Each workload names the device-probe value
-- that measures it, so a ratio carries its own definition (section 6: "with
-- the workload definitions").
--
-- A ratio is Apple over Linux, on a throughput: below 1, the Apple machine
-- is slower on that workload.

import("core.base.json")

local WORKLOADS = {
    {
        name       = "bandwidth",
        probe      = "check.b0_6_timing_agreement",
        metric     = "stream_mib_per_s",
        definition = "GPU memory streaming: decodePage over 4M records, each "
                     .. "read once (16 bytes) and written once decoded (32 "
                     .. "bytes), with no reuse",
    },
    {
        name       = "raster",
        probe      = "probe.mesh_cluster_dispatch",
        variant    = "clusters=262144",
        metric     = "triangles_per_ms",
        definition = "small-triangle rasterisation: 262144 mesh-shader "
                     .. "clusters of 98 triangles over a 1024x1024 target",
    },
    {
        name       = "decode",
        probe      = "probe.page_decode",
        variant    = "page=128KiB",
        metric     = "gpu_decode_mib_per_s",
        definition = "GPU attribute decode of 128 KiB geometry pages, the "
                     .. "documented default size; MiB of records in per second",
    },
    {
        name       = "cpu_preparation",
        probe      = "probe.page_decode",
        variant    = "page=128KiB",
        metric     = "cpu_decompress_mib_per_s",
        definition = "CPU decompression of 128 KiB pages with meshoptimizer's "
                     .. "vertex codec, one thread; MiB out per second",
    },
}

-- The value 'workload' reads from a device-probes document, with its unit;
-- or nil and why there is none.
function _value(probes, workload)
    if probes.status ~= "RAN" then
        return nil, ("the device probes did not run: %s")
                    :format(tostring(probes.reason))
    end
    for _, p in ipairs(probes.probes or {}) do
        if p.id == workload.probe then
            if p.status ~= "PASS" then
                return nil, ("%s is %s"):format(p.id, p.status)
            end
            for _, m in ipairs(p.measurements or {}) do
                if m.metric == workload.metric
                   and (workload.variant == nil
                        or m.variant == workload.variant) then
                    return m.value, nil, m.unit
                end
            end
            return nil, ("%s reports no %s"):format(p.id, workload.metric)
        end
    end
    return nil, ("%s is not in the report"):format(workload.probe)
end

-- One entry per workload: both values, their ratio, and the definition. A
-- workload either side cannot measure has a null ratio and says why.
function derive(apple, linux)
    local out = {}
    for _, w in ipairs(WORKLOADS) do
        local a, whyApple, unit = _value(apple, w)
        local l, whyLinux       = _value(linux, w)
        local entry = {
            workload   = w.name,
            definition = w.definition,
            probe      = w.probe,
            variant    = w.variant or json.null,
            metric     = w.metric,
            unit       = unit or json.null,
            apple      = a or json.null,
            linux      = l or json.null,
            ratio      = json.null,
        }
        if a and l and l > 0 then
            entry.ratio = a / l
        else
            entry.reason = whyApple
                           and ("Apple: " .. whyApple)
                           or (whyLinux and ("Linux: " .. whyLinux)
                               or "the Linux value is zero")
        end
        table.insert(out, entry)
    end
    return out
end

-- Which machine and backend a bundle's numbers come from.
function _side(dir, probes)
    local manifest = json.loadfile(path.join(dir, "manifest.json")) or {}
    return {
        bundle            = dir,
        backend           = probes.backend or json.null,
        device            = (probes.device or {}).device_name or json.null,
        shader_hash       = probes.shader_hash or json.null,
        evidence_eligible = manifest.evidence_eligible == true,
    }
end

-- The ratios document for an Apple bundle and a Linux one, each written by
-- 'xmake bench'. Caveats name what makes the comparison weaker than it
-- looks: the wrong backend on a side, or a side that is not evidence.
function document(appleDir, linuxDir)
    local apple = json.loadfile(path.join(appleDir, "device-probes.json"))
    local linux = json.loadfile(path.join(linuxDir, "device-probes.json"))
    if not apple then
        raise("%s has no device-probes.json; run 'xmake bench' there",
              appleDir)
    end
    if not linux then
        raise("%s has no device-probes.json; run 'xmake bench' there",
              linuxDir)
    end

    local doc = {
        schema_version   = 1,
        generated_at_utc = os.date("!%Y-%m-%dT%H:%M:%SZ"),
        ratio            = "Apple / Linux, on throughputs: below 1, the "
                           .. "Apple machine is slower on that workload",
        apple            = _side(appleDir, apple),
        linux            = _side(linuxDir, linux),
        workloads        = derive(apple, linux),
        caveats          = {},
    }
    if doc.apple.backend ~= "metal" then
        table.insert(doc.caveats, ("the Apple side ran on %s, not Metal")
                                  :format(tostring(doc.apple.backend)))
    end
    if doc.linux.backend ~= "vulkan" then
        table.insert(doc.caveats, ("the Linux side ran on %s, not Vulkan")
                                  :format(tostring(doc.linux.backend)))
    end
    if doc.apple.device ~= json.null
       and doc.apple.device == doc.linux.device then
        table.insert(doc.caveats,
            ("both sides ran on the same device, %s: these ratios compare "
             .. "two APIs, not two machines"):format(doc.apple.device))
    end
    for _, side in ipairs({"apple", "linux"}) do
        if not doc[side].evidence_eligible then
            table.insert(doc.caveats,
                ("the %s bundle is not evidence: its machine is not a "
                 .. "qualification machine"):format(side))
        end
    end
    return doc
end
