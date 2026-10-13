# Shaders

Every kernel is written once in [Slang](https://github.com/shader-slang/slang)
(D3) and compiled ahead of time. SPIR-V goes to Vulkan. MSL goes through
Apple's `metal` and `metallib` and ships as a metallib. **Nothing is compiled
at run time.**

```bash
xmake build shaders                  # this host's target: spirv on Linux, msl on macOS
xmake f --shader-targets=spirv,msl   # build both
xmake ci-shaders                     # what CI runs: full compile and report
xmake ci-shaders --self-test         # prove the toolchain on its fixtures
xmake ci-shader-hash                 # the shader hash of what was built
```

Anything that loads kernels declares `add_deps("shaders")` and reads
`build/shaders/index.json` to find them.

## The kernels

| Kernel | Class | Measures |
| --- | --- | --- |
| `baseline/prefix_scan` | baseline | The R0 toolchain kernel: a three-pass prefix sum with thread-group memory and barriers, one source on SPIR-V and MSL (B0.1) |
| `probes/atomic_max_u64_buffer` | probe | 64-bit buffer atomic max under contention. **MSL escape** |
| `probes/atomic_max_u64_fragment` | probe | 64-bit atomic max from the fragment stage. **MSL escape** |
| `probes/mesh_cluster_dispatch` | probe | Mesh-shader cluster dispatch throughput |
| `probes/mesh_double_sided` | probe | The cost of `doubleSided` on the mesh path |
| `probes/page_decode` | probe | GPU attribute decode of 64 / 128 / 256 KiB geometry pages |
| `common/bench` | module | The hash every probe derives its data from, and the procedural clusters |

---

## Where a kernel lives decides what a failure means

| Directory | Class | A target that rejects it is… |
| --- | --- | --- |
| `baseline/` | Required baseline | **a build error.** Must compile for every target (gate B0.1) |
| `probes/` | Conditional capability probe | **recorded `UNSUPPORTED`**, with the diagnostics. Unsupported is a valid result, not a failure (gate B0.2) |
| `common/` | Module | never compiled alone; `import common.<name>;` |

A `.slang` file anywhere else is refused, because it would have no defined
meaning.

## Every entry point declares its capability profile

Each function marked `[shader("<stage>")]` carries one `[require(...)]` per
target it supports:

```slang
[require(spirv_1_5, subgroup_arithmetic)]
[require(metal, subgroup_arithmetic)]
[shader("compute")]
[numthreads(64, 1, 1)]
void prefixSum(uint3 id : SV_DispatchThreadID) { ... }
```

Declare the SPIR-V side as `spirv_1_5`, the version Slang emits, rather than
the bare `spirv`. Almost every real kernel calls something that needs a SPIR-V
version, such as a barrier, `normalize` or a 64-bit type, and the bare atom
covers none of it. Add what the kernel uses beyond that on the same line:
`spvInt64Atomics`, `spvMeshShadingEXT`, `subgroup_arithmetic`, and so on.

The build compiles with `-restrictive-capability-check`, so Slang holds the
body to the declaration. **The check runs whatever target is being built.** A
kernel that declares `metal` but calls something Metal lacks fails the SPIR-V
build on Linux too, instead of failing on the Mac weeks later. An entry point
with no declaration fails on every target, because without one nothing is
checked.

A probe that cannot run on a target says so by leaving that target out, as
in `[require(spirv, ...)]` alone. That target then reports `UNSUPPORTED`. The
same declaration on a baseline kernel is a build error.

The check has a known gap. Slang lowers `f16tof32` to SPIR-V that declares
the `Int16` and `Float16` capabilities. On Vulkan those need `shaderInt16` and
`shaderFloat16`, and no `[require]` has to say so. The Vulkan validation layer
catches it; the capability check does not.
`page_decode` decodes its halves with integer operations for that reason.

## Bindings

A kernel's harness-facing resources are buffers only: `StructuredBuffer`,
`RWStructuredBuffer` and `ByteAddressBuffer`. Parameters are a 16-byte struct
read as `params[0]`. The toolchain records each buffer's binding by name from
Slang's reflection: Vulkan binding in set 0, or Metal buffer index. A
harness that binds by those names cannot have two buffers silently swapped
by a reordered declaration.

Slang reports no stage and no thread-group size for mesh entry points; a
loader must know those itself.

## The MSL escape hatch

When Apple rejects the MSL Slang emits for a kernel, a hand-written
`<kernel>.metal` beside `<kernel>.slang` replaces it, for the MSL target only
(architecture.md, D3):

- **It must say why**, on a `//@escape <reason>` line. Without one the build
  fails.
- **It must bind like the Slang kernel.** Every reflected buffer must appear
  as `<name> [[buffer(<n>)]]` at Slang's index, or the build fails.
- **The Slang attempt stays in the record** as `slang_result`. That Slang could
  not do it is itself a result.
- **An escape Slang no longer needs fails the build.** The failure mode to
  avoid is a slow drift back into two hand-written shader sets.
- **More than one *baseline* escape** means D3's cost model is wrong
  (architecture.md §10). `ci-shaders` says so. A probe's escape measures the
  hardware apart from Slang, and does not count.

The two 64-bit atomic probes carry escapes today. Slang emits
`atomic_fetch_max_explicit` on `atomic_ulong`, which MSL does not have: its
64-bit max is `atomic_max_explicit` and returns nothing. This holds at the
pinned Slang (`vulkan-sdk-1.4.341.0`) and at Slang master in September 2026.
It is the R0 risk *"Slang's MSL output is unusable for a kernel we need"*,
found at R0 as intended. The hardware does the operation: on the M3 Pro both
escapes run and their results check.

## Outcomes

Recorded per kernel and per target in `build/shaders/index.json`, with the
exact commands and the compiler output:

| Status | Meaning |
| --- | --- |
| `COMPILED` | Every stage accepted it and the artifact exists |
| `UNSUPPORTED` | Probes only. Either the declaration excludes the target, or Apple's toolchain rejected the MSL (Slang's, or an escape's) |
| `UNVALIDATED` | MSL was emitted but Apple's toolchain is not available here. **Not a pass** |
| `FAILED` | Anything else, including a probe whose declaration is wrong. A build error |

Only one diagnostic makes a probe `UNSUPPORTED` rather than `FAILED`: Slang's
36107 ("not available … for target"), raised when the declaration excludes
the target. An undeclared capability (36104) or a declared target that a
dependency cannot meet (36108) is a defect in the kernel. This is kept narrow
on purpose: a broken probe must not pass as an unsupported one.

## The shader hash

`shader_hash` in a run manifest is sha256 over every kernel's loaded
artifact (`.spv`, or `.metallib`), or the word `UNSUPPORTED`, for the one
target the run's backend loads. It changes when any loaded byte changes. It
also changes when a kernel is added, removed or renamed, and when a probe
flips between compiled and unsupported. There is no hash while any kernel is
`FAILED` or `UNVALIDATED`, because that set could not have been loaded.

```bash
xmake ci-shader-hash --stamp=results/B0-L/manifest.json   # uses the manifest's backend
```

The build is deterministic, and the hash depends on that:

- `slangc` is run from the project root on project-relative paths. Slang
  writes the path it was given into the MSL's `#line` directives, so an
  absolute path would make the hash depend on where the checkout lives.
- The Apple build pins `-std=metal3.1` and `-mmacosx-version-min=14.0`.
  Without the second, the metallib depends on which macOS ran the build.

## Toolchain

| Setting | Value | Why |
| --- | --- | --- |
| Slang | pinned in `ci/pins.json` | Ships with Vulkan SDK 1.4.341.0. A bump is a named new baseline |
| `-matrix-layout-column-major` | always | math.typ §1: column-major storage, column vectors |
| `-warnings-as-errors all` | always | The shader side of the `-Werror` policy |
| `-fvk-use-entrypoint-name` | SPIR-V | Entry points keep their names, so the harness finds them by name |
| MSL | 3.1, macOS ≥ 14 | The atomic results B0 records must name the MSL version |

The flags live in `ci/lua/shaders.lua`, the one implementation behind both
`xmake build shaders` and `xmake ci-shaders`.
