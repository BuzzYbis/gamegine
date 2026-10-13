-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the Apple-to-Linux ratios: the arithmetic, and above all that a
-- workload one side could not measure gets no ratio rather than a ratio
-- against nothing. A ratio of 0 or of infinity would compare cleanly and be
-- wrong.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("ratios", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("core.base.json")

-- A device-probes document with the values every workload reads.
function _probes(scale, meshStatus)
    return {
        status = "RAN",
        probes = {
            {id = "check.b0_6_timing_agreement", status = "PASS",
             measurements = {{variant = "dispatches=13",
                              metric = "stream_mib_per_s",
                              value = 100 * scale, unit = "MiB/s"}}},
            {id = "probe.mesh_cluster_dispatch", status = meshStatus,
             measurements = {{variant = "clusters=16384",
                              metric = "triangles_per_ms",
                              value = 1, unit = "triangles/ms"},
                             {variant = "clusters=262144",
                              metric = "triangles_per_ms",
                              value = 50 * scale, unit = "triangles/ms"}}},
            {id = "probe.page_decode", status = "PASS",
             measurements = {{variant = "page=64KiB",
                              metric = "gpu_decode_mib_per_s",
                              value = 1, unit = "MiB/s"},
                             {variant = "page=128KiB",
                              metric = "gpu_decode_mib_per_s",
                              value = 40 * scale, unit = "MiB/s"},
                             {variant = "page=128KiB",
                              metric = "cpu_decompress_mib_per_s",
                              value = 5 * scale, unit = "MiB/s"}}},
        },
    }
end

function _find(workloads, name)
    for _, w in ipairs(workloads) do
        if w.workload == name then
            return w
        end
    end
end

function run(t)
    local both = ratios.derive(_probes(1, "PASS"), _probes(2, "PASS"))
    testing.equal(t, "the four B0 workloads", #both, 4)
    for _, name in ipairs({"bandwidth", "raster", "decode",
                           "cpu_preparation"}) do
        local w = _find(both, name)
        testing.check(t, name .. " is there", w ~= nil)
        testing.equal(t, name .. " is Apple over Linux", w.ratio, 0.5)
        testing.check(t, name .. " carries its definition",
                      type(w.definition) == "string" and #w.definition > 0)
    end
    testing.equal(t, "the variant picks the right value, not the first",
                  _find(both, "decode").apple, 40)

    local unsupported = ratios.derive(_probes(1, "PASS"),
                                      _probes(1, "UNSUPPORTED"))
    local raster = _find(unsupported, "raster")
    testing.check(t, "an unsupported side gives no ratio",
                  raster.ratio == json.null)
    testing.check(t, "and says which side and why",
                  raster.reason:find("Linux", 1, true) ~= nil
                  and raster.reason:find("UNSUPPORTED", 1, true) ~= nil)
    testing.equal(t, "the other workloads are unaffected",
                  _find(unsupported, "decode").ratio, 1)

    local notRun = ratios.derive({status = "NOT_RUN", reason = "no GPU"},
                                 _probes(1, "PASS"))
    testing.check(t, "probes that did not run give no ratios",
                  _find(notRun, "bandwidth").ratio == json.null)
end
