# B1-leaf — R1 acceptance

**Gate for R1 · Profile L only · all results NOT RUN**

Parent documents: [Gates](../gates.md) · [Benchmarks](../benchmarks.md) · [R1 architecture](r1-architecture.md)

The hierarchy half of B1, [B1-DAG](r2-benchmarks.md), gates R2. Nothing here tests LOD, because there is none.

---

## 1. What this gate blocks

R2 may not start until B1-leaf is green. A hierarchy built on a cooker that does not round-trip deterministically is a hierarchy nobody can debug.

## 2. Profile scope

**Profile L only.** Metal implements R1's feature set during R2 (D2-d), so Profile A is not gated here and produces no B1-leaf result. Cooker tests are Linux-only in every release (D5).

## 3. Fixtures created here

| ID | Definition |
| --- | --- |
| S1 | 10 M unique triangles, ten dense rigid meshes, hard edges, UV seams, multiple material sections. Promoted from R0's bare mesh to a full cooked fixture |
| S2 | ≥ 20 adversarial seam and format fixtures: shared borders, thin fins, disconnected leaves, narrow cavities, mirrored instances, nonuniform scale, near-plane crossings. Most of their value arrives at R2, but they must cook correctly now |

Both enter the nightly tier as they are created, not at the gate.

## 4. Procedure

1. Cook S1 three times from a clean cache, then once warm. Compare package hashes.
2. Cook S2. Verify every adversarial case survives import, reindexing and clustering.
3. Render all leaf clusters against the ordinary indexed renderer on a fixed camera.
4. Diff cluster and triangle ID masks against the CPU oracle, on **tiny correctness fixtures only**. The oracle declares its clipping, fill, depth-tie and interpolation policy explicitly, and both it and the GPU path consume the same independently specified geometry and camera — the expected result is never derived from the output being tested. Larger comparisons use the indexed hardware path.
5. Run one builder-generated multi-page group through the format tests, before freezing the page envelope.
6. Play the route manually for ten minutes. Run the deterministic replay.

## 5. Blocking conditions

| # | Condition | Target |
| --- | --- | --- |
| B1L.1 | Q0 — zero invalid indices, nonfinite positions, out-of-bounds shader accesses, validation errors | Exact |
| B1L.2 | Repeated cooks produce identical package hashes on the same toolchain | Exact |
| B1L.3 | Seam vertices decode bit-identically across clusters | Exact |
| B1L.4 | Complete primitive and material coverage — every source triangle appears exactly once | Exact |
| B1L.5 | Position error inside the declared quantization bound | ≤ 10⁻⁵ of the bounding-box diagonal |
| B1L.6 | Cluster and triangle ID masks match the **bounded** CPU oracle away from the documented tie band | Exact on interior pixels |
| B1L.7 | Clean cook of 10 M triangles, at the recorded memory admission limit | **≤ 2 minutes** |
| B1L.8 | Cook peak RSS **and** largest per-task footprint, both recorded. A thread count is not a memory bound | **≤ 8 GiB total** |
| B1L.9 | Encoded leaf geometry | **≤ 16 B per source triangle** |
| B1L.10 | Walkable: collision, jump, reset, and a replay that reproduces frame for frame | Exact |
| B1L.11 | The cluster-colour reveal reads captured selected geometry, not a second mesh | Inspected |
| B1L.12 | One multi-page group passes the format tests before the envelope is frozen | Exact |

Statuses are authoritative in the [gate registry](../gates.md). **B1L.7 is the regression gate.** Corpus cooks are capacity plans, not gates: ≤ 20 minutes for the 320 M demo corpus, ≤ 90 minutes for the full 1.63 G scene, both at ≤ 8 threads.

## 6. Development baselines, not gated

| Measurement | Expected | Why not gated |
| --- | --- | --- |
| Leaf GPU decode and render p95 on S1 | ≤ 8 ms | The renderer has no LOD, so this measures a workload that will never ship |
| CPU frame preparation p95 | ≤ 10 ms | Same. It becomes meaningful at B2-selection |
| Bytes per triangle by component: positions, attributes, indices, page overhead | recorded | Feeds the encoding work if B1L.9 is tight |
| Cluster fill: triangles and vertices per cluster, histogram | recorded | Below about 100 triangles per cluster the index field is buying nothing |

Q6 produces archived captures and human review. It is **informational until R4**, because TAA does not exist until then.

## 7. Evidence bundle

`results/B1-leaf-L/`, containing the standard bundle plus:

- `cook.json` — wall time, peak RSS, thread count, in-flight cap, package hashes across four runs
- `encoding.json` — bytes per source triangle by component, cluster fill histogram
- `quantization.json` — position error distribution, seam decode equality proof
- `masks/` — cluster and triangle ID masks, CPU reference, difference images, the tie-band exclusion mask **and its area**
- `format/` — the multi-page group test result, and the frozen page envelope with its rationale from R0's sweep
- `assets.json` — every source asset with its licence text, hash, and the demo-subset definition
- `replay/` — the deterministic route and a capture proving it reproduces

W1 is written from this bundle, and states plainly which parts were integrated and which were written here.

## 8. Failure handling

| Failure | Response |
| --- | --- |
| Package hashes differ between runs | Blocking. Determinism within one toolchain is achievable and is the basis of every later comparison |
| Seam decode differs | Blocking. This is a crack at R2 and there is no tolerance that makes it acceptable |
| Cook exceeds 2 minutes | **Blocking** (B1L.7 is `required`). Diagnosing against R0's measured throughput is how you decide whether to change the threshold — not a substitute for changing it. A revised budget is a recorded baseline change under [gates §3](../gates.md), preserving the original result |
| Encoding exceeds 16 B per triangle | **Blocking** (B1L.9 is `required`). It also changes the corpus arithmetic in D7, so a revised budget carries the recomputed ratios with it. Do not pad the corpus to preserve a ratio — select more content or amend the ratio goal |
| The multi-page group does not fit the format | **Do not freeze.** This is exactly what the test is for. Fix the format, then freeze |
