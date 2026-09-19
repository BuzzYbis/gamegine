# CI tooling

Automation for the job tiers in [ci.md](../doc/ci.md). Everything here is Lua
running under xmake, so the automation shares the project's toolchain and adds
no language to the stack. The one rule that shapes all of it:

> **Unavailable is never a pass.** A field that could not be read, a job with
> no inputs, a pin nobody has filled in — each is reported with its reason.
> None of them is ever defaulted, estimated, or allowed to look green.

---

## Tasks

Run any of these from the project root. They are the same invocations the
runner uses, because a check that can only be run by pushing is a check nobody
runs before pushing.

| Task | Does | Gate |
| --- | --- | --- |
| `xmake ci-check` | The whole push tier, in the runner's order | — |
| `xmake ci-format` | Checks first-party sources against `.clang-format`. `--fix` rewrites them | — |
| `xmake ci-pins` | Verifies `pins.json` against the machine | **B0.4** |
| `xmake ci-machine` | Captures this runner's machine manifest | — |
| `xmake ci-schema` | Validates JSON documents against a schema | — |
| `xmake ci-bundle` | Validates a result bundle's layout | — |
| `xmake ci-inventory` | Reports which CI inputs exist yet, and which do not | — |
| `xmake ci-test` | Runs the CI tooling's own tests | — |

```bash
xmake ci-check                              # before pushing
xmake ci-format --fix                       # after writing code
xmake ci-machine -o results/B0-A/machine.json
xmake ci-bundle results/B0-A                # before calling a gate green
```

---

## Layout

```
ci/
├── README.md                this file
├── pins.json                pinned and recorded inputs        (ci.md §6)
├── machines.json            which machine is which, and what its results are worth
├── tasks.lua                xmake task definitions
├── lua/
│   ├── probe.lua            run commands, report honestly
│   ├── machine.lua          machine manifest capture   (architecture.md §2)
│   ├── pins.lua             pin verification                     (B0.4)
│   ├── jsonschema.lua       a small JSON Schema subset
│   ├── bundle.lua           result-bundle layout         (benchmarks.md §6)
│   └── sources.lua          which files are ours to police
└── schema/
    ├── run-manifest.schema.json      the frozen manifest contract
    └── examples/                     committed tripwire fixtures
```

`.github/workflows/push.yml` is the push and pull-request tier.

---

## The probe record

Nothing in `machine.lua` returns a bare value. Every field is:

```lua
{value = "Apple M2", status = "OK",          source = "sysctl -n hw.model"}
{value = nil,        status = "UNAVAILABLE", source = "powermetrics ...",
                     reason = "requires sudo; not run unattended in CI"}
```

`source` is the exact command, so any number in a manifest can be reproduced
by hand. `reason` is written for someone reading it months later who has to
decide whether the gap matters.

---

## Pinned inputs

`pins.json` has two halves, and the split is the point.

**`pinned`** — bumped only in a dedicated commit. Slang, meshoptimizer and
`clusterlod.h`. A `clusterlod.h` or meshoptimizer bump is a *content format*
change: full re-cook, new package hashes, named new baseline.

**`recorded`** — cannot be pinned, so captured per run instead. Vulkan SDK,
driver, OS, Xcode. These are a valid explanation for a **timing** change and
never for a **correctness** change. A correctness failure that coincides with
a driver update is still a correctness failure until proven otherwise.

An entry whose `version` is `null` is **UNPINNED**:

- `xmake ci-pins` fails. This is the gate behaviour, and B0.4 is `required`.
- `xmake ci-pins --allow-unpinned` reports it and passes. The push tier uses
  this so the branch is not red for the whole of R0 while a dependency is
  still being chosen.

A pin that *disagrees* with the machine fails in both modes, always.

Three modes, because "the pin is wrong" and "this machine cannot check it"
are different facts:

| Invocation | Used by | Behaviour |
| --- | --- | --- |
| `xmake ci-pins` | GPU runners, gate | Strict. Everything must verify |
| `… --tools-optional` | Cloud push tier | File pins strict; a tool-version pin whose tool is absent is `UNVERIFIABLE_HERE`, reported not failed |
| `… --allow-unpinned` | Local `ci-check` | Also downgrades an entry with no version yet |

A hosted runner has no Slang install. Failing there would say *the pin is
wrong* when the truth is *this machine cannot check it*. The file-based pins
need no tool and stay strict everywhere.

**Do not invent a version to turn the check green.** Fill in the exact tag or
commit you actually vendored.

### How each pin is proved

| Pin | Proved by |
| --- | --- |
| `slang` | Running `slangc -v` and comparing the string |
| `clusterlod_h` | sha256 of `third_party/clusterlod/clusterlod.h`. Editing the vendored file in place is a silent fork, and the hash catches it |
| `meshoptimizer` | The commit *and* its tarball sha256 appearing in `xmake/packages/m/meshoptimizer/xmake.lua`, so `pins.json` and the build cannot drift apart |

### Why meshoptimizer is pinned to a commit, not a release

`clusterlod.h` lives in the upstream `demo/` directory and tracks master, not
releases. At the pinned commit it calls `meshopt_SimplifyErrorClamped` and
`meshopt_SimplifyPreserveFolds`, and **neither symbol exists in v1.2**, the
latest release at the time of pinning. Pairing `clusterlod.h` with a release
it predates does not link.

Both are pinned to the **same commit** and are bumped **together**. A bump of
either is a content format change: full re-cook, new package hashes, named new
baseline.

---

## Thermal and clock measurement on macOS

[architecture.md §2](../doc/architecture.md) requires sustained clocks and
thermal state per run. On macOS the only source is `powermetrics`, which
refuses to run as anyone but root.

`ci-machine` tries two paths that need no human at a keyboard, in order:

1. **direct** — the process is already root, or an equivalent arrangement
   exists (a setuid wrapper, a launchd helper).
2. **`sudo -n`** — which **never prompts**. A `NOPASSWD` rule makes this
   succeed; without one it fails instantly.

`-n` is what makes this safe unattended. An interactive `sudo` would block a
nightly job forever on a password prompt, and **a CI job that hangs is worse
than one that reports a gap.**

If neither path works, every powermetrics-derived field is `UNAVAILABLE` with
the reason, and **every other field is still captured**. The run is not
refused. The manifest records `fields.power_access`, so two runs obtained by
different means are never silently compared as though they were alike.

**This is optional.** Anyone running the benchmarks — and people using the
engine will — gets a complete manifest without it, minus the thermal fields.

### Enabling it on a benchmark machine

On the qualification machines it is worth enabling, because sustained clocks
are how a thermally throttled run is told apart from a slow one. Create the
rule with `visudo`, never by editing the file directly:

```bash
sudo visudo -f /etc/sudoers.d/gamegine-powermetrics
```

```
# Let this user sample power and thermal state unattended, for benchmark runs.
your-username ALL=(root) NOPASSWD: /usr/bin/powermetrics
```

Then confirm, and expect `power_access` to read `sudo-nopasswd`:

```bash
xmake ci-machine | head
```

This is a real privilege grant: `powermetrics` running as root observes
system-wide activity. It suits a dedicated benchmark machine. Do not add it to
a shared or untrusted one, and note that [ci.md §9](../doc/ci.md) already
forbids self-hosted runners from executing untrusted pull-request code.

### What is measured, and what is not

| Field | On Apple silicon |
| --- | --- |
| `sustained_clocks` | Every cluster's **HW active** frequency plus the GPU's. On an M3 Pro that is `E`, `P0` and `P1` — recording `P0` alone would drop half the performance cores |
| `cpu_power_mw`, `gpu_power_mw` | From the `cpu_power` and `gpu_power` samplers |
| `thermal_pressure` | The pressure level: `Nominal`, `Fair`, `Serious`, `Critical` |
| `temperatures` | **Always `UNAVAILABLE`.** Apple silicon publishes no die temperature through `powermetrics` |
| `cooling_mode` | From `pmset -g`: `lowpowermode`, and `highpowermode` on Pro and Max notebooks. Both change results |

A thermal *pressure level* is not a temperature. Recording one in a field
named `temperatures` would be a different measurement wearing the right
label, so that field stays `UNAVAILABLE` and names its substitute.

The parser is covered by `ci-test` against committed fixtures for both cluster
namings. Those tests exist because the machine that edits this code may have
no `powermetrics` access while the machine that runs the benchmarks does —
without them the parser can stop matching after an OS update, and the only
symptom is a manifest quietly missing its clocks.

---

## Machine roles

`machines.json` records which machine is which. `ci.md §2` says the two
development laptops are the CI machines; in practice there is also a box that
is neither, where runs are mock.

| Role | Meaning |
| --- | --- |
| `qualification` | The hardware in [architecture.md §2](../doc/architecture.md). Results are gate evidence |
| `development` | Any other box. Runs exercise the harness; they never substantiate a claim |

An **unrecognised machine defaults to `development`**. Defaulting the other
way would let an unknown box quietly produce evidence.

Every run manifest carries `machine_role` and `evidence_eligible`, and
`ci-bundle` enforces two rules that follow:

- A **T3 gate bundle from a development machine is rejected.** Gate bundles
  are the evidence behind published claims and they outlive the machines that
  produced them.
- `evidence_eligible: true` alongside `machine_role: development` is a
  contradiction, and an error.

Register a new machine by adding an entry to `machines.json`; matching is on
host, reported GPU name and hardware model, so it does not depend on a
hostname that changes.

---

## Status vocabulary

From [ci.md §4](../doc/ci.md). A run reports one of these per row, per
backend:

| Status | Meaning |
| --- | --- |
| `PASS` / `FAIL` | Ran and compared |
| `NOT_IMPLEMENTED` | The backend's feature set does not include this yet. Expected, and **dated** — it carries the release by which it must clear |
| `UNAVAILABLE` | Runner asleep, busy, or unreachable. **Not a pass** |
| `NOT_RUN` | Not scheduled in this tier |

`bundle.lua` enforces two of these rules directly: an `UNAVAILABLE` or
`NOT_IMPLEMENTED` manifest without a `status_reason` is an error, and a
`NOT_IMPLEMENTED` manifest without `not_implemented_clears_by` is an error.

---

## The frozen schema

`schema/run-manifest.schema.json` is transcribed from
[benchmarks.md §6](../doc/benchmarks.md), which stays authoritative. It is a
contract **frozen at R0** (r0-architecture.md §5): every later result is
compared against R0's, so the schema has to survive.

Adding a field is compatible. Renaming or removing one is not, and it
invalidates comparison against every archived bundle.

`schema/examples/` is committed and validated on every push for exactly that
reason. If you edit the schema incompatibly, those fixtures fail immediately
rather than at a release boundary.

---

## Build-out state

[ci.md §11](../doc/ci.md) budgets roughly 80 team-hours across R0. Current
position, as reported by `xmake ci-inventory`:

| Step | When | Delivers | State |
| --- | --- | --- | --- |
| 1 | R0, first | Self-hosted runners on both laptops; overnight schedule; cancel-on-developer-login | **pending** |
| 2 | R0 | Push tier: host build, Slang both targets, capability check, CPU fixtures | **partial** — format, schema and pins are live; build and shader jobs are wired but have no inputs yet |
| 3 | R0 | Golden-image harness, one trivial fixture, green on both machines | **pending** — *this is the R0 exit criterion* |
| 4–7 | R1–R4 | Cooker determinism, cross-backend agreement, streaming fixtures, release archives | not started |

Jobs in step 2 that have no inputs print a GitHub notice naming what will make
them real. They do not skip silently.

---

## What CI does not do

Restating [ci.md §10](../doc/ci.md), because each of these is a
plausible-sounding project that would consume weeks: no distributed build
farm, no cloud GPU testing, no automatic performance gating, no custom
dashboard framework, no auto-updating golden references, and no cooking of the
production corpus in CI.
