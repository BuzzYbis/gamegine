-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Runs BOTH platforms' probe sets, on whichever platform this is.
--
-- Neither developer can run the other's machine, so without this the Linux
-- probes would first execute on ci-linux overnight, and the macOS probes
-- first on ci-mac. Every probe fails on the wrong platform, which is the
-- point: it exercises the parsing and the nil handling. This test is what
-- caught 'glxinfo -B' hanging the whole capture.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("machine", {rootdir = path.join(os.projectdir(), "ci", "lua")})

-- 'next' is not available in the xmake sandbox, so emptiness is checked by
-- iterating.
function _count(tbl)
    local n = 0
    for _ in pairs(tbl or {}) do
        n = n + 1
    end
    return n
end

function _well_formed(t, m, label)
    for group, tbl in pairs({fields = m.fields or {},
                             toolchain = m.toolchain or {}}) do
        for name, record in pairs(tbl) do
            local where = ("%s: %s.%s"):format(label, group, name)
            testing.check(t, where .. " is a record",
                          type(record) == "table" and record.status ~= nil,
                          "every field must be a probe record")
            if type(record) == "table" then
                testing.check(t, where .. " has a status we understand",
                              record.status == "OK"
                              or record.status == "UNAVAILABLE",
                              tostring(record.status))
                -- An UNAVAILABLE field must say why. A gap with no reason is
                -- indistinguishable from a bug.
                if record.status == "UNAVAILABLE" then
                    testing.check(t, where .. " explains itself",
                                  record.reason ~= nil
                                  and tostring(record.reason):trim() ~= "",
                                  "UNAVAILABLE without a reason")
                end
            end
        end
    end
end

function run(t)
    for _, host in ipairs({"linux", "macosx"}) do
        local t0 = os.mclock()
        local m = machine.capture({force_host = host, skip_power = true})
        local elapsed = os.mclock() - t0

        -- The bound that matters. Before probe timeouts existed this ran
        -- forever on the Linux path.
        testing.check(t, ("%s capture terminates promptly"):format(host),
                      elapsed < 60000, ("took %dms"):format(elapsed))

        testing.check(t, ("%s capture returns fields"):format(host),
                      _count(m.fields) > 0)
        _well_formed(t, m, host)

        if host ~= os.host() then
            testing.check(t, ("%s run is marked as a smoke test"):format(host),
                          m.smoke_test ~= nil)
            testing.equal(t, ("%s run is not evidence"):format(host),
                          m.evidence_eligible, false)
        end
    end
end
