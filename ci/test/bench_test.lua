-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the harness's two obligations: it emits a bundle that validates
-- against the frozen schema, and it never presents an absent measurement as
-- a measured one. The second is the one worth testing -- a harness that
-- writes 0.0 where it means "did not run" produces bundles that compare
-- cleanly against real ones and are silently wrong.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("bench", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("bundle", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("core.base.json")

function run(t)
    local out = path.join(os.tmpdir(), "vgbench-test-" .. os.time())
    local dir, report, ok, manifest = bench.run({
        out        = out,
        tier       = "T1",
        skip_power = true,
        command    = "xmake bench --tier=T1 (test)",
    })

    testing.check(t, "a T1 run emits a bundle that validates", ok,
                  table.concat((function ()
                      local m = {}
                      for _, f in ipairs(report.findings or {}) do
                          table.insert(m, f.message)
                      end
                      return m
                  end)(), "; "))

    -- Gate B0.4's second half: pinned and *recorded in every manifest*. The
    -- pin file proves they are pinned; this proves they travel with a result.
    for _, field in ipairs({"slang_version", "meshoptimizer_version",
                            "clusterlod_version"}) do
        testing.check(t, ("manifest carries %s"):format(field),
                      manifest[field] ~= nil
                      and manifest[field] ~= json.null
                      and tostring(manifest[field]) ~= "",
                      "B0.4 requires it in EVERY manifest")
    end

    testing.equal(t, "a run with no engine is NOT_RUN", manifest.status,
                  "NOT_RUN")
    testing.check(t, "and says why", manifest.status_reason ~= nil
                  and tostring(manifest.status_reason):trim() ~= "")

    -- Absent is not zero.
    local aggregate = json.loadfile(path.join(dir, "aggregate.json"))
    for _, group in ipairs({"frame_ms", "gpu_ms", "cpu_active_ms"}) do
        testing.check(t, ("%s p95 is null, not 0"):format(group),
                      aggregate[group].p95 == json.null,
                      "an unmeasured statistic must be absent, never zero")
    end

    -- Every fixture is accounted for; none is silently dropped.
    local fixtures = bench.load_fixtures()
    testing.equal(t, "every registered fixture appears in the aggregate",
                  #aggregate.fixtures, #fixtures)
    for _, f in ipairs(aggregate.fixtures) do
        testing.check(t, ("fixture %s carries a reason"):format(f.id),
                      f.reason ~= nil and tostring(f.reason):trim() ~= "")
    end

    -- A bundle produced on a development machine must never claim to be
    -- evidence, whatever else it contains.
    local machine = json.loadfile(path.join(dir, "machine.json"))
    testing.equal(t, "evidence_eligible matches the machine registry",
                  manifest.evidence_eligible,
                  machine.evidence_eligible == true)
    if manifest.machine_role == "development" then
        testing.equal(t, "a development machine yields no evidence",
                      manifest.evidence_eligible, false)
    end

    -- The validator must actually reject a damaged bundle, or the check
    -- above proves nothing.
    os.rm(path.join(dir, "aggregate.json"))
    local _, still_ok = bundle.validate(dir)
    testing.check(t, "the validator rejects a bundle missing aggregate.json",
                  not still_ok)

    os.tryrm(dir)

    local failed = false
    try
    {
        function () bench.run({out = os.tmpdir(), tier = "T9"}) end,
        catch { function () failed = true end }
    }
    testing.check(t, "an unknown tier is refused", failed)
end
