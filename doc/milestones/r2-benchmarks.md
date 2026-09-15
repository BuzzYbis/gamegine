# B1-DAG and B2-selection — R2 acceptance

**Gates for R2 · Profile L for R2 features, Profile A for R1 features · all results NOT RUN**

Parent documents: [Gates](../gates.md) · [Benchmarks](../benchmarks.md) · [R2 architecture](r2-architecture.md)

---

## 1. What these gates block

R3 may not start until both are green. Streaming a hierarchy whose cuts are not provably legal means debugging residency and coverage simultaneously, and the two failure modes look identical from a screenshot.

## 2. Profile scope

| Profile | Gated on |
| --- | --- |
| L | R2 features: the DAG, GPU selection, mesh shaders |
| A | **R1 features** — leaf clusters, indexed indirect, cluster-ID visibility, the walkable shell (D2-d) |

Every result records which release's feature set Profile A was running. A Metal result marked `NOT_IMPLEMENTED` for an R2 feature is expected and carries the release by which it must clear. An `UNAVAILABLE` is never a pass.

## 3. Fixtures created here

| ID | Definition |
| --- | --- |
| S3 | 100 distinct 100 K-triangle props instanced to 1 K / 10 K / 100 K objects, frozen layouts and seeds |
| S2 | Extended — the seam corpus now carries expected cuts, not just expected cooks |

S4 is created at R3 with the occlusion work.

## 4. Procedure

**B1-DAG.** Sweep S1 and S2 at 0.5, 1.0, 2.0 and 4.0 px through 1,000 camera and scale configurations, including transitions between neighbouring hierarchy depths. Geometry fully resident, CPU selection, so the hierarchy is isolated from streaming. Compare a permanently locked-boundary tree diagnostic against the regrouped DAG on the same continuous surfaces. Diff every group against `clusterlod.h`'s own output.

**B2-selection.** S3 at 1 K / 10 K / 100 K. Compare CPU and GPU selected group IDs on a manageable subset with deterministic settings. Measure visited BVH nodes and dispatch count, not only final triangles. Force every queue to a quarter capacity.

**D6.** Histogram projected triangle area on S3 and an S8 preview at 1.0 px, by depth complexity, weighted by covered pixels and rasterisation time.

## 5. B1-DAG blocking conditions

| # | Condition | Target |
| --- | --- | --- |
| B1D.1 | Q1 — every legal cut covers each region exactly once; shared nodes never emitted twice; all group members use coherent decisions | Exact |
| B1D.2 | Zero internal crack pixels on shared-border fixtures | Exact |
| B1D.3 | **Projected**-error monotonicity on every tested dependency path, including nonuniform transforms and near-plane cases | Exact. Scalar monotonicity alone does not pass |
| B1D.4 | Q2 — sampled silhouette distance against source, at 1.0 px | p99 ≤ 1.5 px, max ≤ 3 px |
| B1D.5 | Every selected resident group's declared error ≤ target, unless classified as fallback | Exceptions counted and reported |
| B1D.6 | Selected triangles at 1.0 px on fixed distant views of dense continuous S1 surfaces | ≤ 25% of leaf triangles, while meeting B1D.4 |
| B1D.7 | Moving 1 px → 2 px does not increase selected triangle count after settling | ≤ 1% |
| B1D.8 | Hysteresis settles on a static camera | ≤ 10 frames |
| B1D.9 | Hierarchy size against the leaf package | ≤ 2.5× |
| B1D.10 | Differential oracle: unexplained divergence from `clusterlod.h` group output | Zero |

Thin and aggregate meshes are reported separately and never averaged into B1D.6. Sparse geometry resisting volume-preserving decimation is a known property of the technique, not a bug in the integration.

## 6. B2-selection blocking conditions

**Profile A has R1 features at R2 and therefore has no GPU selection.** These rows are Profile L only; the Apple column gives the release at which each becomes due, not a target it could meet now.

| # | Condition | L target | A due |
| --- | --- | --- | --- |
| B2S.1 | Q0, and CPU/GPU cuts match with tie and hysteresis state fixed | Exact | R3 |
| B2S.2 | GPU selection plus compaction p95 | ≤ 2 ms | R3, cap frozen after B0 |
| B2S.3 | CPU scene and render preparation p95 | ≤ 2 ms | R3, cap frozen after B0 |
| B2S.4 | 1 K → 10 K instances added outside the view raises CPU render preparation by | ≤ 25% | R3 |
| B2S.5 | Queues at quarter capacity: overflow detected, complete fallback cut emitted, zero holes | Exact | R3 |
| B2S.6 | Overflows in normal runs | Zero | R3 |
| B2S.7 | Indirect work counts derive from GPU output with no blocking current-frame readback | Exact | R3 |
| B2S.8 | Mesh-shader and indexed-draw paths produce identical ID masks | Exact | R3 |

Profile A's own required rows at R2 are the **B1-leaf set**, run against R1's feature set.

B2S.4 reports absolute times alongside the ratio. A ratio that holds because both runs are slow is not the property being tested.

## 7. The D6 decision

Not a pass or fail. It produces a distribution and a written decision.

| Recorded | Why all of them |
| --- | --- |
| **Triangle-count distribution** by projected area | Tiny triangles covering few or zero samples still cost setup and processing. Weighting only by covered pixels hides exactly the workload under investigation |
| **Projected-area distribution** | The shape, not a single fraction |
| Covered-pixel distribution | What the shading cost follows |
| Depth complexity | Overdraw changes the answer |
| **Controlled total-visibility timings**, with the attribution method stated | A final visibility buffer alone cannot account for submitted, hidden or zero-sample triangles. Say how time is attributed |

**The criterion is arithmetic, and is written down before the result is read.** With *p* the fraction of baseline total visibility time the compute path could accelerate and *s* its speedup on that portion, frame benefit is `S = 1/((1−p) + p/s)`, ceiling `1/(1−p)`. A 2× frame speedup needs **p > 0.50 even at infinite s**, and **p > 0.75 at a realistic s = 3**; 1.5× at s = 3 still needs p > 0.50. Record the chosen *p*, the frame benefit it implies, and the fifteen-week calendar cost side by side. Deciding the threshold after seeing the distribution is not a measurement.

If the predefined criterion is met, [B4-M](r4-benchmarks.md) exists and the compute rasterizer is scheduled — **with a priced calendar alternative or a named scope substitution**, since the branch is fifteen weeks. Otherwise it is not built, the measurement is published, and the question is not reopened. **The experiment is the only decision point.**

## 8. Evidence bundle

`results/B1-DAG-L/`, `results/B2-selection-L/`, `results/B1-leaf-A/`, each with the standard bundle plus:

- `hierarchy.json` — per-level reduction, error growth, locked-edge density, DAG depth and fan-in
- `locked-tree-comparison.json` — the regrouped DAG against a permanently locked tree, which is W2's finding
- `oracle-diff/` — group-by-group divergence from `clusterlod.h`, with explanations
- `monotonicity/` — every tested dependency path, with the nonuniform and near-plane cases called out
- `cuts/` — CPU and GPU selected-ID comparisons, tie and hysteresis state
- `traversal.json` — visited BVH nodes, dispatch counts, queue peaks, overflow behaviour at quarter capacity
- `raster-parity/` — mesh-shader against indexed-draw ID masks
- `histogram/` — the D6 distribution and the written decision
- `metal/` — Profile A results tagged with `feature_set_release: R1`

## 9. Failure handling

| Failure | Response |
| --- | --- |
| Monotonicity fails | Blocking. Force it during the build. Do not relax the gate |
| Cracks on shared borders | Blocking, no tolerance. Check the oracle diff first |
| Cut agreement diverges | Blocking. Tie and hysteresis state are the usual cause |
| Selected triangles far above 25% of leaves | Not automatically blocking on thin or aggregate meshes, which are reported separately. Blocking on dense continuous surfaces, where it means the selector is refining past target |
| Traversal overflows in a normal run | Blocking. The fallback path exists for stress, not for ordinary frames |
| Mesh shaders slower than indexed draws | B2S.8 (parity) is `required` and still blocks; B2S.9 (mesh shaders faster) is `diagnostic` and does not. Making indexed primary is an **amendment to D3b** recorded under [gates §3](../gates.md), not a failure-table aside |
| The whole hierarchy gate fails after the allotted weeks | Descend the fallback ladder in [architecture §9](../architecture.md). Step 1 is checking our side, not the builder |
