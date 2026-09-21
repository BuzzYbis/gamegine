-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the probe layer's two promises: it never raises, and it never
-- blocks. The second one is not theoretical -- 'glxinfo -B' blocks forever
-- when no display server answers, which is the normal state of a headless
-- runner, and it hung this machine for two minutes before the timeout
-- existed. A nightly job that hangs is worse than one that reports a gap.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("probe", {rootdir = path.join(os.projectdir(), "ci", "lua")})

function run(t)
    local missing = probe.run("gamegine-no-such-tool-exists", {})
    testing.equal(t, "missing program is UNAVAILABLE", missing.status,
                  "UNAVAILABLE")
    testing.absent(t, "missing program has no value", missing.value)
    testing.check(t, "missing program records the command",
                  missing.source == "gamegine-no-such-tool-exists")

    -- The promise that matters: a command that would block is killed and
    -- reported, rather than stopping the run.
    local t0 = os.mclock()
    local slow = probe.run("sleep", {"30"}, {timeout = 500})
    local elapsed = os.mclock() - t0
    testing.equal(t, "a blocking command is UNAVAILABLE", slow.status,
                  "UNAVAILABLE")
    testing.check(t, "it is killed near the timeout, not left running",
                  elapsed < 5000, ("took %dms"):format(elapsed))
    testing.check(t, "the reason says it timed out",
                  tostring(slow.reason):find("timed out", 1, true) ~= nil,
                  tostring(slow.reason))

    -- A tool that reports on stderr is still read correctly; several version
    -- probes, slangc among them, do exactly this.
    local err = probe.run("sh", {"-c", "echo to-stderr 1>&2"},
                          {stream = "stderr"})
    testing.equal(t, "stderr is captured when asked for", err.value,
                  "to-stderr")

    local nonzero = probe.run("sh", {"-c", "exit 3"})
    testing.equal(t, "a non-zero exit is UNAVAILABLE", nonzero.status,
                  "UNAVAILABLE")
    local accepted = probe.run("sh", {"-c", "echo v1.2; exit 3"},
                               {accept = {0, 3}})
    testing.equal(t, "an accepted non-zero exit succeeds", accepted.value,
                  "v1.2")

    -- A pattern that does not match must fail the probe, never return a
    -- wrong value: a tool whose output format changed is a failed probe.
    local nomatch = probe.match("sh", {"-c", "echo hello"}, "version%s+(%d+)")
    testing.equal(t, "a non-matching pattern is UNAVAILABLE", nomatch.status,
                  "UNAVAILABLE")
    local matched = probe.match("sh", {"-c", "echo version 42"},
                                "version%s+(%d+)")
    testing.equal(t, "a matching pattern returns the capture", matched.value,
                  "42")

    testing.check(t, "available() is false for UNAVAILABLE",
                  not probe.available(missing))
    testing.check(t, "available() is true for OK", probe.available(matched))
end
