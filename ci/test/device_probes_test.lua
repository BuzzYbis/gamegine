-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the junction between vg_bench and the bundle: the CSV vg_bench
-- writes is read back exactly -- quoted fields included -- and becomes the
-- device-probes document B0's evidence names. A parser that split a quoted
-- comma would shift every later column, and the numbers would still look
-- like numbers.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("bench", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("core.base.json")

function run(t)
    -- Quoting, as vg_bench's csv_field writes it.
    local rows = bench.parse_csv(
        'a,b,c\n'
        .. 'plain,"with, comma","with ""quotes"""\n'
        .. '"line\nbreak",,last\n')
    testing.equal(t, "two data rows", #rows, 2)
    testing.equal(t, "a plain field", rows[1].a, "plain")
    testing.equal(t, "a quoted comma stays in its field", rows[1].b,
                  "with, comma")
    testing.equal(t, "a doubled quote is one quote", rows[1].c,
                  'with "quotes"')
    testing.equal(t, "a quoted line break stays in its field", rows[2].a,
                  "line\nbreak")
    testing.equal(t, "an empty field is empty, not missing", rows[2].b, "")
    testing.equal(t, "the field after it is still in place", rows[2].c,
                  "last")

    -- A directory as vg_bench leaves it.
    local dir = path.join(os.tmpdir(), "device-probes-test-" .. os.time())
    os.mkdir(dir)
    io.writefile(path.join(dir, "run.csv"),
        "key,value\n"
        .. "generator,vg_bench\nbackend,metal\nshader_target,msl\nruns,5\n"
        .. "shader_hash,abc123\napi,metal\ndevice_name,Apple M3 Pro\n"
        .. "gpu_family,apple9\ntoolchain.msl_std,metal3.1\n"
        .. "toolchain.slang_pin_matches,no\n")
    io.writefile(path.join(dir, "probes.csv"),
        "probe,kernel,status,unsupported_by,reason,summary,notes\n"
        .. 'probe.a,probes/a,PASS,,,"1.0 ms, 2 ops/ms","stage: compute"\n'
        .. "probe.b,probes/b,UNSUPPORTED,device,no mesh shaders,,\n")
    io.writefile(path.join(dir, "measurements.csv"),
        "probe,variant,metric,sample,value,unit\n"
        .. "probe.a,n=4,gpu_ns,0,1000,ns\n"
        .. "probe.a,n=4,gpu_ns,1,1100,ns\n"
        .. "probe.a,n=4,ops_per_ms,,2.5,ops/ms\n")

    local doc = bench.device_probes(dir)
    os.tryrm(dir)

    testing.equal(t, "the document says the probes ran", doc.status, "RAN")
    testing.equal(t, "it carries the backend", doc.backend, "metal")
    testing.equal(t, "and the shader hash", doc.shader_hash, "abc123")
    testing.equal(t, "runs is a number", doc.runs, 5)
    testing.equal(t, "the device gets the device's keys",
                  doc.device.gpu_family, "apple9")
    testing.absent(t, "and not the run's own", doc.device.shader_hash)
    testing.equal(t, "toolchain keys lose their prefix",
                  doc.toolchain.msl_std, "metal3.1")
    testing.equal(t, "every probe is there", #doc.probes, 2)

    local a, b = doc.probes[1], doc.probes[2]
    testing.equal(t, "a summary with a comma survives", a.summary,
                  "1.0 ms, 2 ops/ms")
    testing.equal(t, "only derived values are carried, not raw samples",
                  #a.measurements, 1)
    testing.equal(t, "a derived value is a number", a.measurements[1].value,
                  2.5)
    testing.equal(t, "an UNSUPPORTED probe keeps its reason", b.reason,
                  "no mesh shaders")
    testing.check(t, "and an empty field becomes null, not \"\"",
                  b.summary == json.null)
end
