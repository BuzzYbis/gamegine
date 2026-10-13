-- SPDX-License-Identifier: Apache-2.0
-- Copyright (c) 2026 BuzzY_ & Rether
--
-- Guards the parts of the shader toolchain that need no compiler: the
-- capability-declaration check, the reading of diagnostics and dependency
-- files, and the shader hash. They decide whether a kernel may pass, whether
-- a probe is unsupported or broken, and what a run manifest records, so a
-- regression in any of them grants a pass nobody earned.
--
-- The same classification is exercised against real compiler output by
-- 'xmake ci-shaders --self-test', which needs slangc.

import("testing", {rootdir = path.join(os.projectdir(), "ci", "lua")})
import("shaders", {rootdir = path.join(os.projectdir(), "ci", "lua")})

function _declarations(t)
    local declared = [[
[require(spirv)] [require(metal)]
[shader("compute")]
[numthreads(64, 1, 1)]
void a(uint3 id : SV_DispatchThreadID) {}
]]
    testing.equal(t, "a declared entry point passes",
                  #shaders.undeclared_entry_points(declared), 0)

    -- The declaration may sit after [shader], on the same line or not.
    local after = [[
[shader("compute")] [numthreads(64, 1, 1)] [require(spirv)]
void b() {}
]]
    testing.equal(t, "a declaration after [shader] counts",
                  #shaders.undeclared_entry_points(after), 0)

    local missing = shaders.undeclared_entry_points([[
RWStructuredBuffer<uint> o;

[shader("compute")]
[numthreads(64, 1, 1)]
void fill(uint3 id : SV_DispatchThreadID) {}
]])
    testing.equal(t, "an undeclared entry point is found", #missing, 1)
    testing.equal(t, "it is named", missing[1] and missing[1].name, "fill")
    testing.equal(t, "its line is the [shader] line",
                  missing[1] and missing[1].line, 3)

    -- A commented-out declaration is not a declaration.
    local commented = shaders.undeclared_entry_points([[
// [require(spirv)]
/* [require(metal)] */
[shader("compute")] [numthreads(1, 1, 1)]
void c() {}
]])
    testing.equal(t, "a commented-out [require] does not count",
                  #commented, 1)

    -- A declaration belongs to its own entry point only. The second entry
    -- point below must not borrow the first one's.
    local two = shaders.undeclared_entry_points([[
[require(spirv)]
[shader("compute")] [numthreads(1, 1, 1)]
void first() {}

[shader("compute")] [numthreads(1, 1, 1)]
void second() {}
]])
    testing.equal(t, "a declaration is per entry point", #two, 1)
    testing.equal(t, "the undeclared one is reported",
                  two[1] and two[1].name, "second")

    -- A "//" inside an attribute's string argument must not be read as a
    -- comment that swallows the rest of the line.
    local strings = shaders.undeclared_entry_points([[
[require(spirv)] [shader("compute")] [numthreads(8, 8, 1)]
[outputtopology("a//b")]
void d() {}
]])
    testing.equal(t, "a '//' inside a string is not a comment", #strings, 0)

    local generic = shaders.undeclared_entry_points([[
[shader("fragment")]
vector<float, 4> shade(float4 p : SV_Position) : SV_Target { return p; }
]])
    testing.equal(t, "a templated return type still yields the name",
                  generic[1] and generic[1].name, "shade")

    testing.equal(t, "a helper without [shader] needs no declaration",
                  #shaders.undeclared_entry_points("uint f(uint x) "
                                                   .. "{ return x; }"), 0)
end

function _diagnostics(t)
    -- Real slangc lines, with the message text shortened.
    local declared = [[
shaders/probes/x.slang(14): error 36107: entrypoint 'fill' uses features...
]]
    local ids = shaders.diagnostic_ids(declared)
    testing.equal(t, "a diagnostic id is read", ids[1], 36107)
    testing.check(t, "36107 alone means declared unsupported",
                  shaders.declares_unsupported(ids))

    -- An undeclared capability alongside it is a defect in the kernel. It
    -- must not ride along with the 36107 and pass as unsupported.
    local mixed = shaders.diagnostic_ids(declared .. [[
shaders/probes/x.slang(14): error 36104: 'fill' uses undeclared capability...
]])
    testing.equal(t, "every error id is read", #mixed, 2)
    testing.check(t, "36107 with 36104 is a failure, not unsupported",
                  not shaders.declares_unsupported(mixed))

    testing.check(t, "a declared target a dependency cannot meet fails",
                  not shaders.declares_unsupported({36108}))
    testing.check(t, "a syntax error fails",
                  not shaders.declares_unsupported({30015}))
    -- A failure with no diagnostic has no reason attached.
    testing.check(t, "no diagnostic at all is not unsupported",
                  not shaders.declares_unsupported({}))
    testing.equal(t, "warnings are not errors",
                  #shaders.diagnostic_ids("x(1): warning 41012: profile"), 0)
end

function _depfiles(t)
    local deps = shaders.parse_depfile(
        "out.spv: /a/k.slang /a/common/m.slang\n")
    testing.equal(t, "a depfile lists every dependency", #deps, 2)
    testing.equal(t, "the import is read", deps[2], "/a/common/m.slang")

    local continued = shaders.parse_depfile(
        "out.spv: /a/k.slang \\\n  /a/with\\ space.slang\n")
    testing.equal(t, "line continuations are joined", #continued, 2)
    testing.equal(t, "escaped spaces stay in the path", continued[2],
                  "/a/with space.slang")
    testing.equal(t, "a missing depfile yields nothing",
                  #shaders.parse_depfile(nil), 0)
end

function _targets(t)
    local both = shaders.parse_targets("msl, spirv")
    testing.equal(t, "targets are put in canonical order", both[1], "spirv")
    testing.equal(t, "both are kept", #both, 2)
    testing.equal(t, "duplicates collapse", #shaders.parse_targets("msl,msl"),
                  1)
    testing.equal(t, "an empty value is the host default",
                  #shaders.parse_targets(""), 1)
    local ok = try { function ()
        shaders.parse_targets("hlsl")
        return true
    end }
    testing.check(t, "an unknown target is refused", not ok)

    testing.equal(t, "vulkan loads spirv",
                  shaders.target_for_backend("vulkan"), "spirv")
    testing.equal(t, "metal loads msl", shaders.target_for_backend("metal"),
                  "msl")
    testing.absent(t, "the CPU reference loads no shaders",
                   shaders.target_for_backend("cpu-reference"))
end

function _kernels(t)
    local k = shaders.kernel("shaders/probes/atomic_max_u64.slang")
    testing.equal(t, "a probe is classified by its directory",
                  k and k.class, "probe")
    testing.equal(t, "the name keeps the directory",
                  k and k.name, "probes/atomic_max_u64")
    testing.equal(t, "the source is project-relative, for #line",
                  k and k.source, "shaders/probes/atomic_max_u64.slang")
    testing.equal(t, "baseline/ is required",
                  shaders.kernel("shaders/baseline/x.slang").class,
                  "baseline")
    testing.equal(t, "common/ holds modules",
                  shaders.kernel("shaders/common/x.slang").class, "module")
    local stray, reason = shaders.kernel("shaders/x.slang")
    testing.absent(t, "a kernel outside the three directories is refused",
                   stray)
    testing.check(t, "and the refusal says why", reason ~= nil)
end

function _hashing(t)
    local a = {kernel = "baseline/a", status = "COMPILED", sha256 = "aa"}
    local b = {kernel = "probes/b", status = "UNSUPPORTED"}

    local h1 = shaders.hash_records("spirv", {a, b})
    local h2 = shaders.hash_records("spirv", {b, a})
    testing.equal(t, "a hash is produced", h1.status, "OK")
    testing.equal(t, "the hash ignores record order", h1.value, h2.value)

    local other = shaders.hash_records("msl", {a, b})
    testing.check(t, "the target is part of the hash",
                  other.value ~= h1.value)

    local flipped = shaders.hash_records("spirv", {a, {kernel = "probes/b",
        status = "COMPILED", sha256 = "bb"}})
    testing.check(t, "a probe becoming supported changes the hash",
                  flipped.value ~= h1.value)

    local renamed = shaders.hash_records("spirv", {{kernel = "baseline/c",
        status = "COMPILED", sha256 = "aa"}, b})
    testing.check(t, "a renamed kernel changes the hash",
                  renamed.value ~= h1.value)

    -- A set with a failed or unvalidated kernel was not loadable, so it has
    -- no hash; claiming one would be a pass nobody earned.
    for _, status in ipairs({"FAILED", "UNVALIDATED", "NOT_BUILT"}) do
        local h = shaders.hash_records("spirv", {a, {kernel = "probes/b",
                                                     status = status}})
        testing.equal(t, ("a %s kernel leaves no hash"):format(status),
                      h.status, "UNAVAILABLE")
        testing.check(t, ("and says why (%s)"):format(status),
                      h.reason ~= nil)
    end
    testing.equal(t, "an empty set has no hash",
                  shaders.hash_records("spirv", {}).status, "UNAVAILABLE")
end

function _escapes(t)
    testing.equal(t, "an escape's reason is read",
                  shaders.escape_reason("// x\n//@escape  Slang emits "
                                        .. "atomic_fetch_max on ulong \n"),
                  "Slang emits atomic_fetch_max on ulong")
    testing.absent(t, "an escape without a reason has none",
                   shaders.escape_reason("// just MSL\n"))
    testing.absent(t, "an empty '//@escape' is not a reason",
                   shaders.escape_reason("//@escape\n"))

    -- An escape binds by the same indices Slang reflected, or a loader
    -- binding by name would hand it the wrong buffers.
    local bindings = {{name = "params", index = 0}, {name = "keys", index = 1}}
    local check = shaders.escape_binding_mismatches
    testing.equal(t, "an escape binding like Slang passes",
                  #check("device P* params [[buffer(0)]],\n"
                         .. "device atomic_ulong* keys [[ buffer( 1 ) ]]",
                         bindings), 0)
    testing.equal(t, "swapped indices are both reported",
                  #check("params [[buffer(1)]], keys [[buffer(0)]]",
                         bindings), 2)
    testing.equal(t, "a buffer the escape never binds is reported",
                  #check("params [[buffer(0)]]", bindings), 1)
    testing.equal(t, "a longer name ending in the binding's does not count",
                  #check("myparams [[buffer(0)]] keys [[buffer(1)]]",
                         bindings), 1)
    testing.equal(t, "a commented-out parameter does not count",
                  #check("// params [[buffer(0)]]\nkeys [[buffer(1)]]",
                         bindings), 1)
end

function run(t)
    _escapes(t)
    _declarations(t)
    _diagnostics(t)
    _depfiles(t)
    _targets(t)
    _kernels(t)
    _hashing(t)
end
