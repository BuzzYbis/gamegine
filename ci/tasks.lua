-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- tasks.lua -- the CI entry points, exposed as xmake tasks.
--
-- Included from the root xmake.lua. Everything CI does is reachable from the
-- command line on a developer's laptop with the same invocation the runner
-- uses, because a check that can only be run by pushing is a check nobody
-- runs before pushing.
--
--   xmake ci-machine     capture this machine's manifest
--   xmake ci-pins        verify the pinned inputs           (gate B0.4)
--   xmake ci-schema      validate a JSON document
--   xmake ci-bundle      validate a result bundle layout
--   xmake ci-format      check, or fix, source formatting
--   xmake ci-check       run the whole push tier locally

-- ------------------------------------------------------------ ci-machine ---

task("ci-machine")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-machine [options]",
        description = "Capture the machine manifest for this runner.",
        options = {
            {'o', "output",  "kv", nil, "Write JSON here (default: stdout)."},
            {'p', "profile", "kv", nil, "Force a profile id, L or A."},
            {'j', "json-only", "k", nil, "Print JSON only, no summary."},
            {'n', "no-power", "k", nil, "Skip the powermetrics capture "
                                        .. "(macOS). Faster, but sustained "
                                        .. "clocks and thermal state are "
                                        .. "then UNAVAILABLE."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("core.base.json")
        import("machine", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local manifest = machine.capture({
            profile    = option.get("profile"),
            skip_power = option.get("no-power"),
        })
        local encoded  = json.encode(manifest)

        local output = option.get("output")
        if output then
            os.mkdir(path.directory(output))
            io.writefile(output, encoded)
        end

        if option.get("json-only") then
            if not output then
                print(encoded)
            end
            return
        end

        if manifest.status == "UNSUPPORTED_HOST" then
            style.say("${warn}%s", manifest.reason)
            return
        end

        style.say("${bold}machine manifest${reset}  host=%s arch=%s",
               manifest.host, manifest.arch)
        style.say("  %s", manifest.machine.label)

        if manifest.evidence_eligible then
            style.say("${ok}  qualification machine${reset}  "
                   .. "profile %s  --  results from here are gate evidence",
                   manifest.profile.id or "?")
        else
            style.say("${warn}  development machine${reset}  --  runs "
                   .. "here are mock. They exercise the harness; they do not "
                   .. "substantiate a claim.")
            if not manifest.machine.known then
                style.say("${warn}  this machine is not in "
                       .. "ci/machines.json${reset}, so it defaulted to "
                       .. "'development'. Register it if that is wrong.")
            end
        end

        -- A profile mismatch is surfaced, never resolved. The plan names the
        -- qualification hardware; the device reports what it actually is.
        -- Only checked on a qualification machine: elsewhere the hardware
        -- differs by design and the role already says so.
        if manifest.profile.checked and not manifest.profile.matches_claim then
            style.say("${bad}profile mismatch against architecture.md "
                   .. "section 2:")
            for _, m in ipairs(manifest.profile.mismatches) do
                style.say("${bad}  %-14s plan claims %-28s device "
                       .. "reports %s", m.field, tostring(m.claimed),
                       tostring(m.reported))
            end
            style.say("${dim}  Recorded as measured. This machine is registered "
                   .. "as qualification hardware but does not match the "
                   .. "claim; reconcile the registry or the claim before "
                   .. "archiving anything from it.")
        end

        -- Fields that share a cause are grouped under it. Repeating one long
        -- reason once per field buries the distinct gaps among the copies.
        if manifest.unavailable_count > 0 then
            style.say("${warn}%d field(s) UNAVAILABLE "
                   .. "(never counted as a pass):",
                   manifest.unavailable_count)

            local order, byreason = {}, {}
            for _, name in ipairs(manifest.unavailable) do
                local group, key = name:match("^(%w+)%.(.+)$")
                local record = manifest[group] and manifest[group][key]
                local reason = record and record.reason or "no reason recorded"
                if not byreason[reason] then
                    byreason[reason] = {}
                    table.insert(order, reason)
                end
                table.insert(byreason[reason], key)
            end

            for _, reason in ipairs(order) do
                style.say("${dim}  %s", reason)
                style.say("${dim}      %s",
                       table.concat(byreason[reason], ", "))
            end
        end

        if output then
            style.say("${ok}wrote %s", output)
        else
            print(encoded)
        end
    end)
task_end()

-- --------------------------------------------------------------- ci-pins ---

task("ci-pins")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-pins [options]",
        description = "Verify the pinned inputs in ci/pins.json (gate B0.4).",
        options = {
            {'o', "output", "kv", nil, "Write the JSON report here."},
            {'r', "release", "kv", nil, "Check pins due by this release."},
            {'u', "allow-unpinned", "k", nil, "Report unfilled pins instead "
                                              .. "of failing. The push tier "
                                              .. "uses this; the gate does "
                                              .. "not."},
            {'t', "tools-optional", "k", nil, "Report tool-version pins the "
                                              .. "runner cannot check, "
                                              .. "instead of failing. File "
                                              .. "pins stay strict."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("core.base.json")
        import("pins", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local report, ok = pins.verify({
            release        = option.get("release"),
            allow_unpinned = option.get("allow-unpinned"),
            tools_optional = option.get("tools-optional"),
        })

        local output = option.get("output")
        if output then
            os.mkdir(path.directory(output))
            io.writefile(output, json.encode(report))
        end

        style.say("${bold}pinned inputs${reset}  release=%s  (ci.md section 6)",
               report.release)
        for _, e in ipairs(report.pinned) do
            local colour = "${ok}"
            if e.status == "UNPINNED" or e.status == "MISMATCH"
               or e.status == "PROBE_UNAVAILABLE" then
                colour = "${bad}"
            elseif e.status ~= "PINNED_VERIFIED" then
                colour = "${warn}"
            end
            if e.blocking == false then
                colour = "${warn}"
            end
            if e.status == "UNVERIFIABLE_HERE" then
                colour = "${warn}"
            end
            style.say(colour .. "  %-16s %-18s${reset} %s", e.name, e.status,
                   e.detected or e.pinned or "")
            if e.detail then
                style.say("${dim}    %s", e.detail)
            end
        end

        style.say("${bold}recorded per run${reset}  (cannot be pinned)")
        for _, e in ipairs(report.recorded) do
            style.say("  %-16s %s %s", e.name,
                      style.statusword(e.status, 18),
                      tostring(e.value or e.reason or ""))
        end

        if ok and report.allow_unpinned then
            local unpinned = 0
            for _, e in ipairs(report.pinned) do
                if e.status == "UNPINNED" then
                    unpinned = unpinned + 1
                end
            end
            if unpinned > 0 then
                style.say("")
                style.say("${warn}%d pin(s) still unfilled.${reset} "
                       .. "Reported, not failed, because --allow-unpinned "
                       .. "was given. Gate B0.4 runs without it and will "
                       .. "block the end of R0.", unpinned)
                return
            end
        end

        if not ok then
            style.say("")
            style.say("${bad}B0.4 does not pass.${reset} Gate B0.4 is "
                   .. "'required': Slang, meshoptimizer and clusterlod.h "
                   .. "versions pinned and recorded in every manifest.")
            style.say("${dim}Fill in ci/pins.json with the exact tag or commit "
                   .. "you vendored. Do not invent a version to turn this "
                   .. "green.")
            raise("pinned inputs are not satisfied")
        end
        style.say("${ok}B0.4 satisfied on this machine.")
    end)
task_end()

-- ------------------------------------------------------------- ci-schema ---

task("ci-schema")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-schema --schema=<file> <document.json>...",
        description = "Validate JSON documents against a schema.",
        options = {
            {'s', "schema", "kv", nil, "Schema file to validate against."},
            {nil, "documents", "vs", nil, "Documents to validate."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("jsonschema",
               {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local schema = option.get("schema")
            or path.join(os.projectdir(), "ci", "schema",
                         "run-manifest.schema.json")
        local documents = option.get("documents") or {}
        if #documents == 0 then
            raise("no documents given; pass one or more JSON files")
        end

        local failed = 0
        for _, doc in ipairs(documents) do
            local ok, errors = jsonschema.validate_file(doc, schema)
            if ok then
                style.say("${ok}ok${reset}   %s", doc)
            else
                failed = failed + 1
                style.say("${bad}FAIL${reset} %s", doc)
                for _, e in ipairs(errors) do
                    style.say("${dim}       %s", e)
                end
            end
        end
        if failed > 0 then
            raise("%d document(s) failed schema validation", failed)
        end
    end)
task_end()

-- ------------------------------------------------------------- ci-bundle ---

task("ci-bundle")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-bundle <bundle-directory>...",
        description = "Validate the layout of a result bundle.",
        options = {
            {nil, "bundles", "vs", nil, "Bundle directories to validate."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("bundle", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local bundles = option.get("bundles") or {}
        if #bundles == 0 then
            raise("no bundles given; pass one or more directories")
        end

        local failed = 0
        for _, dir in ipairs(bundles) do
            local report, ok = bundle.validate(dir)
            if ok then
                style.say("${ok}ok${reset}   %s  (gate %s, %s)", dir,
                       report.gate or "?", report.status or "?")
            else
                failed = failed + 1
                style.say("${bad}FAIL${reset} %s", dir)
            end
            for _, f in ipairs(report.findings) do
                local colour = f.severity == "error" and "${bad}"
                               or "${warn}"
                style.say(colour .. "       %-8s${reset} %s", f.severity,
                       f.message)
                if f.why then
                    style.say("${dim}                %s", f.why)
                end
            end
        end
        if failed > 0 then
            raise("%d bundle(s) failed layout validation", failed)
        end
    end)
task_end()

-- ------------------------------------------------------------- ci-format ---

task("ci-format")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-format [--fix]",
        description = "Check first-party sources against .clang-format.",
        options = {
            {nil, "fix", "k", nil, "Rewrite files in place instead of "
                                   .. "reporting them."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("probe", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("sources", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local version = probe.match("clang-format", {"--version"},
                                    "version%s+([%d%.]+)")
        if version.status ~= "OK" then
            raise("clang-format is not available: %s", version.reason)
        end

        local files = sources.list()
        if #files == 0 then
            style.say("${warn}no first-party sources yet; nothing to "
                   .. "check")
            return
        end

        -- Not named 'style': that is the output module imported above, and
        -- a local of the same name shadows it for the rest of the function.
        local style_arg = "file:"
                          .. path.join(os.projectdir(), ".clang-format")
        local fix = option.get("fix")
        local offenders = {}

        for _, f in ipairs(files) do
            if fix then
                os.execv("clang-format", {"-i", "--style=" .. style_arg, f})
            else
                local formatted = os.tmpfile()
                local code = os.execv("clang-format", {"--style=" .. style_arg, f},
                                      {stdout = formatted, try = true})
                if code == 0 then
                    if io.readfile(formatted) ~= io.readfile(f) then
                        table.insert(offenders, f)
                    end
                end
                os.tryrm(formatted)
            end
        end

        if fix then
            style.say("${ok}formatted %d file(s) with clang-format %s",
                   #files, version.value)
            return
        end

        if #offenders > 0 then
            for _, f in ipairs(offenders) do
                style.say("${bad}needs formatting${reset} %s",
                       path.relative(f, os.projectdir()))
            end
            raise("%d of %d file(s) are not formatted; run "
                  .. "'xmake ci-format --fix'", #offenders, #files)
        end
        style.say("${ok}%d file(s) formatted correctly "
               .. "(clang-format %s)", #files, version.value)
    end)
task_end()

-- -------------------------------------------------------------- ci-check ---

task("ci-check")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-check",
        description = "Run the push tier locally, in the runner's order.",
        options = {
            {nil, "keep-going", "k", nil, "Run every step even after one "
                                          .. "fails."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("core.base.task")

        -- The push tier, in the order ci.md section 3 lists it. Cheap checks
        -- first: a broken format or an unpinned dependency should not wait
        -- behind a compile.
        local steps = {
            {name = "format", task = "ci-format"},
            {name = "lua",    task = "ci-lua"},
            {name = "tests",  task = "ci-test"},
            {name = "unit",   task = "ci-unit"},
            -- B0.4 is a release-gate condition, so the local push tier
            -- reports unfilled pins rather than failing on them.
            -- Locally the toolchain is present, so tool pins are checked
            -- for real; only an unfilled pin is downgraded.
            {name = "pins",   task = "ci-pins",
             options = {["allow-unpinned"] = true}},
        }

        local failures = {}
        for _, step in ipairs(steps) do
            style.say("")
            style.say("${bold}== %s ==", step.name)
            local failed = nil
            try
            {
                function ()
                    task.run(step.task, step.options or {})
                end,
                catch
                {
                    function (errors)
                        failed = tostring(errors)
                    end
                }
            }
            if failed then
                table.insert(failures, {name = step.name, reason = failed})
                if not option.get("keep-going") then
                    break
                end
            end
        end

        style.say("")
        if #failures > 0 then
            for _, f in ipairs(failures) do
                style.say("${bad}%s failed${reset}", f.name)
            end
            raise("push tier failed: %d step(s)", #failures)
        end
        style.say("${ok}push tier passed")
    end)
task_end()

-- ---------------------------------------------------------- ci-inventory ---

task("ci-inventory")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-inventory [--github]",
        description = "Report which CI inputs exist yet, and which do not.",
        options = {
            {'g', "github", "k", nil, "Also write key=value pairs to "
                                      .. "$GITHUB_OUTPUT."},
            {'o', "output", "kv", nil, "Write the JSON report here."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("core.base.json")
        import("sources", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("pins",    {rootdir = path.join(os.projectdir(), "ci", "lua")})

        -- A job with no inputs must say so. ci.md section 4: "Silent skips
        -- are how a backend quietly stops being tested." Every count here
        -- exists so a workflow can print 'nothing to check yet, and here is
        -- what makes this real' rather than reporting a green tick.
        local cpp     = sources.list()
        local units   = sources.translation_units()
        local shaders = os.files(path.join(os.projectdir(), "shaders",
                                           "**.slang"))
        local bundles = os.dirs(path.join(os.projectdir(), "results", "*"))

        local report, pins_ok = pins.verify({allow_unpinned = true})
        local unpinned = {}
        for _, e in ipairs(report.pinned) do
            if e.status == "UNPINNED" then
                table.insert(unpinned, e.name)
            end
        end

        -- ci.md section 11 build-out. Step 3 is the R0 exit criterion.
        local buildout = {
            {step = 1, what = "self-hosted runners, overnight schedule, "
                              .. "cancel-on-login", done = false},
            {step = 2, what = "push tier: host build, Slang both targets, "
                              .. "capability check, CPU fixtures",
             done = false, partial = true},
            {step = 3, what = "golden-image harness, one trivial fixture, "
                              .. "green on both machines (R0 EXIT)",
             done = false},
        }

        local inventory = {
            schema_version   = 1,
            checked_at_utc   = os.date("!%Y-%m-%dT%H:%M:%SZ"),
            cpp_sources      = #cpp,
            translation_units = #units,
            slang_shaders    = #shaders,
            result_bundles   = #bundles,
            pins_complete    = (#unpinned == 0),
            unpinned         = unpinned,
            ci_buildout      = buildout,
            first_party_roots = sources.roots(),
        }

        style.say("${bold}CI inventory${reset}  (what exists, and what does "
               .. "not)")
        style.say("  first-party C++ sources   %d  (%d translation units)",
               inventory.cpp_sources, inventory.translation_units)
        style.say("  Slang shaders             %d", inventory.slang_shaders)
        style.say("  result bundles            %d", inventory.result_bundles)
        style.say("  pins complete             %s%s",
                  style.flag(inventory.pins_complete),
                  #unpinned > 0 and ("  (missing: "
                                     .. table.concat(unpinned, ", ") .. ")")
                                 or "")
        style.say("${bold}ci.md section 11 build-out${reset}")
        for _, b in ipairs(buildout) do
            local mark = b.done and "${ok}done   "
                         or b.partial and "${warn}partial"
                         or "${warn}pending"
            style.say("  %s${reset}  step %d  %s", mark, b.step, b.what)
        end

        local output = option.get("output")
        if output then
            os.mkdir(path.directory(output))
            io.writefile(output, json.encode(inventory))
        end

        if option.get("github") then
            local gh = os.getenv("GITHUB_OUTPUT")
            if gh then
                local lines = {
                    ("cpp_sources=%d"):format(inventory.cpp_sources),
                    ("slang_shaders=%d"):format(inventory.slang_shaders),
                    -- The build job keys off translation units: a static
                    -- library with only headers cannot be archived.
                    ("has_cpp=%s")
                        :format(tostring(inventory.translation_units > 0)),
                    ("has_headers=%s")
                        :format(tostring(inventory.cpp_sources > 0)),
                    ("has_shaders=%s")
                        :format(tostring(inventory.slang_shaders > 0)),
                    ("pins_complete=%s")
                        :format(tostring(inventory.pins_complete)),
                }
                local existing = io.readfile(gh) or ""
                io.writefile(gh, existing .. table.concat(lines, "\n") .. "\n")
            else
                style.say("${warn}--github given but $GITHUB_OUTPUT is "
                       .. "not set; nothing written")
            end
        end
    end)
task_end()

-- --------------------------------------------------------------- ci-test ---

task("ci-test")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-test",
        description = "Run the CI tooling's own tests.",
        options = {}
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        -- These guard parsers and validators that run on machines nobody is
        -- watching. They need no GPU and no engine, so they run in the cloud
        -- push tier alongside the other cheap checks.
        local suites = {"probe_test", "powermetrics_test",
                        "jsonschema_test", "machine_test",
                        "bench_test"}

        local total, failed = 0, 0
        for _, name in ipairs(suites) do
            local suite = import(name,
                {rootdir = path.join(os.projectdir(), "ci", "test")})
            local t = testing.new(name)
            suite.run(t)

            total = total + t.passed + #t.failures
            if #t.failures == 0 then
                style.say("${ok}ok${reset}   %-22s %d checks", name,
                       t.passed)
            else
                failed = failed + #t.failures
                style.say("${bad}FAIL${reset} %-22s %d passed, %d failed",
                       name, t.passed, #t.failures)
                for _, f in ipairs(t.failures) do
                    style.say("${bad}       %s${reset}", f.label)
                    if f.detail ~= "" then
                        style.say("${dim}         %s", f.detail)
                    end
                end
            end
        end

        if failed > 0 then
            raise("%d of %d checks failed", failed, total)
        end
        style.say("${ok}%d checks passed", total)
    end)
task_end()

-- ---------------------------------------------------------------- bench ---

task("bench")
    set_category("plugin")
    set_menu {
        usage       = "xmake bench --tier=T1 --out=results/ci/<rev>",
        description = "Run a benchmark tier and emit a result bundle.",
        options = {
            {'o', "out",     "kv", nil, "Bundle output directory."},
            {'t', "tier",    "kv", nil, "T1 nightly, T2 weekly, T3 gate."},
            {'b', "backend", "kv", nil, "vulkan, metal, cpu-reference, "
                                        .. "indexed-oracle."},
            {'p', "profile", "kv", nil, "Profile id, L or A."},
            {'m', "mode",    "kv", nil, "warm, engine-cold, storage-cold."},
            {'s', "scene",   "kv", nil, "Fixture id. Default: all."},
            {nil, "all-fixtures", "k", nil, "Run every registered fixture."},
            {nil, "runs",    "kv", nil, "Override the tier's run count."},
            {nil, "no-power", "k", nil, "Skip the powermetrics capture."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("bench", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local out = option.get("out")
        if not out then
            raise("no --out given; a run must say where its bundle goes")
        end

        -- The manifest records the actual command (benchmarks.md section 6),
        -- so it is reconstructed here rather than guessed at read time.
        local parts = {"xmake bench"}
        for _, name in ipairs({"out", "tier", "backend", "profile", "mode",
                               "scene", "runs"}) do
            local v = option.get(name)
            if v then
                table.insert(parts, ("--%s=%s"):format(name, tostring(v)))
            end
        end

        local dir, report, ok, manifest = bench.run({
            out        = out,
            tier       = option.get("tier"),
            backend    = option.get("backend"),
            profile    = option.get("profile"),
            mode       = option.get("mode"),
            scene      = option.get("scene"),
            runs       = option.get("runs") and tonumber(option.get("runs")),
            skip_power = option.get("no-power"),
            command    = table.concat(parts, " "),
        })

        style.say("${bold}%s / %s${reset}  status=%s", manifest.benchmark,
               manifest.tier, manifest.status)
        style.say("  bundle           %s", dir)
        style.say("  machine role     %s", manifest.machine_role)
        style.say("  evidence         %s",
               manifest.evidence_eligible and "eligible"
               or "${warn}NOT eligible (mock run)${reset}")
        if manifest.status_reason then
            style.say("${dim}  %s", manifest.status_reason)
        end

        for _, f in ipairs(report.findings or {}) do
            local colour = f.severity == "error" and "${bad}"
                           or "${warn}"
            style.say(colour .. "  %-8s${reset} %s", f.severity, f.message)
        end

        if not ok then
            raise("the harness emitted a bundle that does not validate; "
                  .. "this is a harness bug, not a run failure")
        end
        style.say("${ok}bundle validates against the frozen schema")
    end)
task_end()

-- --------------------------------------------------------------- ci-lua ---

task("ci-lua")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-lua",
        description = "Check the CI Lua for dead code (the -Wunused of the "
                      .. "scripting side).",
        options = {}
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("lualint", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local findings, count = lualint.check()
        if #findings == 0 then
            style.say("${ok}%d Lua file(s) clean", count)
            return
        end
        for _, f in ipairs(findings) do
            style.say("${bad}%s:%d${reset} %s", f.file, f.line,
                   f.message)
        end
        raise("%d finding(s) in %d Lua file(s)", #findings, count)
    end)
task_end()

-- -------------------------------------------------------------- ci-tidy ---

task("ci-tidy")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-tidy",
        description = "Static analysis of first-party C++ (clang-tidy).",
        options = {
            {nil, "fix", "k", nil, "Apply clang-tidy's suggested fixes."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("probe",   {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("sources", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        -- Homebrew keeps clang-tidy out of PATH because it would shadow
        -- Apple's clang; look there before giving up.
        local candidates = {
            "clang-tidy",
            "/opt/homebrew/opt/llvm/bin/clang-tidy",
            "/usr/local/opt/llvm/bin/clang-tidy",
        }
        local tidy
        for _, c in ipairs(candidates) do
            if probe.run(c, {"--version"}).status == "OK" then
                tidy = c
                break
            end
        end
        if not tidy then
            raise("clang-tidy not found. Tried: %s",
                  table.concat(candidates, ", "))
        end

        -- clang-tidy needs the real compile flags; xmake keeps
        -- compile_commands.json up to date at the project root.
        local database = path.join(os.projectdir(), "compile_commands.json")
        if not os.isfile(database) then
            raise("compile_commands.json is missing; run 'xmake build' first")
        end

        local units = sources.translation_units()
        if #units == 0 then
            style.say("${warn}no first-party translation units yet; "
                   .. "nothing to analyse")
            return
        end

        local failed = 0
        for _, unit in ipairs(units) do
            local relative = path.relative(unit, os.projectdir())
            local argv = {"--config-file="
                          .. path.join(os.projectdir(), ".clang-tidy"),
                          "-p", os.projectdir(), "--quiet"}
            if option.get("fix") then
                table.insert(argv, "--fix")
            end
            table.insert(argv, unit)

            -- allow_empty because clang-tidy prints nothing when clean;
            -- accept 1 so its findings arrive as output rather than as a
            -- failed probe, which is what lets them be reported per file.
            local record = probe.run(tidy, argv,
                                     {lines = true, stream = "both",
                                      allow_empty = true, accept = {0, 1},
                                      timeout = 300000})
            if record.status ~= "OK" then
                failed = failed + 1
                style.say("${bad}FAIL${reset} %s ${dim}(clang-tidy could not "
                          .. "run)${reset}", relative)
                style.say("${dim}       %s${reset}", record.reason or "")
            else
                -- A finding usually lives in a HEADER the unit included, not
                -- in the unit itself. Reporting only the unit sends the
                -- reader to the wrong file, so each finding names its own.
                local findings = {}
                for line in tostring(record.value):gmatch("[^\n]+") do
                    if line:find("error:") or line:find("warning:") then
                        table.insert(findings, line)
                    end
                end
                if #findings == 0 then
                    style.say("${ok}ok${reset}   %s", relative)
                else
                    failed = failed + 1
                    style.say("${bad}FAIL${reset} %s ${dim}(%d finding(s))"
                              .. "${reset}", relative, #findings)
                    for _, line in ipairs(findings) do
                        style.say("${dim}       %s${reset}", line)
                    end
                end
            end
        end

        if failed > 0 then
            raise("%d translation unit(s) have findings", failed)
        end
        style.say("${ok}%d translation unit(s) clean", #units)
    end)
task_end()

-- ------------------------------------------------------------- ci-style ---

task("ci-style")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-style",
        description = "Preview the output palette in your own terminal.",
        options = {}
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        style.say("${bold}palette preview${reset}  "
                  .. "-- every line below must be readable on YOUR background")
        style.say("")
        style.say("  ${ok}ok${reset}       success   #2E8B2E   "
                  .. "worst-case contrast 4.33")
        style.say("  ${bad}FAIL${reset}     failure   #D1242F   "
                  .. "worst-case contrast 4.00")
        style.say("  ${warn}warning${reset}  caution   #9A6700   "
                  .. "worst-case contrast 4.31")
        style.say("  ${note}note${reset}     detail    #1F6FEB   "
                  .. "worst-case contrast 4.53")
        style.say("  ${dim}dim${reset}      context   #6E7781   "
                  .. "worst-case contrast 4.55")
        style.say("")
        style.say("${dim}4.58 is the ceiling for any colour against both "
                  .. "black and white, so these are within 0.3 of the best "
                  .. "possible.${reset}")
        style.say("${dim}If a line is still hard to read, say which one and "
                  .. "what your background is; the hue can move without "
                  .. "losing contrast.${reset}")
        style.say("")
        style.say("${dim}NO_COLOR=1 disables colour entirely.${reset}")
    end)
task_end()

-- -------------------------------------------------------------- ci-unit ---

task("ci-unit")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-unit [--filter=<pattern>]",
        description = "Build and run the C++ unit tests.",
        options = {
            {'f', "filter", "kv", nil, "GoogleTest filter, e.g. 'CoreError.*'."},
            {'a', "show-all", "k", nil, "Show every test, not just "
                                        .. "failures."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("probe",   {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("sources", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        -- 'xmake test' exists, but its summary is printed by xmake itself in
        -- full-brightness truecolour that no project setting can reach. This
        -- task runs the same binary and reports through style.lua, so unit
        -- test output is readable on the same backgrounds as everything else.
        if #sources.translation_units() == 0 then
            style.say("${warn}no translation units yet; nothing to test"
                      .. "${reset}")
            return
        end

        local built = os.execv("xmake", {"build", "--yes", "tests"},
                               {stdout = os.tmpfile(), stderr = os.tmpfile(),
                                try = true})
        if built ~= 0 then
            raise("the test target failed to build")
        end

        local binaries = os.files(path.join(os.projectdir(), "build",
                                            "**", "tests"))
        if #binaries == 0 then
            raise("built the test target but found no binary under build/")
        end

        local argv = {"--gtest_color=no"}
        if option.get("filter") then
            table.insert(argv, "--gtest_filter=" .. option.get("filter"))
        end

        local record = probe.run(binaries[1], argv,
                                 {lines = true, stream = "both",
                                  accept = {0, 1}, timeout = 600000})
        if record.status ~= "OK" then
            raise("could not run the tests: %s", record.reason)
        end

        -- Parse GoogleTest's plain output rather than trusting an exit code,
        -- so the summary names what failed.
        local passed, failed, suites = 0, {}, nil
        for line in record.value:gmatch("[^\n]+") do
            local n = line:match("^%[%s*PASSED%s*%]%s+(%d+) test")
            if n then
                passed = tonumber(n)
            end
            local name = line:match("^%[%s*FAILED%s*%]%s+([%w_]+%.[%w_]+)")
            if name then
                local seen = false
                for _, f in ipairs(failed) do
                    if f == name then
                        seen = true
                    end
                end
                if not seen then
                    table.insert(failed, name)
                end
            end
            local ran = line:match("(%d+ tests? from %d+ test suites? ran)")
            if ran then
                suites = ran
            end
            if option.get("show-all") then
                local ok_name = line:match("^%[%s*OK%s*%]%s+(.+)$")
                if ok_name then
                    style.say("  ${ok}pass${reset}  %s", ok_name)
                end
            end
        end

        for _, name in ipairs(failed) do
            style.say("  ${bad}FAIL${reset}  %s", name)
        end

        if #failed > 0 then
            style.say("")
            style.say("${bad}%d failed${reset}, %d passed%s", #failed, passed,
                      suites and ("   (" .. suites .. ")") or "")
            raise("%d unit test(s) failed", #failed)
        end

        style.say("${ok}%d test(s) passed${reset}%s", passed,
                  suites and ("   ${dim}(" .. suites .. ")${reset}") or "")
    end)
task_end()
