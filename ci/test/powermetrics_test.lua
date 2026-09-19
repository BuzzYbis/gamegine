-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards machine.parse_powermetrics against an output-format change.
--
-- These fixtures matter because the machine that edits this file may have no
-- powermetrics access at all, while the machine that runs the benchmarks
-- does. Without them the parser can stop matching after a macOS update and
-- the only symptom is a manifest quietly missing its clocks.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("machine", {rootdir = path.join(os.projectdir(), "ci", "lua")})

function _fixture(name)
    return io.readfile(path.join(os.projectdir(), "ci", "test", "fixtures",
                                 name))
end

function run(t)
    -- M3 Pro: Profile A. Two performance clusters, and dropping either would
    -- halve the recorded performance-core picture.
    local pro = machine.parse_powermetrics(_fixture("powermetrics-m3pro.txt"))
    testing.equal(t, "m3pro: cluster count", #pro.clusters, 3)
    testing.equal(t, "m3pro: E cluster name", pro.clusters[1].cluster, "E")
    testing.equal(t, "m3pro: E is HW active, not nominal",
                  pro.clusters[1].mhz, 1284)
    testing.equal(t, "m3pro: P0 name", pro.clusters[2].cluster, "P0")
    testing.equal(t, "m3pro: P0 MHz", pro.clusters[2].mhz, 3624)
    testing.equal(t, "m3pro: P1 name", pro.clusters[3].cluster, "P1")
    testing.equal(t, "m3pro: P1 MHz", pro.clusters[3].mhz, 3576)
    testing.equal(t, "m3pro: gpu MHz", pro.gpu_mhz, 1398)
    testing.equal(t, "m3pro: cpu power mW", pro.cpu_power_mw, 12043)
    testing.equal(t, "m3pro: gpu power mW", pro.gpu_power_mw, 8421)
    testing.equal(t, "m3pro: thermal pressure", pro.thermal_pressure,
                  "Nominal")

    -- Base M-series names its performance cluster 'P-Cluster', with no index.
    local base = machine.parse_powermetrics(_fixture("powermetrics-m2.txt"))
    testing.equal(t, "m2: cluster count", #base.clusters, 2)
    testing.equal(t, "m2: P cluster has no index", base.clusters[2].cluster,
                  "P")
    testing.equal(t, "m2: P MHz", base.clusters[2].mhz, 3204)
    testing.equal(t, "m2: thermal pressure", base.thermal_pressure, "Fair")

    -- A field that is not there must be absent, never zero. A missing clock
    -- reported as 0 MHz is a fabricated measurement.
    local empty = machine.parse_powermetrics("")
    testing.equal(t, "empty: no clusters", #empty.clusters, 0)
    testing.absent(t, "empty: gpu MHz absent", empty.gpu_mhz)
    testing.absent(t, "empty: cpu power absent", empty.cpu_power_mw)
    testing.absent(t, "empty: pressure absent", empty.thermal_pressure)

    -- Output that mentions the right words with the wrong shape must not
    -- produce a number.
    local garbage = machine.parse_powermetrics(
        "CPU Power: not-a-number\nGPU HW active frequency: lots\n")
    testing.absent(t, "garbage: cpu power absent", garbage.cpu_power_mw)
    testing.absent(t, "garbage: gpu MHz absent", garbage.gpu_mhz)

    testing.absent(t, "nil input is tolerated",
                   machine.parse_powermetrics(nil).gpu_mhz)
end
