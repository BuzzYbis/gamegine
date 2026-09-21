-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- machine.lua -- capture the machine manifest required at every benchmark run.
--
-- The field list is architecture.md section 2 ("Machine manifest"). Every
-- field is a probe record from probe.lua, so a field that could not be read
-- carries UNAVAILABLE and its reason rather than a plausible default. See
-- references.md: "the machine manifest records device-reported values,
-- because a laptop GPU's behaviour depends on the power limit, cooling mode
-- and display path far more than on the model name."

import("probe")
import("core.base.json")

-- Shorten a probe reason to its first sentence for display.
function _firstline(text)
    local line = tostring(text or ""):match("^([^\r\n]*)") or ""
    return line:trim()
end

-- The profiles declared in architecture.md section 2. 'claimed' values are
-- what the plan says the qualification hardware is; they are compared against
-- what the device actually reports, and a mismatch is recorded, not resolved.
local PROFILES = {
    L = {
        id                   = "L",
        host                 = "linux",
        claimed_gpu          = "NVIDIA GeForce RTX 4060 Laptop GPU",
        claimed_cpu          = "Intel Core Ultra 9 185H",
        claimed_memory_gb    = 32,
        claimed_api          = "Vulkan 1.4",
        required_frame_rate  = 60,
    },
    A = {
        id                   = "A",
        host                 = "macosx",
        claimed_gpu          = "Apple M3 Pro",
        claimed_cpu          = "Apple M3 Pro",
        claimed_memory_gb    = 18,
        claimed_api          = "Metal 4",
        required_frame_rate  = 30,
    },
}

-- Pull "Key: value" out of a system_profiler or lscpu style block.
function _field(text, key)
    if not text then
        return nil
    end
    local pattern = key:gsub("([%^%$%(%)%%%.%[%]%*%+%-%?])", "%%%1")
    return text:match(pattern .. "%s*:%s*([^\r\n]+)")
end

-- Capture a whole command's output once so several fields can be read from
-- it without paying for the command repeatedly; system_profiler is slow.
function _capture(program, argv)
    local record = probe.run(program, argv, {lines = true})
    return record
end

-- Derive one field from an already-captured block.
function _from_block(block, key, transform)
    if block.status ~= "OK" then
        return probe.unavailable(
            ("source command unavailable: %s"):format(block.reason or "unknown"),
            block.source)
    end
    local value = _field(block.value, key)
    if not value then
        return probe.unavailable(
            ("field %q not present in output"):format(key), block.source)
    end
    value = value:trim()
    if transform then
        value = transform(value)
    end
    return probe.ok(value, block.source)
end

function _tonumber_record(record)
    if record.status ~= "OK" then
        return record
    end
    local n = tonumber((tostring(record.value):gsub("[^%d%.%-]", "")))
    if not n then
        return probe.unavailable("value is not numeric", record.source)
    end
    return probe.ok(n, record.source)
end

-- ---------------------------------------------------------------- macOS ----

function _capture_macos(opt)
    opt = opt or {}
    local m = {}

    local sw = _capture("sw_vers", {})
    m.os_name        = _from_block(sw, "ProductName")
    m.os_version     = _from_block(sw, "ProductVersion")
    m.os_build       = _from_block(sw, "BuildVersion")
    m.kernel         = probe.run("uname", {"-r"})

    m.cpu_model      = probe.run("sysctl", {"-n", "machdep.cpu.brand_string"})
    m.cpu_logical    = _tonumber_record(probe.run("sysctl", {"-n", "hw.ncpu"}))
    m.hardware_model = probe.run("sysctl", {"-n", "hw.model"})
    m.memory_bytes   = _tonumber_record(
                           probe.run("sysctl", {"-n", "hw.memsize"}))

    -- Unified memory: on Apple silicon the GPU shares hw.memsize, so the
    -- "VRAM" field of the manifest is the same number and is labelled as
    -- unified rather than dedicated.
    m.memory_kind    = probe.ok("unified", "architecture.md section 2")

    local disp = _capture("system_profiler", {"SPDisplaysDataType"})
    m.gpu_name        = _from_block(disp, "Chipset Model")
    m.gpu_core_count  = _tonumber_record(
                            _from_block(disp, "Total Number of Cores"))
    m.gpu_vendor      = _from_block(disp, "Vendor")
    m.graphics_api    = _from_block(disp, "Metal Support")
    m.display_main    = _from_block(disp, "Resolution")

    local nvme = _capture("system_profiler", {"SPNVMeDataType"})
    m.storage_model   = _from_block(nvme, "Model")

    -- AC state: "Now drawing from 'AC Power'" or "'Battery Power'". Laptop
    -- results are valid for the recorded power configuration only.
    local batt = _capture("pmset", {"-g", "batt"})
    if batt.status == "OK" then
        local source = batt.value:match("Now drawing from '([^']+)'")
        m.ac_state = source and probe.ok(source, batt.source)
                     or probe.unavailable("power source line not found",
                                          batt.source)
    else
        m.ac_state = batt
    end

    -- Cooling mode. macOS does expose one: Low Power Mode throttles
    -- sustained clocks, and High Power Mode (Pro and Max notebooks) raises
    -- the fan ceiling. Both change benchmark results, so both are recorded.
    local pm_settings = _capture("pmset", {"-g"})
    if pm_settings.status == "OK" then
        local low  = pm_settings.value:match("lowpowermode%s+(%d)")
        local high = pm_settings.value:match("highpowermode%s+(%d)")
        if low or high then
            local modes = {}
            if low then
                table.insert(modes, "lowpowermode=" .. low)
            end
            if high then
                table.insert(modes, "highpowermode=" .. high)
            end
            m.cooling_mode = probe.ok(table.concat(modes, " "),
                                      pm_settings.source)
        else
            m.cooling_mode = probe.unavailable(
                "pmset reports neither lowpowermode nor highpowermode",
                pm_settings.source)
        end
    else
        m.cooling_mode = pm_settings
    end

    -- Sustained clocks and thermal state, if powermetrics can be reached
    -- without a password. Every field below degrades to UNAVAILABLE with a
    -- reason when it cannot.
    -- Written out rather than folded into an and/or chain: that form
    -- collapses a two-value return to one, and the access record would be
    -- silently lost.
    local pm, access
    if opt.skip_power then
        access = probe.unavailable("skipped: --no-power was given",
                                   "powermetrics")
    else
        pm, access = _powermetrics_capture()
    end
    m.power_access = access

    if pm then
        local sample = parse_powermetrics(pm.value)

        if #sample.clusters > 0 or sample.gpu_mhz then
            m.sustained_clocks = probe.ok({
                clusters = sample.clusters,
                gpu_mhz  = sample.gpu_mhz,
            }, pm.source)
        else
            m.sustained_clocks = probe.unavailable(
                "powermetrics ran but reported no cluster or GPU frequency; "
                .. "its output format may have changed", pm.source)
        end

        m.cpu_power_mw = sample.cpu_power_mw
                         and probe.ok(sample.cpu_power_mw, pm.source)
                         or probe.unavailable(
                                "no 'CPU Power' line in powermetrics output",
                                pm.source)
        m.gpu_power_mw = sample.gpu_power_mw
                         and probe.ok(sample.gpu_power_mw, pm.source)
                         or probe.unavailable(
                                "no 'GPU Power' line in powermetrics output",
                                pm.source)
        m.thermal_pressure = sample.thermal_pressure
                             and probe.ok(sample.thermal_pressure, pm.source)
                             or probe.unavailable(
                                    "no thermal pressure level in output",
                                    pm.source)
    else
        local reason = access.reason or "powermetrics unavailable"
        m.sustained_clocks = probe.unavailable(reason, "powermetrics")
        m.cpu_power_mw     = probe.unavailable(reason, "powermetrics")
        m.gpu_power_mw     = probe.unavailable(reason, "powermetrics")
        m.thermal_pressure = probe.unavailable(reason, "powermetrics")
    end

    -- Die temperature in degrees is a separate claim from thermal pressure,
    -- and Apple silicon does not publish one through powermetrics. Recording
    -- a pressure level in a field named 'temperatures' would be a different
    -- measurement wearing the right label, so the field stays UNAVAILABLE and
    -- names its substitute.
    if os.arch() == "arm64" then
        m.temperatures = probe.unavailable(
            "Apple silicon publishes no die temperature through "
            .. "powermetrics; fields.thermal_pressure carries the thermal "
            .. "pressure level instead, which is a different measurement",
            "powermetrics --samplers thermal")
    else
        m.temperatures = probe.unavailable(
            "not probed on Intel macOS; the smc sampler would be the source",
            "powermetrics --samplers smc")
    end

    local therm = _capture("pmset", {"-g", "therm"})
    m.thermal_notes = therm

    -- Profile A also records memory pressure and the compression and swap
    -- deltas, because unified memory makes them part of the GPU budget.
    local pressure = _capture("memory_pressure", {})
    if pressure.status == "OK" then
        local pct = pressure.value:match(
            "System%-wide memory free percentage:%s*(%d+)")
        m.memory_free_percent = pct and probe.ok(tonumber(pct),
                                                 pressure.source)
                                or probe.unavailable(
                                       "free-percentage line not found",
                                       pressure.source)
    else
        m.memory_free_percent = pressure
    end

    local swap = probe.run("sysctl", {"-n", "vm.swapusage"})
    m.swap_usage = swap

    local vmstat = _capture("vm_stat", {})
    if vmstat.status == "OK" then
        local compressed = vmstat.value:match(
            "Pages occupied by compressor:%s*(%d+)")
        m.pages_compressed = compressed
            and probe.ok(tonumber(compressed), vmstat.source)
            or probe.unavailable("compressor line not found", vmstat.source)
    else
        m.pages_compressed = vmstat
    end

    -- Display path: macOS has no mux, but the presentation scaling matters
    -- ("native 1080p names the render buffer, not the Mac's physical panel").
    m.mux_path = probe.ok("n/a (Apple silicon, integrated GPU only)",
                          "architecture.md section 2")
    m.compositor = probe.ok("WindowServer (Quartz Compositor)", "macOS")

    return m
end

-- --------------------------------------------------- powermetrics ----

-- powermetrics is the only source on macOS for sustained clocks and thermal
-- state, and it refuses to run as anyone but root. Rather than assume one
-- arrangement, this tries the two that do not require a human at a keyboard:
--
--   1. direct  -- the process is already root, or the site has arranged an
--                 equivalent (a setuid wrapper, a launchd helper).
--   2. sudo    -- 'sudo -n', which NEVER prompts. A NOPASSWD sudoers rule for
--                 powermetrics makes this succeed; without one it fails
--                 instantly.
--
-- '-n' is what makes this safe to run unattended. An interactive sudo would
-- block a nightly job forever on a password prompt, and a CI job that hangs
-- is worse than one that reports a gap.
--
-- Anyone running the benchmarks without that arrangement -- and people using
-- the engine will -- gets every powermetrics-derived field marked
-- UNAVAILABLE with the reason, and every other field still captured. The
-- manifest records WHICH path was used, so two runs obtained by different
-- means are never silently compared as though they were alike.
local PM_SAMPLERS = "cpu_power,gpu_power,thermal"
local PM_ARGS = {"--samplers", PM_SAMPLERS, "-n", "1", "-i", "1000"}

local PM_TIMEOUT_MS = 20000

function _powermetrics_capture()
    local direct = probe.run("powermetrics", PM_ARGS,
                             {lines = true, timeout = PM_TIMEOUT_MS})
    if direct.status == "OK" then
        return direct, probe.ok("direct", direct.source)
    end

    local sudoargs = {"-n", "powermetrics"}
    for _, a in ipairs(PM_ARGS) do
        table.insert(sudoargs, a)
    end
    local viasudo = probe.run("sudo", sudoargs,
                              {lines = true, timeout = PM_TIMEOUT_MS})
    if viasudo.status == "OK" then
        return viasudo, probe.ok("sudo-nopasswd", viasudo.source)
    end

    return nil, probe.unavailable(
        ("powermetrics needs root and no passwordless path is configured "
         .. "(direct: %s; sudo -n: %s). See ci/README.md to enable it.")
        :format(_firstline(direct.reason or "failed"),
                _firstline(viasudo.reason or "failed")),
        "powermetrics")
end

-- Parse a powermetrics sample. Public so it can be tested without root:
-- the machine that runs the benchmarks may have powermetrics enabled while
-- the machine that edits this file does not, and a parser nobody can
-- exercise is a parser that silently stops matching after an OS update.
--
-- Returns a table of whatever it recognised. A field it cannot find is
-- absent, never zero -- a missing clock reported as 0 MHz would be a
-- fabricated measurement.
--
-- Apple silicon names its clusters 'E-Cluster' and 'P-Cluster' on the base
-- parts, and 'E-Cluster', 'P0-Cluster', 'P1-Cluster' on Pro and Max. Profile
-- A is an M3 Pro, so every cluster is collected rather than just the first;
-- recording P0 alone would drop half the performance cores.
function parse_powermetrics(text)
    text = text or ""
    local out = {clusters = {}}

    for name, mhz in text:gmatch("([EP]%d*)%-Cluster HW active frequency:"
                                .. "%s*([%d%.]+)") do
        table.insert(out.clusters, {cluster = name, mhz = tonumber(mhz)})
    end

    local gpu_mhz = text:match("GPU HW active frequency:%s*([%d%.]+)")
    if gpu_mhz then
        out.gpu_mhz = tonumber(gpu_mhz)
    end

    local cpu_mw = text:match("CPU Power:%s*([%d%.]+)%s*mW")
    if cpu_mw then
        out.cpu_power_mw = tonumber(cpu_mw)
    end

    local gpu_mw = text:match("GPU Power:%s*([%d%.]+)%s*mW")
    if gpu_mw then
        out.gpu_power_mw = tonumber(gpu_mw)
    end

    local pressure = text:match("[Cc]urrent pressure level:%s*(%a+)")
    if pressure then
        out.thermal_pressure = pressure
    end

    return out
end

-- ---------------------------------------------------------------- Linux ----

function _capture_linux()
    local m = {}

    local osrel = probe.run("cat", {"/etc/os-release"}, {lines = true})
    m.os_name    = _from_block(osrel, "^NAME")
    m.os_version = _from_block(osrel, "^VERSION_ID")
    m.os_build   = probe.run("uname", {"-v"})
    m.kernel     = probe.run("uname", {"-r"})

    local lscpu = _capture("lscpu", {})
    m.cpu_model   = _from_block(lscpu, "Model name")
    m.cpu_logical = _tonumber_record(_from_block(lscpu, "^CPU%(s%)"))

    m.hardware_model = probe.readfile(
        "/sys/devices/virtual/dmi/id/product_name")

    local meminfo = probe.run("cat", {"/proc/meminfo"}, {lines = true})
    local memtotal = _from_block(meminfo, "MemTotal")
    m.memory_bytes = memtotal.status == "OK"
        and probe.ok(tonumber(memtotal.value:match("(%d+)")) * 1024,
                     memtotal.source)
        or memtotal
    m.memory_kind = probe.ok("dedicated", "architecture.md section 2")

    -- One nvidia-smi call for every GPU field: it is the authoritative
    -- device-reported source for the driver version and the power limit,
    -- both of which change results and neither of which can be pinned.
    local query = "name,driver_version,memory.total,power.limit," ..
                  "power.max_limit,clocks.max.sm,temperature.gpu," ..
                  "clocks.sm,pcie.link.gen.current"
    local smi = probe.run("nvidia-smi",
                          {"--query-gpu=" .. query,
                           "--format=csv,noheader,nounits"})
    if smi.status == "OK" then
        local fields = {}
        for part in (smi.value .. ","):gmatch("%s*([^,]*),") do
            table.insert(fields, part:trim())
        end
        local function at(i, numeric)
            local v = fields[i]
            if not v or v == "" or v == "[N/A]" then
                return probe.unavailable("nvidia-smi reported [N/A]",
                                         smi.source)
            end
            return probe.ok(numeric and tonumber(v) or v, smi.source)
        end
        m.gpu_name           = at(1)
        m.gpu_driver_version = at(2)
        m.gpu_memory_mib     = at(3, true)
        m.gpu_power_limit_w  = at(4, true)
        m.gpu_power_max_w    = at(5, true)
        m.gpu_clock_max_mhz  = at(6, true)
        m.gpu_temperature_c  = at(7, true)
        m.sustained_clocks   = at(8, true)
        m.pcie_link_gen      = at(9, true)
    else
        for _, k in ipairs({"gpu_name", "gpu_driver_version", "gpu_memory_mib",
                            "gpu_power_limit_w", "gpu_power_max_w",
                            "gpu_clock_max_mhz", "gpu_temperature_c",
                            "sustained_clocks", "pcie_link_gen"}) do
            m[k] = smi
        end
    end

    -- SM count stands in for "GPU core count" on NVIDIA; CUDA-core counts are
    -- a marketing multiple of it and are not device-reported.
    m.gpu_core_count = _tonumber_record(
        probe.match("nvidia-smi", {"-q"}, "MultiProcessor Count%s*:%s*(%d+)",
                    {lines = true}))

    m.graphics_api = probe.match("vulkaninfo", {"--summary"},
                                 "Vulkan Instance Version:%s*([%d%.]+)",
                                 {lines = true})

    m.storage_model = probe.match("lsblk", {"-d", "-n", "-o", "NAME,MODEL"},
                                  "^%S+%s+(.+)$", {lines = true})

    local ac = probe.readfile("/sys/class/power_supply/AC/online")
    if ac.status ~= "OK" then
        ac = probe.readfile("/sys/class/power_supply/ACAD/online")
    end
    m.ac_state = ac.status == "OK"
        and probe.ok(ac.value == "1" and "AC Power" or "Battery Power",
                     ac.source)
        or ac

    m.cooling_mode = probe.readfile(
        "/sys/firmware/acpi/platform_profile")
    m.temperatures = m.gpu_temperature_c

    -- Which GPU actually renders, on a machine that may have two. glxinfo is
    -- the natural source and also a trap: with no display server answering it
    -- blocks forever, which is the normal state of a headless runner and of a
    -- laptop whose session is locked at 3am. Guard on the display being set
    -- at all, and bound the call even then.
    local display = os.getenv("DISPLAY") or os.getenv("WAYLAND_DISPLAY")
    if display then
        m.mux_path = probe.match("glxinfo", {"-B"},
                                 "OpenGL renderer string:%s*(.+)",
                                 {lines = true, timeout = 5000})
    else
        m.mux_path = probe.unavailable(
            "no display server reachable: DISPLAY and WAYLAND_DISPLAY are "
            .. "both unset, so the render path cannot be queried",
            "glxinfo -B")
    end
    m.compositor = probe.ok(os.getenv("XDG_SESSION_TYPE") or "unknown",
                            "$XDG_SESSION_TYPE")
    m.thermal_notes = probe.unavailable(
        "no aggregated thermal log on Linux", "n/a")

    return m
end

-- ------------------------------------------------------------ toolchain ----

function _capture_toolchain()
    local t = {}

    -- Slang is pinned (ci.md section 6). The version is still recorded here so
    -- the manifest is self-contained and the pin check has something to
    -- compare against.
    t.slang = probe.run("slangc", {"-v"}, {stream = "stderr"})

    t.vulkan_instance = probe.match("vulkaninfo", {"--summary"},
                                    "Vulkan Instance Version:%s*([%d%.]+)",
                                    {lines = true})

    if os.host() == "macosx" then
        t.xcode = probe.match("xcodebuild", {"-version"}, "Xcode%s+([%d%.]+)",
                              {lines = true})
        -- 'metal' and 'metallib' accepting the MSL is B0.1, and the Metal
        -- Toolchain is a separate Xcode component. If it is missing the probe
        -- must say so plainly: this is the only place the MSL is validated.
        t.metal = probe.run("xcrun",
                            {"-sdk", "macosx", "metal", "--version"},
                            {stream = "both"})
    else
        t.gcc = probe.match("gcc", {"--version"}, "(%d+%.%d+%.%d+)")
        t.clang = probe.match("clang", {"--version"}, "version%s+([%d%.]+)",
                              {lines = true})
    end

    t.xmake = probe.match("xmake", {"--version"}, "xmake%s+v([%d%.%+]+)",
                          {lines = true})
    t.clang_format = probe.match("clang-format", {"--version"},
                                 "version%s+([%d%.]+)")
    return t
end

-- --------------------------------------------------------------- roles ----

-- Resolve this machine against ci/machines.json. Returns the registry entry,
-- or a synthetic 'unknown' entry carrying the registry's default role.
--
-- The default is 'development' on purpose. A machine nobody registered must
-- not be able to produce gate evidence just by being unrecognised.
function _resolve_machine(host, fields)
    local registryfile = path.join(os.projectdir(), "ci", "machines.json")
    if not os.isfile(registryfile) then
        return {
            id      = "unregistered",
            label   = "no ci/machines.json in the tree",
            role    = "development",
            profile = nil,
            known   = false,
        }
    end

    local registry = json.loadfile(registryfile)
    local gpu   = probe.available(fields.gpu_name) and fields.gpu_name.value
                  or ""
    local model = probe.available(fields.hardware_model)
                  and fields.hardware_model.value or ""

    for _, entry in ipairs(registry.machines or {}) do
        local m  = entry.match or {}
        local hit = true
        if m.host and m.host ~= host then
            hit = false
        end
        if hit and m.gpu_name_contains
           and not tostring(gpu):find(m.gpu_name_contains, 1, true) then
            hit = false
        end
        if hit and m.hardware_model and m.hardware_model ~= model then
            hit = false
        end
        if hit then
            return {
                id      = entry.id,
                label   = entry.label,
                role    = entry.role,
                profile = entry.profile ~= json.null and entry.profile or nil,
                runs    = entry.runs,
                known   = true,
            }
        end
    end

    return {
        id      = "unknown",
        label   = ("unrecognised machine: %s / %s / %s")
                  :format(host, model ~= "" and model or "?",
                          gpu ~= "" and gpu or "?"),
        role    = registry.unknown_machine_role or "development",
        profile = nil,
        known   = false,
    }
end

-- ---------------------------------------------------------------- entry ----

-- Capture the machine manifest. 'opt.profile' forces a profile id; otherwise
-- it is resolved from ci/machines.json, which is what CI wants.
--
-- 'opt.force_host' runs another platform's probe set on this machine. Every
-- probe then fails and reports UNAVAILABLE, which is the point: it exercises
-- the parsing and nil-handling of a path that would otherwise only ever run
-- on hardware the author cannot reach. Neither developer can run the other's
-- platform, so without this the Linux probes would first execute on ci-linux
-- at 3am. A forced manifest is marked and can never be archived.
function capture(opt)
    opt = opt or {}
    local host = opt.force_host or os.host()

    local fields = (host == "macosx") and _capture_macos(opt)
                   or (host == "linux") and _capture_linux()
                   or nil

    local manifest = {
        schema_version  = 1,
        captured_at_utc = os.date("!%Y-%m-%dT%H:%M:%SZ"),
        host            = host,
        arch            = os.arch(),
    }

    if opt.force_host and opt.force_host ~= os.host() then
        manifest.smoke_test = ("probe set for %q was forced on a %q host; "
                               .. "every value here is meaningless and this "
                               .. "manifest must never be archived")
                              :format(opt.force_host, os.host())
    end

    if not fields then
        manifest.status = "UNSUPPORTED_HOST"
        manifest.reason =
            ("no machine probes are defined for host %q; the CI machines are "
             .. "Linux and macOS (ci.md section 2)"):format(host)
        return manifest
    end

    manifest.fields    = fields
    manifest.toolchain = _capture_toolchain()

    -- Which machine is this, and is anything measured on it evidence?
    local registered = _resolve_machine(host, fields)
    manifest.machine = registered

    -- The profile this machine is claimed to be. An explicit --profile wins,
    -- then the registry. A development machine has no profile of its own: it
    -- can run a mock for either, but it IS neither.
    local profile = PROFILES[opt.profile] or PROFILES[registered.profile]

    -- Render target and frame rate are contract, not measurement.
    manifest.contract = {
        render_target_px    = {1920, 1080},
        reconstruction      = "none",
        required_frame_rate = profile and profile.required_frame_rate or nil,
    }

    -- Compare what the device reports against what the plan claims the
    -- qualification hardware is. A mismatch is recorded and surfaced; it is
    -- never silently accepted, and it is never "fixed" by overwriting the
    -- measured value.
    --
    -- This only means anything for a qualification machine. On a development
    -- box the hardware differs by design, so the comparison is skipped rather
    -- than reported as a problem -- while the role keeps its results out of
    -- the evidence path.
    local mismatches = {}
    if profile and registered.role == "qualification" then
        local reported_gpu = fields.gpu_name
        if probe.available(reported_gpu) and
           not tostring(reported_gpu.value):find(profile.claimed_gpu, 1, true)
        then
            table.insert(mismatches, {
                field    = "gpu_name",
                claimed  = profile.claimed_gpu,
                reported = reported_gpu.value,
            })
        end
        local mem = fields.memory_bytes
        if probe.available(mem) then
            local gb = math.floor(mem.value / (1024 * 1024 * 1024) + 0.5)
            if gb ~= profile.claimed_memory_gb then
                table.insert(mismatches, {
                    field    = "memory_gb",
                    claimed  = profile.claimed_memory_gb,
                    reported = gb,
                })
            end
        end
    end

    manifest.profile = {
        id               = profile and profile.id or nil,
        source           = opt.profile and "command line"
                           or (registered.known and "ci/machines.json"
                               or "unresolved"),
        claimed_hardware = profile and profile.claimed_gpu or nil,
        matches_claim    = (#mismatches == 0),
        mismatches       = mismatches,
        checked          = (profile ~= nil
                            and registered.role == "qualification"),
    }

    -- The single field downstream tooling reads to decide whether a bundle
    -- from this machine may be archived as evidence. A forced-host smoke run
    -- is never eligible, whatever machine it ran on.
    manifest.evidence_eligible = (registered.role == "qualification")
                                 and manifest.smoke_test == nil

    -- Count what could not be read, so a caller can refuse to treat a
    -- half-empty manifest as a complete one.
    -- Meta fields describe WHY measurements are missing; counting them as
    -- missing measurements reports the same gap twice.
    local META = {power_access = true}

    local unavailable = {}
    for group, tbl in pairs({fields = fields, toolchain = manifest.toolchain}) do
        for name, record in pairs(tbl) do
            if type(record) == "table" and record.status == "UNAVAILABLE"
               and not META[name] then
                table.insert(unavailable, group .. "." .. name)
            end
        end
    end
    table.sort(unavailable)
    manifest.unavailable       = unavailable
    manifest.unavailable_count = #unavailable

    return manifest
end
