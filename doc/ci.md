# Continuous integration

Companion files: [Architecture](architecture.md) · [Roadmap](roadmap.md) · [Benchmarks](benchmarks.md) · [Gates](gates.md) · [Math](math.typ) · [References](references.md).

---

## 1. Why this exists

Two developers work two hours a day on one engine with two graphics backends, two operating systems, one shared geometry format and one shader source language that compiles to two targets. They integrate weekly at best, and neither can run the other's hardware.

Nothing in that arrangement keeps the two backends in agreement. A rule that says "a shader layout change updates both backends and both test-vector sets in the same commit" is a rule; CI is the enforcement. Without it, divergence is discovered at a release gate, weeks after the commit that caused it, when the session budget for bisecting is two hours a day.

The single job that matters most is **the nightly golden-image comparison on both machines**. Everything else in this document supports it.

---

## 2. Where it runs

**The two development laptops are the CI machines.** There is no cloud GPU that runs Vulkan 1.4 on an RTX 4060 Laptop and Metal 4 on an M3 Pro, and renting one would not test the hardware the project is qualified on. This is a constraint, not a preference, and it shapes everything below.

| Runner | Hardware | Runs |
| --- | --- | --- |
| `ci-linux` | Profile L laptop, self-hosted runner | Host build, shader compile for both targets, CPU fixtures, cooker, Vulkan golden images, timing smoke |
| `ci-mac` | Profile A laptop, self-hosted runner | Host build, Metal golden images, timing smoke, decode of Linux-cooked packages |
| `ci-cloud-linux` | Hosted Linux runner, no GPU | Host build, SPIR-V emission, CPU unit tests, linting, manifest schema validation |
| `ci-cloud-macos` | Hosted macOS runner, no GPU | Host build, **and the only place `metal` and `metallib` actually accept the MSL** |

Three consequences to design around:

- **The CI machines are also the development machines.** Jobs run overnight on a schedule, not on every push. A job that pins the GPU for forty minutes at 3pm costs a development session.
- **Jobs must be interruptible and must clean up.** A developer sitting down at the laptop cancels the run; the next scheduled run starts from scratch rather than from a half-finished state.
- **Cloud runners catch the cheap failures fast.** Anything that does not need a GPU runs on every push, so a broken build or a shader that fails to compile for Metal is known within minutes rather than at 3am.

---

## 3. Job tiers

### Push and pull-request validation — cloud runners, target under 5 minutes

Named for what it does. A job cannot retroactively fail a push that has already happened; what it can do is block a merge, so these are configured as **required checks on the pull request**. Never needs a GPU.

An OS matrix is not optional here. Slang emitting MSL *text* on Linux and Apple's toolchain *accepting* that text are different checks, and only the second one proves the shader will load on Profile A. Run SPIR-V emission on Linux and `metal`/`metallib` compilation on macOS.

| Job | What it checks |
| --- | --- |
| Host build | Debug and release, both platforms' host code, warnings as errors |
| Shader compile | Every Slang kernel emits SPIR-V (Linux) **and** compiles through `metal`/`metallib` on macOS. A kernel that builds for one target and not the other fails here, not at a gate |
| Capability check | Each kernel's declared Slang capability profile matches its target's supported set. This is the mechanism that stops a Vulkan-only feature reaching the Metal backend |
| CPU fixtures | Math test vectors, quantization round-trips, projected-error monotonicity, cut-validity unit tests, page-format serialization round-trips |
| Manifest schema | Every benchmark and cooker manifest validates against the schema |
| Format and lint | `clang-format`, static analysis on changed files |

### Nightly — both GPU runners, target under 45 minutes each

Fails the branch and notifies both developers. This is the tier that keeps the backends honest.

| Job | What it checks |
| --- | --- |
| **Golden images** | Depth and visibility-ID masks for every fixture, on both backends, against frozen references. See §4 |
| **Cross-backend agreement** | Vulkan and Metal outputs compared to each other, on the **common supported feature set only** (see eligibility below), independently of their agreement with the golden reference. Both drifting together is still a failure |
| Cooker determinism | Cook the small fixtures twice with a clean cache, compare package hashes, compare against the previous night's hashes |
| Package portability | `ci-mac` loads and decodes packages cooked on `ci-linux` that night. Cooking is Linux-only, so this is the only thing proving the packages travel |
| Memory caps | Every fixture runs inside the profile caps. Allocated and used bytes reported separately |
| Timing smoke | One 20-second replay per fixture, single run. Not a benchmark — a tripwire for a 3× regression |
| Validation layers | A separate run with Vulkan validation and Metal API/shader validation enabled. Zero errors required |
| Leak check | Tracked GPU and host memory returns to baseline after fixture teardown |

### Weekly — both GPU runners, target under 2 hours

Runs Saturday night. Informational: it reports, it does not fail the branch.

One 120-second route per profile, single run, full telemetry, plus a trend chart against the last twelve weeks. This is where slow drift becomes visible before a release gate discovers it as a cliff.

### Gate — manual, at a release boundary only

The full five-run protocol in [benchmarks.md §2](benchmarks.md). Hours of exclusive machine time per device. Triggered by hand, never on a schedule.

---

## 4. Feature eligibility while Metal trails

D2 has the Metal implementation one release behind Vulkan. Without an explicit rule, a new Vulkan R2 fixture demands equivalent Metal R2 output that by design does not exist, and the only way the job stays green is a silent skip. Silent skips are how a backend quietly stops being tested.

The manifest carries a matrix of **fixture × backend × feature-set release**, with five statuses:

| Status | Meaning |
| --- | --- |
| `PASS` / `FAIL` | Ran and compared |
| `NOT_IMPLEMENTED` | The backend's accepted feature set does not include this yet. Expected, and dated |
| `UNAVAILABLE` | Runner asleep, busy, or the device was not reachable. **Not a pass** |
| `NOT_RUN` | Not scheduled in this tier |

Rules that follow:

- Cross-backend agreement runs on the intersection of supported sets, never on the union.
- A Vulkan-only new-feature fixture still runs, compared against the **CPU or indexed reference** rather than against Metal.
- `NOT_IMPLEMENTED` carries the release by which it must clear. At a release boundary, the specific Metal subset promised by D2 must be `PASS`; nothing may remain `NOT_IMPLEMENTED` past its date without being recorded as a schedule event.
- An `UNAVAILABLE` result preserves the last successful result **and its age**, displayed on the report page. A three-week-old green is not a green.

## 5. Golden images

The heart of the system, and the part with the most ways to get subtly wrong.

**What is compared.** Not screenshots. For each fixture and camera, CI compares:

- The **depth mask** — normalized depth per pixel.
- The **visibility-ID mask** — instance, cluster and local triangle identity per pixel.
- A **deterministic unlit render** in linear RGB.

Identity masks are the important ones. A lit screenshot can match while the renderer selects the wrong geometry and happens to shade it similarly; an ID mask cannot.

**Fixed settings.** TAA, jitter, motion blur, auto exposure and every stochastic effect off. Fixed seeds, fixed exposure, fixed camera, fixed LOD error target, residency forced fully available. A golden image that depends on streaming timing is not a golden image.

**Tolerances**, matching the correctness definitions in the benchmark file:

| Comparison | Tolerance |
| --- | --- |
| Interior pixels, ID mask | Exact. Zero mismatches |
| Depth, covered pixels | ≤ 2×10⁻⁵ normalized for 99.99% of pixels |
| Unlit linear RGB | MAE ≤ 1/255 outside the documented edge band |
| Silhouette and tie band | One-pixel band excluded, and **the exclusion mask is archived with the result** |

The exclusion mask is archived because an edge exclusion that quietly widens is how a systematic raster gap hides for months.

**Storage.** Reference images are content-addressed and stored outside the git history — a plain object store keyed by hash, with the manifest in the repository recording which hash belongs to which fixture and revision. Committing PNGs into git makes the repository unusable within a year.

**Updating a reference is a reviewed act.** A golden image changes only through an explicit update commit that states why the output legitimately moved, links the run that produced the new reference, and is approved by the other developer. Never automatically, never silently, never as a side effect of a build fix. A CI system that regenerates its own baselines tests nothing.

**When a golden image fails**, the job publishes the reference, the actual, the absolute difference, the exclusion mask and the per-pixel worst offender coordinates. Two hours a day is not enough time to reproduce a failure locally before understanding it.

---

## 6. Pinned inputs

Reproducibility is worth nothing if the inputs move. These are pinned by exact version or commit, recorded in every manifest, and bumped only in a dedicated commit:

| Input | Bump policy |
| --- | --- |
| Slang compiler | Named new baseline. Shader hashes change; golden images are re-verified before the reference is updated |
| `clusterlod.h` commit | Treated as a content format change: full re-cook, new package hashes, named new baseline. Its API is explicitly evolving upstream |
| meshoptimizer version | As above |
| Physics, window/input, codec libraries | Ordinary dependency bump; pre-push tier must stay green |
| Vulkan SDK, driver, macOS and Xcode | Cannot be fully pinned. **Recorded per run** and treated as a valid explanation for a timing change, never for a correctness change |

A correctness failure that coincides with a driver update is still a correctness failure until proven otherwise.

---

## 7. Results and artifacts

Every GPU-tier run produces a result bundle in the format defined in [benchmarks.md §6](benchmarks.md): `manifest.json`, frame and streaming samples, aggregate JSON, correctness images and masks, logs, and a short Markdown interpretation.

Bundles are retained for the last 30 nightly runs, every weekly run, and **every gate run forever**. Gate bundles are the evidence behind published claims and behind the release report; they outlive the machines that produced them.

Reporting is a static HTML page generated from the bundles and published from the repository. No dashboard service, no database, no hosted analytics. It shows: pass or fail per job per machine, the trend of timing smoke and weekly route figures, current memory headroom against the caps, and how far Metal is behind Vulkan in feature coverage.

That last row exists because the Metal backend trails Vulkan by one release by design, and "one release behind" degrades into "deferred" unless something displays the gap every day.

---

## 8. Adversarial fixtures

These are small, cheap and each one protects a named correctness risk from the architecture or the mathematics. They are not a test-count target.

| Fixture | Protects |
| --- | --- |
| Negative-zero and NaN depth values | The float-ordering property holds only for canonical non-negative finite depth. A `-0.0` has bit pattern `0x80000000` and wins every atomic max |
| Mirrored and sheared instances | `max │scale│` is exact for `R·diag(s)` and wrong under shear; mirrored instances also flip winding |
| Grazing normal cones | The cone test's angular spread term, and the reject-or-fallback policy for singular transforms |
| Odd-size HZB levels | Non-power-of-two mip dimensions and footprint padding at level boundaries |
| Missing closure member | A split group must not activate with one dependency absent |
| Simultaneous activation and eviction | The transition window where old fallback, new closure, staged payload and live readers all occupy the pool at once |
| Delayed completion on a second queue | Retirement must track every consumer across every queue, not just the raster queue |
| Generation wrap simulation | A wrapped generation counter aliases a stale reference to a live slot |
| Corrupted length and offset fields | Rejection before GPU publication, with no read past the end |

## 9. Runner hygiene

Three rules that are cheap now and expensive later.

**Same inputs.** GPU runners consume the same source revision and the **exact Linux-cooked package hashes**, recorded in the manifest. A runner that cooked its own packages is testing a different artifact.

**The cooker runs containerised, and that is the answer to the single-point-of-failure problem.** D5 makes cooking Linux-only, which makes the Linux laptop the only machine that can produce content. A pinned container image running the cooker on `ci-cloud-linux` removes that: fixtures are cooked and archived off-machine nightly, so a dead laptop costs hardware and not the ability to generate assets.

Two conditions make it trustworthy. The image **pins the toolchain and the ISA baseline**, because FMA contraction and vectorization decisions are made at compile time and a different `-march` produces a different mesh. And the container's output hashes are compared against the laptop's for the same inputs — if they ever diverge, the determinism claim underlying every package hash is false and that is worth knowing immediately rather than at a gate.

The corpus still cooks on the laptop: 90 minutes under a measured memory admission limit is not a hosted-runner workload. What the container protects is the fixtures, the pipeline and the ability to rebuild.

**Cook provenance.** Every cooked package records its **thread count and peak RSS**. The committed corpus is cooked at eight threads or fewer to stay inside 32 GB; a cook that used sixteen and swapped produced the same bytes by a different process and its timing is not comparable. CI never cooks the corpus — only the small fixtures — but it does verify that the corpus manifest carries these fields before a gate run uses it.

**Isolation.** Self-hosted runners must not execute untrusted pull-request code — a fork PR that runs arbitrary commands on a developer's laptop is a real exposure. Restrict cache write credentials to the intended producer; everything else reads.

**Honest reporting.** A sleeping or busy laptop produces `UNAVAILABLE`, never a pass. The report shows the last successful result together with its age.

**The 45-minute nightly target is a hypothesis.** It was chosen before any fixture existed. Measure it as fixtures accumulate, and separate tiny correctness fixtures from full-quality source references — otherwise the target becomes unreachable without anyone consciously changing test policy, and the usual resolution is to quietly drop a job.

## 10. What CI does not do

Stated explicitly, because each of these is a plausible-sounding project that would consume weeks:

- **No distributed or remote build farm.** Two laptops.
- **No cloud GPU testing.** The qualification hardware is the hardware.
- **No automatic performance gating.** Timing smoke catches cliffs; real numbers come from the five-run protocol under human supervision. Auto-failing on a 6% move produces noise, and noise produces disabled jobs.
- **No custom dashboard framework.** Static HTML from JSON.
- **No auto-updating golden references.**
- **No cooking of the production corpus in CI.** Fixtures only. The corpus is cooked on demand and lives in the content cache.

---

## 11. Build-out

Roughly 80 team-hours during R0, then ongoing maintenance inside each release.

| Step | When | Delivers |
| --- | --- | --- |
| 1 | R0, first | Self-hosted runners registered on both laptops; overnight schedule; cancel-on-developer-login |
| 2 | R0 | Pre-push tier: host build, Slang compile for both targets, capability check, CPU fixtures |
| 3 | R0 | Golden-image harness with one trivial fixture, green on both machines. **This is the R0 exit criterion** |
| 4 | R1 | Cooker determinism, package portability, memory caps, real fixtures as they appear |
| 5 | R2 | Cross-backend agreement job; the Metal-lag indicator on the report page |
| 6 | R3 | Streaming fixtures with injected I/O limits; leak check extended to page lifetimes |
| 7 | R4 | Clean-machine install verification; release archive production |

Each release adds its fixtures to the nightly tier as part of that release's work, not afterwards. A fixture that exists only in a gate run is a fixture that regresses unnoticed.

---

## 12. Content and the cache

Cooked packages are build artifacts produced on Linux. CI cooks the small fixtures itself every night as a determinism check. The production corpus is cooked on demand and published to a content cache keyed by input hash, cooker version, settings and asset manifest hash.

Both machines read the cache; only Linux writes to it.

The Linux laptop is therefore a single point of failure for content. The cache and the source assets need an off-machine backup. That is an operational requirement, and it is the kind that is obvious in hindsight and expensive at the time.
