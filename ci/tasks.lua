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
--   xmake format         fix source formatting (overrides xmake's plugin)
--   xmake ci-shaders     compile every kernel     (gates B0.1, B0.2)
--   xmake ci-shader-hash compute, or stamp, the shader hash
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
        description = "Check first-party sources against .clang-format "
                      .. "and the BDE rules it cannot express.",
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
        -- clang-format, then the BDE pass for the rules clang-format cannot
        -- keep. The editor's format-on-save runs clang-format alone and
        -- undoes those; this puts them back before a push.
        import("formatter", {rootdir = path.join(os.projectdir(), "ci", "lua")})

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

        local fix = option.get("fix")
        local offenders = {}

        for _, f in ipairs(files) do
            local original = io.readfile(f)
            local text, reason = formatter.format_text(original, f)
            if text == nil then
                raise("%s: %s", path.relative(f, os.projectdir()), reason)
            elseif text ~= original then
                if fix then
                    io.writefile(f, text)
                else
                    table.insert(offenders, f)
                end
            end
        end

        if fix then
            style.say("${ok}formatted %d file(s): clang-format %s, then the "
                   .. "BDE pass", #files, version.value)
            return
        end

        if #offenders > 0 then
            for _, f in ipairs(offenders) do
                style.say("${bad}needs formatting${reset} %s",
                       path.relative(f, os.projectdir()))
            end
            raise("%d of %d file(s) are not formatted; run "
                  .. "'xmake format'", #offenders, #files)
        end
        style.say("${ok}%d file(s) formatted correctly "
               .. "(clang-format %s)", #files, version.value)
    end)
task_end()

-- ---------------------------------------------------------------- format ---

-- Overrides xmake's built-in 'format' plugin, which runs clang-format ALONE
-- and would undo the BDE tags and banners just before a push. A project
-- task of the same name takes precedence (checked on xmake 3.1.1), so the
-- obvious command does the right thing.
task("format")
    set_category("plugin")
    set_menu {
        usage       = "xmake format",
        description = "Format first-party sources: clang-format, then the "
                      .. "BDE pass. Same as 'xmake ci-format --fix'.",
        options = {}
    }
    on_run(function ()
        import("core.base.task")
        task.run("ci-format", {fix = true})
    end)
task_end()

-- ------------------------------------------------------------ ci-shaders ---

task("ci-shaders")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-shaders [options]",
        description = "Compile every Slang kernel and report "
                      .. "(gates B0.1, B0.2).",
        options = {
            {'t', "targets", "kv", nil, "Targets, comma-separated: spirv, "
                                        .. "msl. Default: the configured "
                                        .. "'shader-targets', else spirv on "
                                        .. "Linux and msl on macOS."},
            {'o', "output", "kv", nil, "Also write the index JSON here."},
            {nil, "self-test", "k", nil, "Compile the fixtures in "
                                         .. "ci/test/fixtures/shaders and "
                                         .. "check every outcome against its "
                                         .. "//@expect line, instead of "
                                         .. "compiling shaders/."},
        }
    }
    on_run(function ()
        import("core.base.option")
        import("core.project.config")
        import("shaders", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        config.load()
        local targets = shaders.parse_targets(
            option.get("targets") or config.get("shader-targets"))
        local tc = shaders.toolchain(targets)
        shaders.require_toolchain(tc)

        cprint("${bright}shader toolchain${clear}  targets: %s",
               table.concat(targets, ", "))
        cprint("  slangc     %s  %s", tc.slangc.value,
               tc.slang_pin_matches and "${color.success}(pinned)"
               or ("${color.warning}(pin is %s)"):format(tc.slang_pin))
        if tc.metal then
            if shaders.can_validate_msl(tc) then
                cprint("  metal      %s, -std=%s, macOS >= %s", tc.metal.value,
                       tc.msl_std, tc.macos_min)
            else
                cprint("${color.warning}  metal      UNAVAILABLE${clear}  %s",
                       (tc.metal.reason or tc.metallib.reason))
            end
        end

        -- The self-test proves the classification against real compiler
        -- output. It exists because the rule that separates an unsupported
        -- probe from a broken one reads diagnostic ids, and a Slang bump that
        -- renumbers them would otherwise pass broken probes as unsupported
        -- without anyone noticing.
        if option.get("self-test") then
            local root = path.join("ci", "test", "fixtures", "shaders")
            local outdir = os.tmpfile() .. ".shaders"
            local kernels = shaders.discover({root = root})
            local failures, unchecked, checked = 0, 0, 0

            cprint("${bright}self-test${clear}  %s", root)
            for _, k in ipairs(kernels) do
                local text = io.readfile(path.join(os.projectdir(),
                                                   k.source))
                local expect = {}
                for t, s in (text:match("//@expect([^\n]*)") or "")
                            :gmatch("(%w+)=(%u+)") do
                    expect[t] = s
                end
                for _, t in ipairs(targets) do
                    local r = shaders.compile(k, t, {toolchain = tc,
                                                     outputdir = outdir})
                    local want = expect[t]
                    if not want then
                        failures = failures + 1
                        cprint("${color.error}  FAIL${clear} %-28s %-5s no "
                               .. "//@expect for this target", k.name, t)
                    elseif r.status == "UNVALIDATED" and want ~= "FAILED" then
                        -- Only a macOS host can show what Apple's toolchain
                        -- does with the MSL. Counted apart, never as a pass.
                        unchecked = unchecked + 1
                        cprint("${dim}  --   %-28s %-5s expects %s; not "
                               .. "checkable on this host", k.name, t, want)
                    elseif r.status == want and r.escape
                           and r.status == "COMPILED"
                           and not (r.slang_result and r.escape.reason) then
                        -- An escape that compiled must still carry the
                        -- rejected Slang attempt and its own reason.
                        failures = failures + 1
                        cprint("${color.error}  FAIL${clear} %-28s %-5s "
                               .. "escaped, but the record lost the Slang "
                               .. "attempt or the reason", k.name, t)
                    elseif r.status == want then
                        checked = checked + 1
                        cprint("${color.success}  ok${clear}   "
                               .. "%-28s %-5s %s%s", k.name, t, r.status,
                               r.rejected_by and ("  (" .. r.rejected_by
                                                  .. ")") or "")
                    else
                        failures = failures + 1
                        cprint("${color.error}  FAIL${clear} %-28s %-5s "
                               .. "expected %s, got %s", k.name, t, want,
                               r.status)
                        cprint("${dim}%s", shaders.describe(r))
                    end
                end
            end

            -- The hash assumes a kernel compiles to the same bytes every
            -- time. Check that on a real kernel rather than assume it.
            for _, k in ipairs(kernels) do
                if k.name == "baseline/imports_common" then
                    local t = targets[1]
                    local a = shaders.compile(k, t, {toolchain = tc,
                                                     outputdir = outdir})
                    local b = shaders.compile(k, t, {toolchain = tc,
                                                     outputdir = outdir})
                    local deps = table.concat(a.dependencies or {}, " ")
                    if a.status == "COMPILED" then
                        checked = checked + 1
                        if a.sha256 ~= b.sha256 then
                            failures = failures + 1
                            cprint("${color.error}  FAIL${clear} %s is not "
                                   .. "deterministic: %s then %s", k.name,
                                   a.sha256, b.sha256)
                        end
                        -- An import must be tracked, or editing a module
                        -- would leave stale kernels behind.
                        checked = checked + 1
                        if not deps:find("common/fixture_hash.slang", 1,
                                         true) then
                            failures = failures + 1
                            cprint("${color.error}  FAIL${clear} %s does not "
                                   .. "list its import: %s", k.name, deps)
                        end
                    end
                end
            end
            os.tryrm(outdir)

            cprint("")
            if unchecked > 0 then
                cprint("${color.warning}%d outcome(s) not checkable here"
                       .. "${clear} (Apple's toolchain is macOS-only)",
                       unchecked)
            end
            if failures > 0 then
                raise("shader toolchain self-test: %d of %d check(s) failed",
                      failures, failures + checked)
            end
            cprint("${color.success}shader toolchain self-test: %d check(s) "
                   .. "passed", checked)
            return
        end

        local kernels, misplaced = shaders.discover()
        for _, reason in ipairs(misplaced) do
            cprint("${color.error}misplaced${clear} %s", reason)
        end
        if #misplaced > 0 then
            raise("%d .slang file(s) outside baseline/, probes/ and common/",
                  #misplaced)
        end

        local failed = {}
        for _, k in ipairs(kernels) do
            for _, t in ipairs(targets) do
                local r = shaders.compile(k, t, {toolchain = tc})
                if r.status == "FAILED" then
                    table.insert(failed, r)
                end
            end
        end

        local index = shaders.index(kernels, tc,
                                    {generator = "xmake ci-shaders"})
        shaders.prune(index)
        local indexfile = shaders.write_index(index)
        local output = option.get("output")
        if output then
            os.mkdir(path.directory(output))
            os.cp(indexfile, output)
        end

        cprint("${bright}kernels${clear}  %d under shaders/", #kernels)
        if #kernels == 0 then
            cprint("${color.warning}  none yet.${clear} The toolchain is "
                   .. "ready; the first baseline kernel makes gate B0.1 "
                   .. "checkable (r0-architecture.md section 4).")
        end
        local unvalidated = 0
        for _, entry in ipairs(index.kernels) do
            for _, t in ipairs(targets) do
                local r = entry.results[t]
                local colour = "${color.success}"
                if r.status == "FAILED" or r.status == "NOT_BUILT" then
                    colour = "${color.error}"
                elseif r.status ~= "COMPILED" then
                    colour = "${color.warning}"
                end
                if r.status == "UNVALIDATED" then
                    unvalidated = unvalidated + 1
                end
                local note = ""
                if r.rejected_by then
                    note = "  rejected by " .. r.rejected_by
                elseif r.slang_result then
                    note = "  via MSL escape (Slang: "
                           .. r.slang_result.reason .. ")"
                end
                cprint("  %-8s %-36s %-5s " .. colour .. "%s${clear}%s",
                       entry.class, entry.name, t, r.status, note)
            end
        end

        -- architecture.md section 10: "If more than one kernel needs it,
        -- D3's cost model is wrong." That is about the kernels the engine
        -- needs; a probe's escape measures the hardware apart from Slang.
        for _, class in ipairs({"baseline", "probe"}) do
            local list = index.escapes[class]
            if #list > 0 then
                cprint("${bright}MSL escapes${clear} (%s)  %s", class,
                       table.concat(list, ", "))
            end
        end
        if #index.escapes.baseline > 1 then
            cprint("${color.warning}%d baseline kernels ship hand-written "
                   .. "MSL.${clear} architecture.md section 10: past one, "
                   .. "D3's cost model is wrong and D2's Metal estimate "
                   .. "moves toward 800 hours. Raise it.",
                   #index.escapes.baseline)
        end

        cprint("${bright}shader hash${clear}  (what a run manifest records)")
        for _, t in ipairs(targets) do
            local h = index.shader_hash[t]
            if h.status == "OK" then
                cprint("  %-5s %s", t, h.value)
            else
                cprint("${color.warning}  %-5s UNAVAILABLE${clear}  %s", t,
                       h.reason)
            end
        end
        cprint("${dim}  index: %s", path.relative(indexfile,
                                                  os.projectdir()))

        for _, r in ipairs(failed) do
            cprint("")
            cprint("${color.error}%s", shaders.describe(r))
        end
        if #failed > 0 then
            raise("%d kernel compile(s) failed", #failed)
        end
        -- On macOS the MSL can be proven, so failing to prove it is a broken
        -- runner. Elsewhere it is expected, and said so.
        if unvalidated > 0 then
            if os.host() == "macosx" then
                raise("%d MSL compile(s) could not be validated: %s. B0.1 "
                      .. "cannot be evaluated without Apple's toolchain, and "
                      .. "an absent toolchain is never a pass", unvalidated,
                      tc.metal.reason or tc.metallib.reason)
            end
            cprint("${color.warning}%d MSL compile(s) not validated here; "
                   .. "only a macOS host proves B0.1 for MSL", unvalidated)
        end
    end)
task_end()

-- -------------------------------------------------------- ci-shader-hash ---

task("ci-shader-hash")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-shader-hash [--backend=<name>] "
                      .. "[--stamp=<manifest.json>]",
        description = "Compute the shader hash of the compiled set, and "
                      .. "optionally write it into a run manifest.",
        options = {
            {'b', "backend", "kv", nil, "vulkan or metal: the backend whose "
                                        .. "shader set to hash. Default: "
                                        .. "every target built, or the "
                                        .. "stamped manifest's own backend."},
            {'s', "stamp", "kv", nil, "Write shader_hash into this run "
                                      .. "manifest, then validate it against "
                                      .. "the schema."},
        }
    }
    on_run(function ()
        import("core.base.option")
        import("core.base.json")
        import("core.project.config")
        import("shaders", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("jsonschema",
               {rootdir = path.join(os.projectdir(), "ci", "lua")})

        config.load()
        local index, err = shaders.load_index()
        if not index then
            raise(err)
        end

        -- Recomputed from the artifacts on disk, not copied from the index:
        -- the hash has to describe the bytes a backend would load now, and
        -- an artifact replaced since the index was written would otherwise
        -- go unnoticed.
        local hashes = shaders.rehash(index)

        local stamp = option.get("stamp")
        local manifest
        local backend = option.get("backend")
        if stamp then
            manifest = json.loadfile(stamp)
            if not backend and manifest.backend ~= json.null then
                backend = manifest.backend
            end
            if not backend then
                raise("%s names no backend; pass --backend=vulkan or "
                      .. "--backend=metal", stamp)
            end
        end

        local targets = index.targets
        if backend then
            local t = shaders.target_for_backend(backend)
            if not t then
                raise("backend '%s' loads no compiled shader set", backend)
            end
            targets = {t}
        end

        for _, t in ipairs(targets) do
            local h = hashes[t]
            if not h then
                raise("the %s target was not built; configure it with "
                      .. "'xmake f --shader-targets=%s'", t, t)
            end
            if h.status == "OK" then
                cprint("%-5s %s", t, h.value)
            else
                cprint("${color.warning}%-5s UNAVAILABLE${clear}  %s", t,
                       h.reason)
            end
        end

        if stamp then
            local h = hashes[targets[1]]
            -- A manifest must never carry a hash for a set that was not
            -- loadable. The field stays null and the reason is raised.
            if h.status ~= "OK" then
                raise("not stamping %s: %s", stamp, h.reason)
            end
            manifest.shader_hash = h.value
            io.writefile(stamp, json.encode(manifest))
            local ok, errors = jsonschema.validate_file(stamp,
                path.join(os.projectdir(), "ci", "schema",
                          "run-manifest.schema.json"))
            if not ok then
                for _, e in ipairs(errors) do
                    cprint("${color.error}  %s", e)
                end
                raise("%s no longer validates against the run manifest "
                      .. "schema", stamp)
            end
            cprint("${color.success}stamped %s${clear} (%s)", stamp,
                   targets[1])
        end
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
            -- The toolchain proves itself on its fixtures before it is
            -- trusted with the real kernels.
            {name = "shader toolchain", task = "ci-shaders",
             options = {["self-test"] = true}},
            {name = "shaders", task = "ci-shaders"},
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
        local kernels = os.files(path.join(os.projectdir(), "shaders",
                                           "**.slang"))
        local bundles = os.dirs(path.join(os.projectdir(), "results", "*"))

        local report = pins.verify({allow_unpinned = true})
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
            slang_shaders    = #kernels,
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
                        "bench_test", "bdestyle_test",
                        "shaders_test", "device_probes_test",
                        "ratios_test"}

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
            {nil, "no-probes", "k", nil, "Skip the vg_bench device probes."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("bench", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.json")

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
            device_probes = not option.get("no-probes"),
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
        local probes = json.loadfile(path.join(dir, "device-probes.json"))
        if probes and probes.status == "RAN" then
            for _, p in ipairs(probes.probes) do
                style.say("  %-12s %s", p.status, p.id)
            end
        elseif probes then
            style.say("${warn}  device probes not run:${reset} %s",
                      tostring(probes.reason))
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

-- ------------------------------------------------------------- ci-ratios ---

task("ci-ratios")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-ratios --apple=<bundle> --linux=<bundle>",
        description = "Derive the per-workload Apple-to-Linux ratios of gate "
                      .. "B0 from two bundles.",
        options = {
            {nil, "apple", "kv", nil, "The Apple bundle (Profile A, Metal)."},
            {nil, "linux", "kv", nil, "The Linux bundle (Profile L, Vulkan)."},
            {'o', "out", "kv", nil, "Write ratios.json here instead of into "
                                    .. "both bundles."},
        }
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("core.base.option")
        import("core.base.json")
        import("ratios", {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local apple, linux = option.get("apple"), option.get("linux")
        if not apple or not linux then
            raise("both --apple and --linux are needed: a ratio has two "
                  .. "sides")
        end
        local doc = ratios.document(path.absolute(apple),
                                    path.absolute(linux))

        -- r0-benchmarks.md section 6 puts ratios.json in both gate bundles.
        local targets = option.get("out")
                        and {option.get("out")}
                        or {path.join(apple, "ratios.json"),
                            path.join(linux, "ratios.json")}
        for _, file in ipairs(targets) do
            io.writefile(file, json.encode(doc))
        end

        style.say("${bold}Apple-to-Linux ratios${reset}  %s on %s, %s on %s",
                  tostring(doc.apple.device), tostring(doc.apple.backend),
                  tostring(doc.linux.device), tostring(doc.linux.backend))
        for _, w in ipairs(doc.workloads) do
            if w.ratio ~= json.null then
                style.say("  %-16s %8.3f  ${dim}%s${reset}", w.workload,
                          w.ratio, tostring(w.unit))
            else
                style.say("  %-16s %8s  ${warn}%s${reset}", w.workload, "-",
                          w.reason)
            end
        end
        for _, c in ipairs(doc.caveats) do
            style.say("${warn}  caveat:${reset} %s", c)
        end
        for _, file in ipairs(targets) do
            style.say("${ok}wrote %s", file)
        end
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

        -- clang-tidy 19 or newer. clang 18 declares __cpp_concepts as
        -- 201907, and libstdc++ provides std::expected only from 202002, so
        -- on Linux clang-tidy 18 cannot parse code that uses it, and every
        -- finding is a parse error. Versioned names come first: Ubuntu
        -- installs clang-tidy-19 beside an unversioned clang-tidy 18.
        -- Homebrew keeps clang-tidy out of PATH because it would shadow
        -- Apple's clang; look there, versioned kegs included, before giving
        -- up.
        local minimum = 19
        local candidates = {
            "clang-tidy-20",
            "clang-tidy-19",
            "clang-tidy",
            "/opt/homebrew/opt/llvm/bin/clang-tidy",
            "/usr/local/opt/llvm/bin/clang-tidy",
        }
        -- The kegs are symlinks, which os.files does not descend into.
        for _, keg in ipairs(os.filedirs("/opt/homebrew/opt/llvm@*")) do
            table.insert(candidates, path.join(keg, "bin", "clang-tidy"))
        end
        local tidy
        local too_old = {}
        for _, c in ipairs(candidates) do
            -- 'lines': the version is on the second line of the output.
            local version = probe.match(c, {"--version"}, "version%s+(%d+)",
                                        {lines = true})
            if version.status == "OK" then
                if tonumber(version.value) >= minimum then
                    tidy = c
                    break
                end
                table.insert(too_old, ("%s is %s"):format(c, version.value))
            end
        end
        if not tidy then
            raise("clang-tidy %d or newer not found%s. Tried: %s", minimum,
                  #too_old > 0
                      and (" (" .. table.concat(too_old, ", ") .. ")")
                      or "",
                  table.concat(candidates, ", "))
        end
        style.say("${dim}%s${reset}", tidy)

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

        -- 'xmake test' exists, but its summary is printed by xmake itself in
        -- full-brightness truecolour that no project setting can reach. This
        -- task runs the same binary and reports through style.lua, so unit
        -- test output is readable on the same backgrounds as everything else.
        --
        -- The guard is on tests, not on C++: first-party code without a test
        -- (vg_bench, for one) leaves the test target disabled in xmake.lua,
        -- with nothing to build.
        local tests = {}
        for _, pattern in ipairs({"tests/**.t.cpp", "gamegine/src/**.t.cpp"}) do
            table.join2(tests, os.files(path.join(os.projectdir(), pattern)))
        end
        if #tests == 0 then
            style.say("${warn}no unit tests yet; nothing to test${reset}")
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
                passed = tonumber(n) or passed
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

-- ---------------------------------------------------------- ci-coverage ---

task("ci-coverage")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-coverage",
        description = "Measure line, function and branch coverage of "
                      .. "gamegine/ by the unit tests.",
        options = {}
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("coverage",
               {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local context, reason = coverage.measure()
        if not context then
            -- Coverage that could not be measured is not a number.
            style.say("${warn}coverage UNAVAILABLE${reset}  %s", reason)
            raise("coverage could not be measured")
        end

        local table_text, err = coverage.report(context)
        if not table_text then
            raise("llvm-cov report failed: %s", err or "")
        end
        style.say("${bold}coverage of gamegine/ by the unit tests${reset}")
        print(table_text)
        style.say("${dim}Lines that ran, not results that were checked. "
                  .. "'xmake ci-coverage-html' shows which lines.${reset}")
    end)
task_end()

-- ----------------------------------------------------- ci-coverage-html ---

task("ci-coverage-html")
    set_category("plugin")
    set_menu {
        usage       = "xmake ci-coverage-html",
        description = "Write the llvm-cov HTML report, annotated per line "
                      .. "and per branch.",
        options = {}
    }
    on_run(function ()
        import("style", {rootdir = path.join(os.projectdir(), "ci", "lua")})
        import("coverage",
               {rootdir = path.join(os.projectdir(), "ci", "lua")})

        local context, reason = coverage.measure()
        if not context then
            style.say("${warn}coverage UNAVAILABLE${reset}  %s", reason)
            raise("coverage could not be measured")
        end

        local index, err = coverage.html(context)
        if not index then
            raise("llvm-cov show failed: %s", err or "")
        end
        style.say("${ok}HTML coverage report written${reset}")
        style.say("  %s", path.relative(index, os.projectdir()))
        style.say("${dim}  open it with:  open %s${reset}",
                  path.relative(index, os.projectdir()))
    end)
task_end()
