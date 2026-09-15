# B3-temporal, B4 and B4-M — R4 acceptance and release

**Gates for R4 · both profiles, everything · all results NOT RUN**

Parent documents: [Gates](../gates.md) · [Benchmarks](../benchmarks.md) · [R4 architecture](r4-architecture.md)

---

## 1. What these gates block

The release. B4 is the only gate whose failure has no next milestone to defer to.

## 2. Profile scope

Both profiles, on **everything**. Metal catches up fully at R4, so `feature_set_release` is R4 on both sides and no `NOT_IMPLEMENTED` may remain.

## 3. Fixtures created here

| ID | Definition |
| --- | --- |
| S7 | 32 PBR variants, textured slanted surfaces, 10 K two-sided leaf cards, bounded wind, four-cascade shadows. Used only by B3-temporal |
| S8 | Final walkable route through the S5 corpus: ≈ 320 M unique, ≥ 1 G source-equivalent instanced, 9.5 GiB cooked, small foliage patch. Close-up detail, an occluding passage and a distant vista all on the route |
| S6 | Controlled triangles at median projected areas 0.25, 1, 4, 16, 64 px², each at depth complexity 1, 4, 16, matched coverage per pair. **Only if the R2 histogram justified B4-M** |

## 4. B3-temporal blocking conditions

Everything involving S7, plus the numerical Q6 threshold. None of it was measurable at R3, because S7 and TAA did not exist.

| # | Condition | L | A |
| --- | --- | --- | --- |
| B3T.1 | **Q6** — candidate-minus-**reference** residual on aligned valid samples, reported as temporal bias, RMS and variance, below the threshold calibrated here and frozen | Exact | Exact |
| B3T.1a | Motion vectors validated independently on known camera and rigid motion, **before** Q6 uses them to align | Exact | Exact |
| B3T.2 | Zero hidden opacity holes promoted to opaque occlusion | Exact | Exact |
| B3T.3 | Zero bounds-culling losses at permitted deformation | Exact | Exact |
| B3T.4 | Unlit textured MAE outside the declared edge band | ≤ 2/255 | ≤ 2/255 |
| B3T.4a | **Material sweep plus an energy-conservation reference fixture.** An unlit MAE test does not validate PBR lighting | Per-fixture tolerance | Per-fixture tolerance |
| B3T.4b | Cascade transition and light-leak **numerical** tolerances, not qualitative absolutes | Declared bound | Declared bound |
| B3T.5 | Masked fixtures match coverage pixels | ≥ 99.9% | ≥ 99.9% |
| B3T.6 | Out-of-envelope deformation rejected, or bounds expanded, **before** rendering. Expanded bounds fix **culling**, not LOD approximation error: if corresponding surfaces undergo the same Lipschitz deformation with constant *L*, a positional bound propagates as *E_t ≤ L·E_0*, and amplitude alone does not establish *L*. For the small foliage patch, keeping deformed assets on a **leaf-only path** is the chosen scope | Exact, with the leaf-only choice documented | Exact |
| B3T.7 | Full S7 renderer GPU p95 | ≤ 18 ms | ≤ 36 ms |
| B3T.8 | Material resolve plus lighting p95 | ≤ 7 ms | ≤ 14 ms |
| B3T.9 | Mip selection from analytic gradients, against a **stated comparison method** with an acceptable mip and footprint difference. Shader finite differences are not an exact analytic-derivative oracle — name what is being compared | Declared bound | Declared bound |

**Q6 needs an independent reference, and a raw frame difference is not one.** Two runs of the same renderer agree with each other whether or not either is right, and a deterministic renderer run twice produces a zero residual that measures nothing.

The reference is a **finest-detail render on tractable fixtures** — LOD selection pinned to the leaf level, so LOD-induced temporal error is isolated from legitimate change — with matched exposure and temporal settings. Reproject the candidate onto the reference using motion vectors that have themselves been validated (B3T.1a), then report bias, RMS and variance of the residual on aligned valid samples.

Freeze the exclusion masks, **cap the excluded fraction**, and use separate calibration and evaluation captures. Absolute correctness remains Q2, Q3 and Q5's job; Q6 measures temporal stability against a reference that does not switch LOD.

## 5. B4 blocking conditions

| Gate | L | A |
| --- | --- | --- |
| Frame interval p95 / p99 | ≤ 16.667 / 22 ms | ≤ 33.333 / 45 ms |
| GPU p95 | ≤ 14 ms | from B0's per-workload ratios |
| CPU active p95 | ≤ 5 ms | as above |
| Engine-cold to controllable first frame | ≤ 15 s | ≤ 15 s |
| Reveal-only GPU overhead p95 | ≤ 1 ms | ≤ 2 ms |
| Input to updated frame p95 | ≤ 50 ms | ≤ 80 ms |
| Warm-route hitch frames. The thresholds are **absolute perceptual limits, not a uniform multiple**: 50 ms is 3× L's budget, 66.67 ms is 2× A's. Stated explicitly because "3× budget" would put A at 100 ms, letting through hitches twice as long as L's | > 50 ms: ≤ 0.1% | > 66.67 ms: ≤ 0.1% |
| 30-min soak: p95 degradation, first against last 5 min | ≤ 15% | ≤ 15% |
| 60-min soak: net unexplained tracked memory growth after 10 min | ≤ 64 MiB | ≤ 64 MiB |

Plus: every **applicable required** row in the [gate registry](../gates.md) — an unselected compute-rasterizer branch is `not_applicable`, not a failure, and an L-only publication is a legitimate result but **not** a completed two-profile release unless D2 is formally amended. Q0–Q6 where applicable; all caps from [architecture §7](../architecture.md); no in-route shader compilation after prewarming; zero crashes, GPU hangs, unrecoverable page errors or validation defects in correctness runs; reveal colours corresponding to captured selected geometry; page residency demonstrably changing after leaving the initial working set; and the four qualitative reveal criteria.

**L120 is measured and reported with no budget table and no pass or fail.**

## 6. Scaling experiments

Recorded, not gated, and reported as curves.

These are `diagnostic` in the [gate registry](../gates.md). Any text elsewhere implying a 25% scaling **gate** is superseded by that row.

| Experiment | What it shows |
| --- | --- |
| 720p / 1080p / 1440p: geometry, shading, total GPU time | How cost splits between screen-bound and geometry-bound work |
| 1×/2×/4× source tessellation, camera, coverage, error and residency fixed | Geometry GPU time and selected triangles should each grow ≤ 25% from 1× to 4× on the preselected non-pathological fixture. **This is a useful-LOD test, not an O(pixels) proof** |
| Fully occluded instances at 1×/2×/4× S4 scale | Submitted triangles should rise ≤ 10%. Report traversal cost even when raster work stays flat |

Foliage, open-scene and overdraw cases run separately, with no universal constant-cost claim.

**Selected triangles are a scaling observable, not a bound.** A 1 px error target implies no universal triangle count. The defect test is deviation from a frozen reference curve on a named fixture, not membership of a band.

## 7. B4-M — conditional

**This gate exists only if the R2 histogram showed a material fraction of rasterisation work below ~1 px².** If not: record the finding, skip this gate, publish it in W5.

| # | Condition |
| --- | --- |
| B4M.1 | Q0/Q3/Q5 with identical selected triangles across hardware-only, compute-only and hybrid dispatch |
| B4M.2 | At least one preregistered cell with median area ≤ 1 px² achieves **≥ 2.0×** speedup in total visibility time versus hardware-only, on each qualification GPU. In such a cell *p* ≈ 1 by construction, so this tests the rasterizer, **not the frame**. Report the frame-level `S = 1/((1−p) + p/s)` from the measured whole-scene *p* alongside it |
| B4M.3 | Full-sweep median hybrid time does not exceed hardware-only by more than 10% |
| B4M.4 | Per-device crossover thresholds frozen from a calibration subset, then evaluated on held-out camera variations |

Total visibility time includes classification, rasterization, clearing and merge. Report 64-bit atomic contention and scratch-buffer peaks. Shared atomic target first; separate attachments plus merge only if a device refuses, with the merge cost measured when used.

## 8. Procedure

1. Play S8 manually for ten minutes per platform.
2. Run the scripted route: close-up detail through an occluding passage into a vista, with jump, collision, reset, interaction and every reveal mode toggled **while moving**.
3. Full T3 protocol on frozen S8: warm, engine-cold, documented storage-cold where establishable, deterministic I/O stress.
4. Repeat the S2 and S4 correctness fixtures after optimization.
5. Install each release on its qualification machine with no development-environment assumptions. Three clean launches. A 60-minute soak.

If storage-cold cannot be established, report it **unavailable** and run the deterministic throttled-I/O test instead. State the gap in the release report rather than substituting warm numbers.

## 9. Evidence bundle

`results/B3-temporal-L/`, `results/B3-temporal-A/`, `results/B4-L/`, `results/B4-A/`, plus `results/B4-M-*/` if it exists. Each with the standard bundle plus:

- `release/` — archive, source revision, installation instructions, asset notices with every licence
- `manifests/` — both machine manifests, with sustained thermal and power curves across the full protocol
- `samples/` — raw per-frame CSV for all five runs, per configuration
- `memory/` — accounting against every cap, both profiles, with the A-profile process footprint
- `scaling/` — the resolution, tessellation and occlusion curves
- `q6/` — reprojected residuals, the calibration capture, the frozen threshold and its derivation
- `reveal/` — captures of every mode, the wipe transition, and a note on the four qualitative criteria
- `unavailable.md` — every measurement that could not be established, and why

A short demonstration video ships **alongside, not instead of**, the playable build.

## 10. Failure handling

| Failure | Response |
| --- | --- |
| L60 missed | Descope in order: scene polish, the compute rasterizer if built, L120 reporting, the foliage family. Do not relax the error target first — that changes every published number |
| A30 missed but L60 met | Check B0's per-workload ratios. If a specific workload scales worse than measured, that is the finding and it belongs in W5 |
| Q6 threshold cannot be calibrated | Blocking, but diagnose all three causes before concluding: an **invalid reference**, an **unsuitable metric**, or an unstable renderer. The failure table must not assume only the third |
| Metal has not caught up | Blocking for A, not for L. Publish the L result and record Metal's state honestly. Do not imply parity |
| Soak shows unexplained growth | Blocking. Sixty-four MiB over an hour is already generous; growth beyond it is a leak with a name |
| A reveal criterion fails | Not a numeric gate, and still blocking. The reveal is what the project exists to produce |
