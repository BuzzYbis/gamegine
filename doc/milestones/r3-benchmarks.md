# B2-occlusion and B3-streaming — R3 acceptance

**Gates for R3 · Profile L for R3 features, Profile A for R2 features · all results NOT RUN**

Parent documents: [Gates](../gates.md) · [Benchmarks](../benchmarks.md) · [R3 architecture](r3-architecture.md)

---

## 1. What these gates block

R4 may not start until both are green. Adding materials and a temporal filter on top of a streamer that holes is how a residency bug becomes a shading bug in the bug tracker.

## 2. Fixtures created here

| ID | Definition |
| --- | --- |
| S4 | 10 K props behind occluders, ≥ 90% hidden by oracle. Moving door, 180° turns, teleports, near-plane crossings, plus a matched open variant |
| S5 | **Zorah demo corpus** — a named, hashed ≈ 320 M-triangle subset. 9.5 GiB cooked, 9.5:1 against the 1 GiB pool, 19:1 against the 512 MiB stress pool. Route visits four working sets of ≈ 400 MiB, union 1.6 GiB |
| S9 | **Full Zorah scene** — 1.63 G unique, 18.9 G instanced. 48.6 GiB cooked, 48:1, 25.5 M DAG clusters whose 1.52 GiB of metadata exceeds the pool |

## 3. Procedure

**B2-occlusion.** S4 with HZB off and on, plus the matched open scene. Test the first frame after each scripted door opening, 180° turn, teleport and near-plane crossing.

**B3-streaming.** S5 at the 1 GiB normal pool on each device, the 512 MiB stress profile, and a 256 MiB diagnostic where roots fit. Normal and throttled I/O. Twenty alternating teleports across the four working sets. Delay upload completion; corrupt selected non-root pages; force replacement groups to straddle pages with exactly one part or dependency missing. Then S9.

## 4. B2-occlusion blocking conditions

**Profile A has R2 features at R3 and therefore has no occlusion path.** These rows are Profile L only; the Apple column gives the release at which each becomes due.

| # | Condition | L | A due |
| --- | --- | --- | --- |
| B2O.1 | Q4 — zero falsely missing interior visible pixels, including the first frame after disocclusion or camera cut | Exact | R4 |
| B2O.2 | Raster-submitted triangles reduced in the occluded steady view | ≥ 80% | R4 |
| B2O.3 | Total geometry GPU time reduced versus HZB disabled | **≥ 35%** | R4 |
| B2O.4 | Total geometry GPU cost increase on the matched open scene | ≤ 15% | R4 |
| B2O.5 | HZB build plus tests p95 | ≤ 1.5 ms | R4, cap frozen after B0 |
| B2O.6 | Pyramid reduces with **minimum** under reversed Z, verified against a CPU reduction | Exact | R4 |
| B2O.7 | HZB helper on odd image dimensions, sub-pixel bounds and near-plane cases | Exact | R4 |

Temporal false positives are correctness failures even when their average is small. A frame that drops a visible surface and recovers it next frame has failed B2O.1.

## 5. B3-streaming blocking conditions

**Profile L only; Profile A has no streaming path until R4.**

| # | Condition | L | A due |
| --- | --- | --- | --- |
| B3S.1 | Q0/Q1/Q4 on S5 at the **measured decoded ratio, ≥ 9:1** | Exact | R4 |
| B3S.2 | An incomplete split group never activates | Exact | R4 |
| B3S.3 | A valid resident cut survives every eviction; zero missing fallback surfaces, duplicated regions or stale readers | Exact | R4 |
| B3S.4 | **Measured simultaneous resident union** fits the pool, per profile — see §5b | Exact | R4 |
| B3S.5 | No synchronous disk access, decompression or page wait on the render thread | Exact | R4 |
| B3S.6 | Streaming bookkeeping p95 | ≤ 1 ms | R4 |
| B3S.7 | Upload and copy critical path p95 | ≤ 2 ms | R4, cap frozen after B0 |
| B3S.8 | At least one traversal loads cumulative decoded geometry exceeding its pool and safely reuses retired slots | Exact | R4 |
| B3S.9 | Corrupt page leaves the fallback visible and produces a recoverable error | Exact | R4 |
| B3S.10 | Delayed GPU completion produces no stale reader on any queue | Exact | R4 |
| B3S.12 | Metadata paging correct on a small synthetic missing-metadata fixture | Exact | R4 |
| B3S.13 | Metadata paging makes the 512 and 256 MiB profiles feasible | Exact | R4 |

Profile A's own required rows at R3 are the **B1-DAG and B2-selection sets**, run against R2's feature set.

### Teleport recovery

Across 20 scripted teleports, **at most one may miss its deadline**:

| I/O | L | A |
| --- | --- | --- |
| Normal | 1 s | 2 s |
| Throttled, 256 MiB/s + 10 ms | 3 s | 5 s |

This is **finite-corpus acceptance, not a reliability figure.** Even 20 successes out of 20 establish only an 86.1% one-sided 95% lower bound under independence. No result may be published as "95% of teleports recover".

**The deadline applies to `t_quality`. Both timestamps are reported.** `t_quality` is the first frame at which ≥ 99% of visible pixels meet requested error; `t_confirmed` ends 30 consecutive such frames. Confirmation alone costs 0.5 s at 60 Hz and 1 s at 30 Hz — a third of the L deadline, which is why it is not the gated quantity.

If quality drops below threshold during the confirmation window, the window **resets** and `t_confirmed` is taken from the new start; `t_quality` is unchanged. The deadline applies **separately to each pool size and each I/O configuration**, and every row is tagged with both.

**Sanity-check the throttled deadline against the bytes actually read.** The 1.56 s floor for 400 MiB at 256 MiB/s holds only if 400 MiB of **compressed disk bytes** must be read. If the figure is decoded demand, on-disk compression lowers the read floor and the binding constraint moves elsewhere. Derive the lower bound from the **measured missing disk bytes** for the route, then measure decode, upload and dependency-round delays separately. Until then the deadline is a hypothesis, not a target.

The 256 MiB diagnostic has correctness and boundedness conditions but **no quality deadline** when the working set does not fit.

## 6. S9 — the 48:1 measurement

Committed, and reported with its achieved decoded ratio. **It does not block the release.**

| S9 condition | Why |
| --- | --- |
| Metadata paging active and correct | 1.52 GiB of cluster records against a 1 GiB pool. Top twelve levels pinned at ≈ 8.4 MB; everything below pages with its geometry |
| A resident parent stops safely at a paged-out child | The failure metadata paging introduces is a traversal walking into records that are not there |
| Achieved ratio reported as **decoded bytes**, not archive bytes | 48:1 is a decoded-residency claim |
| Cook manifest records thread count and peak RSS | A cook that used sixteen threads and swapped is a different measurement |
| Free disk checked before the run | ≈ 120 GiB once source, upstream render cache and our output are counted |

If metadata paging is not ready, or disk is short, S9 is reported **unavailable** — never estimated — and S5's 9:1 carries the release.

## 5b. Residency accounting — measured byte sets, not estimates

The 400 MiB working set and the 512 / 256 MiB profiles were derived from a triangle-count estimate and do not survive contact with the metadata. At 5.0 M DAG clusters and 64 B per record, the demo corpus carries **305 MiB** of cluster records:

| Profile | Pool | Records if globally resident | Left for geometry pages |
| --- | --- | --- | --- |
| Normal | 1024 MiB | 305 MiB | 719 MiB |
| Stress | 512 MiB | 305 MiB | **207 MiB — a 400 MiB working set does not fit** |
| Diagnostic | 256 MiB | 305 MiB | **−49 MiB — infeasible** |

**Metadata paging is therefore required for the demo corpus, not only for S9.** "Where roots fit" is not the admission test; **all mandatory resident data must fit.**

Before S5 acceptance is constructed, measure and record five byte sets, and check admission against their **actual simultaneous union**:

| Byte set | Definition |
| --- | --- |
| Globally pinned | Records and roots resident for every view, counted **once** |
| Shared pages | Pages demanded by more than one view in the route, counted once |
| View-specific pages | Pages unique to one view |
| Dependency closure | Bytes a cut must hold to stay legal, including coarser fallback |
| In-flight and retiring | Staging, and slots not yet reusable |

The stress fixture is derived from that manifest, not assumed from it. Note the trap: if four "400 MiB working sets" each include the same globally pinned bytes, their union is not 1.6 GiB. With 305 MiB shared it is roughly 305 + 4 × 95 = **684 MiB**, which fits the normal pool and forces no eviction at all.

## 7. Three quantities, always separate

| Quantity | Meaning |
| --- | --- |
| Disk package bytes | Compression, page headers and padding included |
| Decoded unique page bytes | What would actually occupy physical slots |
| Route demanded-byte union | Pages the replay actually needs, reloads counted separately |

Four working sets of 400 MiB total 1.6 GiB, which is 1.56× a 1 GiB pool: enough to demonstrate eviction and genuine new demand, not enough by itself to exercise a 9:1 or 48:1 corpus. Report the corpus ratio and the route's demand as different numbers.

## 8. Evidence bundle

`results/B2-occlusion-L/`, `results/B3-streaming-L/`, `results/S9-L/`, `results/B1-DAG-A/`, `results/B2-selection-A/`, each with the standard bundle plus:

- `occlusion/` — per-frame visibility differences, disocclusion captures, open-scene overhead, HZB verification against the CPU reduction
- `residency.json` — requests, read and upload rates, unique resident slots, pinned bytes, eviction and reload rates, queue peaks
- `teleports/` — per-teleport `t_quality` and `t_confirmed`, fallback-pixel traces throughout recovery, pool size tagged on every row
- `pagesize.json` — padding, metadata and I/O amplification at the frozen page size
- `groups/` — incomplete-group captures, corrupt-page behaviour, delayed-completion traces
- `cascades.json` — shadow residency and request-class behaviour, produced by a **named synthetic multi-view fixture**: real cascades do not exist until R4, so the R3 workload is a declared set of additional views exercising the lower-priority request class
- `s9/` — the 48:1 run, or an explicit unavailability record with its reason
- `metal/` — Profile A tagged `feature_set_release: R2`

W3 is written from the occlusion bundle, W4 from the streaming and S9 bundles.

## 9. Failure handling

| Failure | Response |
| --- | --- |
| A false-occlusion hole | Blocking. The two-pass scheme exists to make this impossible; a hole means the conservatism is wrong somewhere |
| An incomplete group activates | Blocking. This is the failure mode split-group dependency closure exists to prevent |
| A stale reader on a retired slot | Blocking. Check for a CPU frame-count guess that survived the completion-token conversion |
| Teleport deadlines missed on more than one of twenty | Not automatically blocking. Decompose into read, decode, upload, dependency rounds and confirmation first — the deadline may be the wrong hypothesis rather than the code being slow |
| Metadata paging incomplete | S9 unavailable. Release proceeds at 9:1 |
| Open-scene overhead above 15% | Blocking. Two-pass occlusion that costs more than it saves on open scenes has failed its own premise |
