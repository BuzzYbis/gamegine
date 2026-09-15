# Gate registry

**The authoritative list.** Every acceptance condition in every benchmark document appears here with one status. Where a Markdown table and this file disagree, this file wins, and the table is wrong and gets fixed.

Companion files: [Architecture](architecture.md) · [Benchmarks](benchmarks.md) · [Roadmap](roadmap.md) · [CI](ci.md) · [Math](math.typ) · [References](references.md) · [Milestone sheets](milestones/)

---

## 1. Why this exists

Ten milestone documents were written in one pass, and six blocking conditions ended up waived by their own failure tables — cook time, encoding, teleport recovery, scaling, S9 and mesh shaders. Each waiver was written while trying to be helpful about diagnosis, and each one said, in effect, *this gate does not actually block*.

**Investigating a failed budget is sensible. It does not make the gate pass.** A measured hypothesis may be revised, but that produces an explicit new acceptance baseline with a recorded reason, preserving the original result. It never silently converts a `required` row into a `diagnostic` one.

## 2. Status vocabulary

| Status | Meaning |
| --- | --- |
| `required` | Blocks its release. No failure-table text may soften it |
| `diagnostic` | Recorded, reported, never blocks. May become `required` only by an explicit baseline change |
| `conditional` | Blocks **if** its precondition holds. The precondition is named in the row |
| `not_applicable` | Out of scope for this backend or release. Distinct from a failure and from `UNAVAILABLE` |

A run reports `PASS`, `FAIL`, `NOT_IMPLEMENTED`, `UNAVAILABLE` or `NOT_RUN` per row, per backend ([ci.md §4](ci.md)). `UNAVAILABLE` is never a pass.

## 3. Changing a threshold

Three things, in the same commit:

1. The new threshold, with the measurement that justifies it.
2. The **original result preserved** with its original threshold, marked superseded.
3. A `decision_id` in the roadmap decision log.

A threshold changed without all three is a waiver wearing a number.

---

## 4. The registry

`due` is the release at which the row must be green. `backend` is `L`, `A`, `both`, or `linux` for host-only work.

### R0 — B0

| id | feature | due | backend | status | threshold | decision |
| --- | --- | --- | --- | --- | --- | --- |
| B0.1 | Required baseline kernels compile to SPIR-V and through `metal`/`metallib` | R0 | both | required | all baseline kernels | D3 |
| B0.2 | Optional capability probes compile **or** report unsupported | R0 | both | diagnostic | unsupported is a valid result | D3 |
| B0.3 | CI green, one golden-image fixture, cross-backend comparison | R0 | both | required | exact | — |
| B0.4 | Slang, meshoptimizer, `clusterlod.h` versions pinned in every manifest | R0 | linux | required | exact | — |
| B0.5 | Reversed-Z cleared frame presents | R0 | both | required | exact | D1 |
| B0.6 | Timestamps agree with an independent supported timing method | R0 | both | required | ≤ 5% on matched scopes, uncertainty recorded | — |
| B0.7 | Near-plane and overlap geometry probe: winding, clipping, depth order | R0 | both | required | exact | D1 |
| B0.8 | All reference, builder, device and ratio measurements recorded | R0 | both | diagnostic | no threshold | — |

### R1 — B1-leaf

| id | feature | due | backend | status | threshold | decision |
| --- | --- | --- | --- | --- | --- | --- |
| B1L.1 | Q0 — no invalid indices, nonfinite values, OOB access, validation errors | R1 | L | required | exact | — |
| B1L.2 | Repeated cooks produce identical package hashes | R1 | linux | required | exact | D5 |
| B1L.3 | Seam vertices decode bit-identically | R1 | linux | required | exact | — |
| B1L.4 | Complete primitive and material coverage | R1 | linux | required | exact | — |
| B1L.5 | Position error inside the quantization bound | R1 | linux | required | ≤ 10⁻⁵ of the diagonal | — |
| B1L.6 | Cluster and triangle ID masks match the bounded CPU oracle | R1 | L | required | exact on interior pixels | — |
| B1L.7 | Clean cook of 10 M triangles | R1 | linux | required | ≤ 2 min at the recorded admission limit | D4 |
| B1L.8 | Cook peak RSS, and the largest per-task footprint, both recorded | R1 | linux | required | ≤ 8 GiB total | D4 |
| B1L.9 | Encoded leaf geometry | R1 | linux | required | ≤ 16 B per source triangle | D7 |
| B1L.10 | Walkable: collision, jump, reset, reproducing replay | R1 | L | required | exact | — |
| B1L.11 | Reveal reads captured selected geometry | R1 | L | required | inspected | — |
| B1L.12 | Multi-page group passes the format tests **before** the envelope freezes | R1 | linux | required | exact | — |
| B1L.13 | Leaf GPU render and CPU preparation timings | R1 | L | diagnostic | no threshold | — |

**B1L.7 and B1L.9 are `required`.** If either fails, the release does not pass until the threshold is formally changed under §3. Diagnosing against R0's measured throughput is how you decide whether to change it — not a substitute for changing it.

### R2 — B1-DAG and B2-selection

| id | feature | due | backend | status | threshold | decision |
| --- | --- | --- | --- | --- | --- | --- |
| B1D.1 | Q1 — legal cuts, unique emission, coherent group decisions | R2 | L | required | exact | — |
| B1D.2 | Zero internal crack pixels on shared borders | R2 | L | required | exact, no tolerance | — |
| B1D.3 | **Projected**-error monotonicity on every tested path | R2 | L | required | exact | — |
| B1D.4 | Q2 — silhouette distance at 1.0 px on the frozen corpus | R2 | L | required | p99 ≤ 1.5 px, max ≤ 3 px | D7 |
| B1D.5 | Declared error ≤ target, fallback exceptions counted **by reason** | R2 | L | required | exceptions classified, not merely labelled | — |
| B1D.6 | Selected triangles against the fixture's frozen curve, dense surfaces | R2 | L | required | ≤ 25% of leaf triangles | — |
| B1D.7 | 1 px → 2 px does not increase selected count after settling | R2 | L | required | ≤ 1% | — |
| B1D.8 | Hysteresis settles on a static camera | R2 | L | required | ≤ 10 frames | — |
| B1D.9 | Hierarchy size against the leaf package | R2 | L | required | ≤ 2.5× | D7 |
| B1D.10 | Differential agreement with `clusterlod.h` group output | R2 | linux | required | zero unexplained divergence | D0 |
| B1D.11 | Independent geometric validation, separate from B1D.10 | R2 | linux | required | per the error contract in [architecture §5](architecture.md) | — |
| B2S.1 | Q0, and CPU/GPU cut agreement with tie and hysteresis fixed | R2 | L | required | exact | — |
| B2S.2 | GPU selection plus compaction p95 | R2 | L | required | ≤ 2 ms | — |
| B2S.3 | CPU scene and render preparation p95 | R2 | L | required | ≤ 2 ms | — |
| B2S.4 | 1 K → 10 K instances outside the view: CPU preparation growth | R2 | L | required | ≤ 25%, absolute times reported | — |
| B2S.5 | Quarter-capacity queues: overflow detected, complete fallback cut | R2 | L | required | exact | — |
| B2S.6 | Overflows in normal runs | R2 | L | required | zero | — |
| B2S.7 | Indirect counts from GPU output, no blocking current-frame readback | R2 | L | required | exact | — |
| B2S.8 | Mesh-shader and indexed-draw paths produce identical ID masks | R2 | L | required | exact | D3b |
| B2S.9 | Mesh shaders are the faster path on the fixtures | R2 | L | diagnostic | recorded | D3b |
| B1L.* | R1 feature set on Apple | R2 | A | required | as R1 | D2 |

**B2S.9 is `diagnostic` and B2S.8 is `required`.** If mesh shaders lose, the indexed path may become primary — but that is a change to D3b recorded under §3, not a failure-table aside. The parity condition holds either way.

### R2 — D6 experiment

| id | feature | due | backend | status | threshold | decision |
| --- | --- | --- | --- | --- | --- | --- |
| D6.1 | Triangle-count distribution by projected area | R2 | L | required | recorded | D6 |
| D6.2 | Covered-pixel and depth-complexity distribution | R2 | L | required | recorded | D6 |
| D6.3 | Controlled total-visibility timings, with the attribution method stated | R2 | L | required | recorded | D6 |
| D6.4 | Threshold *p* and its implied frame benefit `S = 1/((1−p) + p/s)` written down **before** the result is read | R2 | — | required | in advance, with the calendar cost beside it | D6 |
| D6.5 | Branch decision recorded, with a priced calendar alternative | R2 | — | required | written | D6 |

D6 produces no pass or fail. Its rows are `required` because the *experiment* must be run and its terms fixed in advance, not because any outcome is preferred.

### R3 — B2-occlusion and B3-streaming

| id | feature | due | backend | status | threshold | decision |
| --- | --- | --- | --- | --- | --- | --- |
| B2O.1 | Q4 — zero falsely missing interior visible pixels | R3 | L | required | exact | — |
| B2O.2 | Raster-submitted triangle reduction, occluded steady view | R3 | L | required | ≥ 80% | — |
| B2O.3 | Total geometry GPU time reduction versus HZB disabled | R3 | L | required | ≥ 35% | — |
| B2O.4 | Matched open-scene overhead | R3 | L | required | ≤ 15% | — |
| B2O.5 | HZB build plus tests p95 | R3 | L | required | ≤ 1.5 ms | — |
| B2O.6 | Minimum reduction under reversed Z, verified against a CPU reduction | R3 | L | required | exact | D1 |
| B2O.7 | Odd dimensions, sub-pixel bounds and near-plane cases in the HZB helper | R3 | L | required | exact | — |
| B3S.1 | Q0/Q1/Q4 on the demo corpus at the measured ratio | R3 | L | required | ≥ 9:1 decoded | D7 |
| B3S.2 | An incomplete split group never activates | R3 | L | required | exact | — |
| B3S.3 | Valid resident cut survives every eviction; no stale readers | R3 | L | required | exact | — |
| B3S.4 | Simultaneous resident union inside the pool, **measured byte sets** | R3 | L | required | exact, per profile | D7 |
| B3S.5 | No synchronous page wait on the render thread | R3 | L | required | exact | — |
| B3S.6 | Streaming bookkeeping p95 | R3 | L | required | ≤ 1 ms | — |
| B3S.7 | Upload and copy critical path p95 | R3 | L | required | ≤ 2 ms | — |
| B3S.8 | A traversal exceeds its pool and safely reuses retired slots | R3 | L | required | exact | — |
| B3S.9 | Corrupt page leaves the fallback visible, recoverable error | R3 | L | required | exact | — |
| B3S.10 | Delayed completion produces no stale reader on any queue | R3 | L | required | exact | — |
| B3S.11 | Teleport recovery, 20 scripted, deadline on `t_quality` | R3 | L | required | ≤ 1 miss of 20 | — |
| B3S.12 | Metadata paging correct on a small synthetic missing-metadata fixture | R3 | L | required | exact | D7 |
| B3S.13 | Metadata paging on the 512 and 256 MiB profiles | R3 | L | required | resident union fits | D7 |
| S9.1 | Full-scene corpus cooked and run at the measured ratio | R3 | L | conditional | **if** free disk and bake memory allow; else `UNAVAILABLE` | D7 |
| S9.2 | Metadata paging under 1.52 GiB of records | R3 | L | conditional | precondition S9.1 | D7 |
| B1D.*, B2S.* | R2 feature set on Apple | R3 | A | required | as R2 | D2 |

**S9 is `conditional`, and this is the one place the parent documents were loose.** The commitment is to *attempt* the full-scene measurement with a named precondition, not to ship it. Its unavailability does not violate a release exit, and the roadmap's exit text says the same. S9 also sits **above** the Metal backend on the descope ladder: dropping an optional measurement before a promised platform is the coherent order.

### R4 — B3-temporal, B4, B4-M

| id | feature | due | backend | status | threshold | decision |
| --- | --- | --- | --- | --- | --- | --- |
| B3T.1 | Q6 — residual against an **independently validated reference** | R4 | both | required | threshold calibrated at R4 and frozen | — |
| B3T.2 | Motion vectors validated on known camera and rigid motion | R4 | both | required | exact, **before** Q6 uses them | — |
| B3T.3 | No hidden opacity holes promoted to opaque occlusion | R4 | both | required | exact | — |
| B3T.4 | No bounds-culling losses at permitted deformation | R4 | both | required | exact | — |
| B3T.5 | Unlit textured MAE outside the edge band | R4 | both | required | ≤ 2/255 | — |
| B3T.6 | Material sweep plus an energy-conservation reference fixture | R4 | both | required | per fixture tolerance | — |
| B3T.7 | Cascade transition and light-leak tolerances | R4 | both | required | numerical, not qualitative | — |
| B3T.8 | Analytic gradients against a stated comparison method | R4 | both | required | mip and footprint difference within a declared bound | — |
| B3T.9 | Masked fixture coverage | R4 | both | required | ≥ 99.9% | — |
| B3T.10 | Full S7 renderer GPU p95 | R4 | L | required | ≤ 18 ms | — |
| B3T.11 | Material resolve plus lighting p95 | R4 | L | required | ≤ 7 ms | — |
| B4.1 | Frame interval p95 / p99 | R4 | L | required | ≤ 16.667 / 22 ms | D1 |
| B4.2 | Frame interval p95 / p99 | R4 | A | required | ≤ 33.333 / 45 ms | D1 |
| B4.3 | GPU p95 | R4 | L | required | ≤ 14 ms | — |
| B4.4 | GPU p95 | R4 | A | required | **frozen after B0** from the per-workload ratios | D2 |
| B4.5 | CPU active p95 | R4 | L | required | ≤ 5 ms | — |
| B4.6 | CPU active p95 | R4 | A | required | **frozen after B0** | D2 |
| B4.7 | Engine-cold to controllable first frame | R4 | both | required | ≤ 15 s | — |
| B4.8 | Reveal-only GPU overhead p95 | R4 | both | required | ≤ 1 ms L, ≤ 2 ms A | — |
| B4.9 | Input to updated frame p95 | R4 | both | required | ≤ 50 ms L, ≤ 80 ms A | — |
| B4.10 | Warm-route hitch frames: **> 50 ms on L (3× budget), > 66.67 ms on A (2× budget)** | R4 | both | required | ≤ 0.1% each | D1 |
| B4.11 | 30-min soak p95 degradation | R4 | both | required | ≤ 15% | — |
| B4.12 | 60-min soak unexplained tracked growth | R4 | both | required | ≤ 64 MiB | — |
| B4.13 | Clean-machine install, three launches, no dev-environment assumptions | R4 | both | required | exact | — |
| B4.14 | The four reveal criteria | R4 | both | required | inspected, and blocking | — |
| B4.15 | L120 | R4 | L | diagnostic | no threshold, no pass or fail | D1 |
| B4.16 | Resolution, tessellation and occlusion scaling curves | R4 | both | diagnostic | recorded as curves | — |
| B4M.1 | Q0/Q3/Q5 across hardware, compute and hybrid dispatch | R4 | both | conditional | **if** D6.5 selected the branch | D6 |
| B4M.2 | Speedup on a preregistered sub-1 px² cell, where *p* ≈ 1 by construction. **A cell result is not a frame claim** — report the frame-level *S* separately | R4 | both | conditional | ≥ 2.0×, precondition B4M.1 | D6 |
| B4M.3 | Full-sweep median hybrid time versus hardware-only | R4 | both | conditional | ≤ 110%, precondition B4M.1 | D6 |
| B1D.*, B2S.*, B2O.*, B3S.* | Full feature set on Apple | R4 | A | required | everything **applicable** | D2 |

**B4.16 is `diagnostic`.** The scaling experiments are curves, not gates. Any text in the parent documents implying a 25% scaling gate is superseded by this row.

**"Everything" at R4 means every *applicable required* row.** An unselected compute-rasterizer branch is `not_applicable`, not a failure. An L-only publication is a legitimate result and is **not** a completed two-profile release unless D2 is formally changed under §3.

---

## 5. Generation

The Markdown tables in [benchmarks.md](benchmarks.md) and the [milestone sheets](milestones/) are views onto this file. Keep them generated from it, or checked against it in CI — a script that diffs `id`, `status` and `threshold` is enough and needs no dashboard.

The one thing CI must reject is a milestone document whose failure-handling text contradicts a `required` row here.
