# Virtualized geometry demo engine — planning documents

C++23 engine with native Vulkan 1.4 on Linux/NVIDIA and native Metal 4 on Apple Silicon. A playable environment in which the player moves from a distant vista to close-up geometric detail, then reveals the actual triangles, clusters, LOD selection and streamed pages producing the image.

**Status: planning complete, no implementation. Every number here is a target or an estimate, and none is a measurement.** Replacing them with measurements is what R0 is for.

---

## Which file answers what

| File | Authoritative for | Open it when |
| --- | --- | --- |
| [architecture.md](architecture.md) | Platform contract, memory budgets, data formats, module ownership, decisions D0–D8 | You need to know what was decided and why |
| [gates.md](gates.md) | **Every acceptance condition and its status.** Where a table elsewhere disagrees, this file wins | Before claiming anything passed |
| [roadmap.md](roadmap.md) | Capacity, schedule, descope ladder, kill criteria, decision calendar | Planning, or when a date slips |
| [benchmarks.md](benchmarks.md) | Protocol, correctness definitions Q0–Q6, fixture corpus, evidence format | Designing or running a measurement |
| [ci.md](ci.md) | Job tiers, golden images, feature eligibility while Metal trails, runner hygiene | Setting up or debugging automation |
| [math.pdf](math.pdf) / [math.typ](math.typ) | The mathematics the engine depends on being correct | Implementing anything with a formula in it |
| [references.md](references.md) | 74 sources, each placed at the point it becomes load-bearing, with a reading depth | Before starting a release, or when stuck |
| [milestones/](milestones/) | Per-release working sheets, ten files | At the start of a release, and kept open through it |
| [pipeline-flow.html](pipeline-flow.html) | An animated model of cooking, streaming and the arena allocator | Explaining the system, or building intuition for residency |

The milestone sheets are working views onto the parent documents, not a parallel contract. Each architecture sheet carries a diagram of the pipeline **as it stands at the end of that release**.

## Decisions taken

| ID | Decision |
| --- | --- |
| D0 | Ship a playable demo. The cluster-DAG builder is integrated from `clusterlod.h`, not authored |
| D1 | L60 required, A30 required, L120 measured with no budget, no 240 Hz tier |
| D2 | Backend interface co-developed from R1; the Metal implementation trails Vulkan by one release |
| D3 | Slang as the single shader source, targeting SPIR-V and MSL |
| D3b | Mesh shaders are the primary raster path from R2; indexed indirect draw is kept permanently as oracle and fallback |
| D4 | Cook budgets are regression gates; work is admitted against a measured memory limit, not a thread count |
| D5 | Cooking is Linux-only. Cooked packages are build artifacts |
| D6 | **Deferred by design.** Whether a compute rasterizer is built at all is decided by the R2 experiment. Default: no |
| D6b | Deferred with D6. Shared 64-bit atomic target is the default if it reopens |
| D7 | Every sizing number derives from what a capability claim requires as proof |
| D8 | Geometry from the Zorah export; materials from a separate UV- and tangent-equipped fixture |

## Shape of the plan

Five releases, each producing something that runs. **3,276 team-hours, 205 weeks** at a sustainable pace of 16 team-hours per week, with the optional procedural generator adding 196 hours and 12 weeks.

| Release | Weeks | Hours | Deliverable |
| --- | --- | --- | --- |
| R0 | 1–14 | 224 | Toolchain, reference measurement, CI skeleton |
| R1 | 15–45 | 488 | **Walkable scene rendering cooked leaf clusters** |
| R2 | 46–100 | 882 | Cluster-DAG LOD, GPU selection, mesh shaders |
| R3 | 101–155 | 874 | Two-pass occlusion, streaming, metadata paging |
| R4 | 156–205 | 807 | Materials, shadows, TAA, qualification, release |

## Where to start

1. [milestones/r0-architecture.md](milestones/r0-architecture.md) — what R0 builds and why it produces almost no engine
2. [milestones/r0-benchmarks.md](milestones/r0-benchmarks.md) — what R0 must measure, and the four things that actually block
3. [references.md](references.md) §11 — the four sources to read before R0, and nothing else yet

## Three things to carry into implementation

**A threshold changed without a preserved original is a waiver wearing a number.** [gates.md §3](gates.md) sets the procedure: the new threshold with its justifying measurement, the original result preserved and marked superseded, and a decision-log entry — all in one commit. This discipline matters more in month thirty than month one, and month one is when it has to start.

**Estimates are named as estimates.** The production error path is measured error plus a recorded margin, which is not a bound. Certified bounds apply to small adversarial fixtures only. The asset manifest records which mode produced each asset's figures.

**Unavailable is never a pass.** A sleeping runner, an unestablished storage-cold state, an unbuilt full-scene corpus — each is reported as unavailable with its reason, never estimated and never substituted with a warm number.
