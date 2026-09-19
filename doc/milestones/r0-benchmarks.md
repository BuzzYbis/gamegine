# B0 — R0 acceptance

**Gate for R0 · both profiles, except where noted · all results NOT RUN**

Parent documents: [Gates](../gates.md) · [Benchmarks](../benchmarks.md) · [R0 architecture](r0-architecture.md) · [CI](../ci.md)

---

## 1. What this gate blocks

R1 may not start until B0 is green. The reason is not ceremony: R1 writes shaders, and every kernel written before the Slang toolchain exists is a kernel written twice.

B0 is unusual among the gates in that most of it is **measurement rather than pass or fail**. Four conditions block; the rest produce numbers that later gates are defined against.

## 2. Fixtures

| ID | Definition | Notes |
| --- | --- | --- |
| S0 | 1 M triangles across plane, sphere, cubes, sloped surfaces, with fixed overlapping and near-plane cases | Created here. Serves every later release |
| S1 | 10 M unique triangles, ten dense rigid meshes, hard edges, UV seams, multiple material sections | Pulled forward from R1. At R0 a single 10 M source mesh with no engine format is enough — the builder run needs geometry, not a cooked package |

## 3. Procedure

1. **Reference implementation.** Build and run `vk_lod_clusters` on Profile L with the threedscans scenes, and with Zorah if bandwidth and disk allow. Record frame times, geometry pool occupancy, streaming behaviour and triangle counts. Freeze and record its revision, supersampling factor, raster mode, shading configuration and streaming state.
2. **Builder.** Run `clusterlod.h` over S1. Record DAG build time, node counts, reduction per level, and peak RSS against thread count.
3. **Device microbenchmarks, both machines.** 64-bit buffer atomic max under contention; 64-bit fragment-stage atomics; the exact `ulong` atomic max in a compiled MSL kernel; mesh-shader cluster dispatch throughput; `doubleSided` cost on the mesh path; page decode throughput at 64 / 128 / 256 KiB.
4. **Toolchain.** One non-trivial kernel compiled and running on both SPIR-V and MSL from a single Slang source, with capability profiles declared and shader hashes in the manifest.
5. **Harness.** A T1 run on both machines emitting a schema-valid result bundle.

## 4. Blocking conditions

Baseline kernels and capability probes are different things, and only the first can block.

| Kind | Meaning |
| --- | --- |
| **Required baseline** | Kernels the engine will certainly need. Must compile for both targets |
| **Conditional capability probe** | Kernels testing an optional feature — 64-bit atomics, specific mesh-shader paths. **Unsupported is a valid result**, not a failure |

| # | Condition | Profile | Status |
| --- | --- | --- | --- |
| B0.1 | Slang builds every **required baseline** kernel for both targets, and `metal`/`metallib` accept the MSL on macOS | Both | required |
| B0.2 | Every **conditional capability probe** compiles or reports unsupported, with full device and toolchain provenance | Both | diagnostic |
| B0.3 | CI green on both machines with one trivial golden-image fixture, including cross-backend comparison | Both | required |
| B0.4 | Slang, meshoptimizer and `clusterlod.h` versions pinned and recorded in every manifest | — | required |
| B0.5 | A reversed-Z cleared frame presents | Both | required |
| B0.6 | Timestamps agree with an **independent supported timing method** on matched measurement scopes, with the uncertainty recorded | Both | required |
| B0.7 | A tiny overlap and near-plane probe exercises winding, clipping and depth order | Both | required |

**On B0.6.** There is no universal known-duration workload: fixed shader work takes different times across clocks and devices. Compare the same measurement scope through two supported mechanisms — timestamp queries against command-buffer duration on Vulkan, counter sampling against `GPUStartTime`/`GPUEndTime` on Metal — agree to within 5%, and record the uncertainty rather than implying an absolute reference.

**On B0.7.** A cleared frame does not exercise reversed-Z geometry. The winding, clipping and depth conventions frozen at R0 need at least one overlapping near-plane triangle to be tested at all.

Statuses are authoritative in the [gate registry](../gates.md).

Nothing else in B0 blocks. A microbenchmark that reports an unwelcome number has succeeded.

## 5. Measurements recorded

These have no thresholds. Their job is to be the baseline later gates are defined against.

| Measurement | Unit | Later use |
| --- | --- | --- |
| `vk_lod_clusters` frame time on Profile L | ms, p50/p95 | Sanity floor for B4's 14 ms GPU budget |
| Its geometry pool occupancy and streaming rate | MiB, MiB/s | B3's throttle figures |
| `clusterlod.h` build time per million triangles | s/Mtri | D4 cook budgets, and the 20 / 90 minute corpus plans |
| Its peak RSS against thread count | GB | The ≤ 8 thread cap that keeps the corpus cook inside 32 GB |
| Its node count and per-level reduction | count, ratio | The `N/(1−ρ)` arithmetic in D7 ([math §13](../math.typ)) |
| 64-bit buffer atomic max throughput under contention | ops/ms | D6b, if D6 ever reopens |
| 64-bit fragment-stage atomic availability and cost | bool, ms | D6b's default of a shared target |
| `ulong` atomic max in compiled MSL | works / does not, with family, SDK, MSL version, address space, stage | Whether a Metal compute rasterizer is possible at all |
| Mesh-shader cluster dispatch throughput | clusters/ms | D3b's premise |
| `doubleSided` cost on the mesh path | ratio against single-sided | R4's foliage budget |
| Page decode throughput at 64 / 128 / 256 KiB, against a **named provisional codec and representative payloads**, with **disk read, CPU decompression, upload and GPU attribute decode reported separately** | MiB/s and latency, per stage | **The page size frozen at R1.** It informs the envelope; it cannot measure a decoder that does not exist yet |
| Per-workload Apple-to-Linux ratios: bandwidth, raster, decode, CPU preparation | ratio each | Replaces the scalar 2× in every A-profile target |

**The ratios are per workload, not one scalar.** Bandwidth, raster, decode and CPU preparation need not scale alike between a 4060 Laptop and an M3 Pro, and a single number hides exactly the disagreement that matters.

## 6. Evidence bundle

`results/B0-L/` and `results/B0-A/`, each containing the standard bundle plus:

- `reference-run.json` — `vk_lod_clusters` revision, settings and measured figures
- `builder-run.json` — `clusterlod.h` timings, node counts, reduction curve, RSS against threads
- `device-probes.json` — every microbenchmark with full device and toolchain provenance
- `ratios.json` — the derived per-workload Apple-to-Linux ratios, with the workload definitions
- `decisions.md` — D0–D5, D7, D8 restated with their answers, plus anything R0 learned that qualifies them

## 7. Failure handling

| Failure | Response |
| --- | --- |
| A **required baseline** kernel compiles for SPIR-V but not MSL | Blocking. Fix the kernel or record the documented per-kernel MSL escape. Do not proceed with a Vulkan-only baseline |
| A **conditional capability probe** reports unsupported | Not a failure. That is the probe working. Record it with full provenance |
| The `ulong` atomic max is unavailable on Metal | Not blocking at R0. Record it, and note that D6b's fallback becomes mandatory if D6 ever reopens |
| `vk_lod_clusters` will not build or run | Not blocking. Record it as unavailable, and mark every target that depended on it as resting on published figures alone |
| Cook throughput is far from published figures | Not blocking. Re-derive the D4 budgets from the measured figure and update the architecture. A budget that does not match the machine cannot catch a regression |
| Page decode shows no clear winner across 64 / 128 / 256 KiB | Take 128 KiB, the documented default, and record that the sweep was inconclusive rather than that 128 KiB won |
