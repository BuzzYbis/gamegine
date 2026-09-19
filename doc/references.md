# Sources and references

Companion files: [Architecture](architecture.md) · [Roadmap](roadmap.md) · [Benchmarks](benchmarks.md) · [CI](ci.md) · [Math](math.typ).

**On links.** URLs are given only where they have been checked. The §12 entries are cited without URLs for that reason — each is unambiguous by title and author, and a guessed link is worse than none. Everything else is a full bibliographic citation, which is stable and searchable in a way that a guessed URL is not. Where a paper has an obvious canonical home, it is noted in the entry rather than linked speculatively.

**On use.** The reading order in §11 places every source at the point it becomes load-bearing, with a depth — read, skim, consult, or open only on a named symptom. Entries marked **[core]** are the ones the design actually rests on; the rest is context and depth.

---

## 1. The primary reference

**[core] Karis, B., Stubbe, R., Wihlidal, G.** *Nanite: A Deep Dive.* SIGGRAPH 2021 Advances in Real-Time Rendering.
→ https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf

The single document this project is a response to. Page references throughout the design files are one-based PDF pages including title and notes pages. What matters where:

| Pages | Subject |
| --- | --- |
| 15–19 | GPU-driven scene representation, the persistent scene model |
| 31–33 | Why a locked-boundary tree stops simplifying |
| 45–48 | Regrouping: group, merge, simplify, split, regroup at the next level |
| 63–70 | Local LOD decisions, the parent/child error predicate, group coherence |
| 69–74 | The runtime hierarchy as a structure separate from the build DAG |
| 75–77 | Two-pass occlusion, previous-frame HZB, previous versus current transforms |
| 79–93 | Small-triangle rasterization, the software path, 64-bit visibility atomics |
| 122–127 | Valid resident cuts, split-group activation, dependency-aware requests |
| 129–135 | Disk representation versus GPU representation, quantization, attributes |

It describes 2021 Nanite. Later Unreal versions added capabilities (constrained masked and deformed materials among them) that this document does not cover; for those, cite versioned Epic documentation rather than the talk.

**Epic Games.** *Nanite Virtualized Geometry* documentation, UE 5.2.
→ https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine?application_version=5.2

Version-pinned deliberately. Nanite's supported feature set moves between releases and an unversioned link makes a comparison unreproducible a year later.

---

## 2. Mesh simplification

**[core] Garland, M., Heckbert, P.** *Surface Simplification Using Quadric Error Metrics.* SIGGRAPH 1997.

The origin of the quadric error metric. Everything in [math §7](math.typ) traces here: the fundamental quadric as a plane outer product, additivity under contraction, the linear system for the optimal contraction position. Short, readable, and still the clearest statement of the idea.

**Garland, M., Heckbert, P.** *Simplifying Surfaces with Color and Texture using Quadric Error Metrics.* IEEE Visualization 1998.

The first attribute-aware extension, lifting the quadric into a higher-dimensional space. Read for the idea; Hoppe's version below is the one implementations follow.

**Hoppe, H.** *New Quadric Metric for Simplifying Meshes with Appearance Attributes.* IEEE Visualization 1999.

The formulation that handles attribute discontinuities properly and is what modern simplifiers implement. Relevant to the UV-seam problem: it explains why attribute discontinuities fragment the mesh into components the simplifier cannot merge across.

**Hoppe, H.** *Progressive Meshes.* SIGGRAPH 1996.

The edge-collapse / vertex-split formulation of continuous LOD. Predates the cluster approach and does not survive contact with GPUs at scale, but it is the conceptual parent and worth reading to understand what the cluster granularity is buying.

**Cohen, J., Varshney, A., Manocha, D., et al.** *Simplification Envelopes.* SIGGRAPH 1996.

Simplification with a guaranteed geometric error bound, which is the property the runtime cut predicate needs and QEM does not provide. Read alongside the Hausdorff material in [math §8](math.typ).

**Aspert, N., Santa-Cruz, D., Ebrahimi, T.** *MESH: Measuring Errors between Surfaces using the Hausdorff Distance.* IEEE International Conference on Multimedia and Expo, 2002.

The reference method for two-sided Hausdorff distance between triangle meshes by error-bounded sampling, with sample density tied to face area and bounding-box diameter. This is what [math §8](math.typ) needs to turn "dense sampling plus a margin" into a construction with a stated guarantee, and the distinction matters because sampling gives a *lower* bound.

**Lindstrom, P., Turk, G.** *Fast and Memory Efficient Polygonal Simplification.* IEEE Visualization 1998.

Memory-bounded simplification. Directly relevant to cooking assets far larger than RAM.

---

## 3. Cluster hierarchies and out-of-core LOD

Nanite did not appear from nothing. This lineage explains most of its design decisions.

**[core] Cignoni, P., Ganovelli, F., Gobbetti, E., Marton, F., Ponchio, F., Scopigno, R.** *Adaptive TetraPuzzles: Efficient Out-of-Core Construction and Visualization of Gigantic Multiresolution Polygonal Models.* SIGGRAPH 2004.

The closest ancestor to Nanite's cluster DAG. Groups of triangles simplified together with locked boundaries, arranged so adjacent regions can be drawn at different levels without cracks. If you read one paper from this section, read this one — it makes clear which parts of Nanite are new and which are seventeen years old.

**Cignoni, P., et al.** *BDAM — Batched Dynamic Adaptive Meshes for High Performance Terrain Visualization.* Eurographics 2003.
**Cignoni, P., et al.** *Batched Multi Triangulation.* IEEE Visualization 2005.

The "batch of triangles as the unit of LOD" idea, which is what makes any of this GPU-friendly. The multi-triangulation framing is the general theory of which the cluster DAG is an instance.

**Yoon, S., Salomon, B., Gayle, R., Manocha, D.** *Quick-VDR: Interactive View-Dependent Rendering of Massive Models.* IEEE Visualization 2004.

View-dependent rendering with clustered hierarchies and out-of-core management.

**Gobbetti, E., Marton, F.** *Far Voxels: A Multiresolution Framework for Interactive Rendering of Huge Complex 3D Models on Commodity Graphics Platforms.* SIGGRAPH 2005.

The other answer to the same problem — switch representation instead of simplifying. Useful as a contrast: it explains what aggregate geometry costs a triangle-based approach, which is exactly the foliage weakness.

**Luebke, D., Reddy, M., Cohen, J., Varshney, A., Watson, B., Huebner, R.** *Level of Detail for 3D Graphics.* Morgan Kaufmann, 2002.

The textbook. Dated on hardware, not dated on principles. The chapters on error metrics and view-dependent simplification are the background the papers above assume.

---

## 4. Graph partitioning

**[core] Karypis, G., Kumar, V.** *A Fast and High Quality Multilevel Scheme for Partitioning Irregular Graphs.* SIAM Journal on Scientific Computing, 1998.

The multilevel coarsen–partition–refine scheme behind METIS, and behind every practical cluster grouping implementation. Explains why the problem is tractable in practice despite being NP-hard.

**Kernighan, B., Lin, S.** *An Efficient Heuristic Procedure for Partitioning Graphs.* Bell System Technical Journal, 1970.
**Fiduccia, C., Mattheyses, R.** *A Linear-Time Heuristic for Improving Network Partitions.* Design Automation Conference, 1982.

The local refinement heuristics used at each uncoarsening level. Fiduccia–Mattheyses is the one implementations use, for its bucket data structure.

Note on tooling: `meshopt_partitionClusters` removes the need for an external partitioner in this pipeline, which also removes METIS's licensing from the question. The theory is still worth knowing, because partition quality directly determines how much the simplifier can reduce.

---

## 5. Rasterization, visibility and GPU-driven pipelines

**[core] Burns, C., Hunt, W.** *The Visibility Buffer: A Cache-Friendly Approach to Deferred Shading.* Journal of Computer Graphics Techniques (JCGT), 2013.

The origin of the visibility-buffer approach: store triangle identity per pixel, shade once afterwards. This is why the router in the compute-rasterizer design costs nothing extra in shading — both paths write the same record.

**[core] Haar, U., Aaltonen, S.** *GPU-Driven Rendering Pipelines.* SIGGRAPH 2015 Advances in Real-Time Rendering.

Cluster culling, indirect draw, and moving submission onto the GPU. The practical foundation under the selection and culling design.

**Wihlidal, G.** *Optimizing the Graphics Pipeline with Compute.* GDC 2016.

Triangle-level culling in compute ahead of the hardware rasterizer. Relevant background to the classification question: it is the intermediate position between "hardware only" and "software rasterizer".

**Olano, M., Greer, T.** *Triangle Scan Conversion Using 2D Homogeneous Coordinates.* Graphics Hardware 1997.

Rasterization in homogeneous coordinates, avoiding the perspective divide and its clipping special cases. Worth reading before writing any compute rasterizer.

**Abrash, M.** *Rasterization on Larrabee.* Dr. Dobb's / Intel, 2009.

The most practical account of software rasterization on wide SIMD: edge functions, tiling, fixed point, fill rules. Larrabee did not ship; the article remains the best explanation of the technique.

**Pineda, J.** *A Parallel Algorithm for Polygon Rasterization.* SIGGRAPH 1988.

The edge-function formulation itself. Two pages, and the basis for [math §10](math.typ).

**Mara, M., McGuire, M.** *2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere.* JCGT, 2013.

The closed form for a conservative screen-space bound of a projected sphere, with the near-plane and eye-inside cases handled. Use it rather than deriving it — the sign cases are where the bugs live, and an under-covering bound produces visible holes through Hi-Z.

**[core] Blelloch, G. E.** *Prefix Sums and Their Applications.* Technical Report CMU-CS-90-190, Carnegie Mellon University, 1990.

Work-efficient parallel scan and stream compaction. Cited by [math §16](math.typ) and load-bearing for R2: indirect draw offset allocation, compaction of visible cluster candidates, and workgroup-level queue reservation without serializing. The bounded-queue design in the traversal depends on getting reservation right, and this is where the reasoning lives.

**Kubisch, C.** NVIDIA developer articles on mesh shaders and meshlet culling.
→ https://developer.nvidia.com/blog (search for mesh shaders, meshlets)

Practical guidance on cluster sizes, per-primitive culling, and what the hardware actually rewards.

---

## 6. Depth, precision and antialiasing

**[core] Reed, N.** *Depth Precision Visualized.* 2015.

The clearest explanation of why reversed Z with a float depth buffer is effectively free precision — the interaction between hyperbolic depth distribution and floating-point density. Read before implementing anything depth-related.

**Upchurch, P., Desbrun, M.** *Tightening the Precision of Perspective Rendering.* JCGT, 2012.

The formal treatment of the same question, including the infinite far plane.

**Welzl, E.** *Smallest Enclosing Disks (Balls and Ellipsoids).* New Results and New Trends in Computer Science, LNCS 555, Springer, 1991.

The expected linear-time randomized algorithm for the exact minimal enclosing sphere, resting on the fact that at most four boundary points determine it in three dimensions. Cited by [math §6](math.typ). A loose sphere is not a correctness bug but it lowers the conservative nearest depth, inflating projected error and forcing refinement that buys nothing.

**Ritter, J.** *An Efficient Bounding Sphere.* Graphics Gems, Academic Press, 1990.

The two-pass linear approximation, typically 5–20% larger than minimal. Worth reading beside Welzl to decide where the cooker spends its time: this is fast enough for import, and Welzl is what the cluster bounds should end up using.

**Karis, B.** *High Quality Temporal Supersampling.* SIGGRAPH 2014 Advances in Real-Time Rendering.

The reference for practical TAA: neighbourhood clamping, history rejection, jitter sequences. Required reading, because at a 1-pixel LOD error target the renderer is not viewable without temporal integration, and the reference design assumes the LOD switch is hidden by it.

**Akenine-Möller, T., Haines, E., Hoffman, N., Pesce, A., Iwanicki, M., Hillaire, S.** *Real-Time Rendering, 4th edition.* CRC Press, 2018.

The general reference. Chapters on culling, LOD, sampling and antialiasing cover most of the background this project assumes.

**Pharr, M., Jakob, W., Humphreys, G.** *Physically Based Rendering: From Theory to Implementation, 4th edition.* MIT Press, 2023.
→ https://pbr-book.org/ — full contents of the third and fourth editions, free
→ https://github.com/mmp/pbrt-v4 — the accompanying renderer source

The authority on sampling, filtering and texture derivatives. Two chapters earn their place here specifically: geometry and transformations (§2, for the treatment of normals as covectors and of bounding volumes), and texture filtering, which is the right background for computing analytic UV gradients in a compute resolve where no quad exists to difference against.

---

## 7. Streaming and virtual memory for graphics

**Barrett, S.** *Sparse Virtual Textures.* GDC 2008.
**Mittring, M.** *Advanced Virtual Texture Topics.* SIGGRAPH 2008 course.
**van Waveren, J.M.P.** *Software Virtual Textures.* id Software, 2012.

Virtual texturing is the direct ancestor of virtualized geometry's page system: feedback buffers, page tables, residency, fallback to coarser levels. The problems are the same and the solutions transfer almost directly. Read at least one before designing the page table.

**Little, J.D.C.** *A Proof for the Queuing Formula L = λW.* Operations Research, 1961.

Two pages. It is why injected latency shows up as in-flight depth rather than reduced throughput, which is the shape the throttled-I/O scenario produces and the thing that makes staging-ring sizing predictable. See [math §14](math.typ).

---

## 8. Implementations to read and measure against

This is the section that has changed most since the 2021 talk, and the reason several decisions in the architecture are integration rather than authorship.

### [core] meshoptimizer

→ https://github.com/zeux/meshoptimizer · https://meshoptimizer.org · MIT

The geometry processing library this project depends on. Version 1.0 stabilized the cluster and simplification APIs. What is used and why:

| Function | Role |
| --- | --- |
| `meshopt_buildMeshlets`, `meshopt_buildMeshletsFlex` | Rasterization-oriented clusterization |
| `meshopt_buildMeshletsSpatial` | Spatial-quality clusterization, surface-area heuristic |
| `meshopt_partitionClusters` | Cluster grouping — replaces an external graph partitioner |
| `meshopt_simplifyWithAttributes` + `vertex_lock` | Attribute-aware simplification with per-vertex locking, which is how group boundaries stay bit-identical |
| `meshopt_SimplifySparse` | Large speedup when simplifying small subsets of a big mesh |
| `meshopt_SimplifyErrorAbsolute` | Absolute rather than relative error, required for consistency across subsets |
| `meshopt_optimizeMeshlet` | Per-cluster triangle and vertex ordering for rasterization locality |
| `meshopt_spatialSortTriangles` | Morton ordering, improves clusterization cache behaviour |
| Vertex and index codecs | Fast-decode compression designed for engine-ready formats |

Release notes for 1.0: → https://meshoptimizer.org/v1.html

### [core] clusterlod.h

→ https://github.com/zeux/meshoptimizer/blob/master/demo/clusterlod.h · MIT, single header

The full cluster-group DAG build — the group, merge, simplify, split, regroup loop. Maintained alongside meshoptimizer and used in production by NVIDIA's sample. Its API is explicitly evolving, which is why the version is pinned and a bump is treated as a content-format change.

This project integrates it rather than reimplementing the loop, and additionally uses it as a **differential oracle**: when the cut validator disagrees with reality on a fixture, diffing our group output against its group output localizes the problem immediately.

### [core] vk_lod_clusters

→ https://github.com/nvpro-samples/vk_lod_clusters · Apache-2.0

NVIDIA's end-to-end Vulkan cluster-LOD renderer: streaming from RAM to VRAM, mesh-shader rasterization, ray tracing via cluster acceleration structures, an optional compute rasterizer, and a GPU-driven allocator. Built on `clusterlod.h`.

Three distinct uses here:

1. **Correctness oracle** — a working implementation of the same technique to compare against.
2. **Performance reference on identical hardware** — run it on Profile L, in the same session, and you have an external number that is actually comparable, which is exactly what an Unreal comparison is not.
3. **Documented negative result** — its README reports that the compute rasterizer "in typical usage scenarios is not faster than the mesh-shader because clusters tend to have larger than single pixel triangles". That observation is why the compute rasterizer here is a measured experiment rather than a required capability.

Worth reading in the repository: `docs/lod_generation.md`, `docs/streaming.md`, `docs/clas_allocation.md`.

### Kapoulkine, A. — *Billions of triangles in minutes*

→ https://zeux.io/2025/09/30/billions-of-triangles-in-minutes/

Processing a 1.64 G-triangle scene into a cluster DAG, with the optimization work written up. The source of the cooker budget anchors in [architecture.md D4](architecture.md), and of three implementation requirements worth adopting rather than rediscovering: sort meshes by triangle count descending before dispatching to a thread pool, cap in-flight triangles with a counting semaphore, and use per-thread allocation arenas.

Also useful as a worked example of profiling methodology — the gap between what the profiler reported and what actually reduced wall time is instructive.

### nv_cluster_builder

→ https://github.com/nvpro-samples/nv_cluster_builder

NVIDIA's raytracing-aware clusterization library. A different implementation of ideas related to `meshopt_buildMeshletsSpatial`; worth comparing if cluster quality becomes a question.

### niagara — Kapoulkine, A.

A public live-coded Vulkan renderer covering mesh shaders, cluster culling and GPU-driven submission from first principles. Useful as a worked implementation of the parts this project writes rather than integrates. Available as a repository and a recorded stream series; search by name.

### Bevy virtual geometry — jms55

→ https://jms55.github.io/posts/2024-06-09-virtual-geometry-bevy-0-14/
→ https://jms55.github.io/posts/2024-11-14-virtual-geometry-bevy-0-15/

A from-scratch implementation written up honestly, including what did not work. The closest published thing to this project's own situation — a small team implementing the technique without Epic's resources — and therefore the most directly useful account of where the time goes.

---

## 9. Tooling and platform documentation

### Shader toolchain

**Slang** → https://github.com/shader-slang/slang · MIT
Single-source shaders targeting SPIR-V and MSL, shipped in the Vulkan SDK. The capability system is the mechanism that rejects unsupported feature use at type-check time rather than at runtime on one device.

Target compatibility notes, including the MSL 64-bit atomic limitation and MoltenVK's `shaderBufferInt64Atomics = false` on Apple Silicon:
→ https://github.com/shader-slang/slang/blob/master/docs/target-compatibility.md

### Graphics APIs

**Vulkan 1.4 feature proposal** → https://docs.vulkan.org/features/latest/features/proposals/VK_VERSION_1_4.html
**VK_EXT_mesh_shader** → https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_mesh_shader.html
**VkPhysicalDeviceShaderAtomicInt64Features** → https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceShaderAtomicInt64Features.html
**Metal Feature Set Tables** → https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf — read the footnotes; the 64-bit atomic min/max exceptions live there
**Metal tools overview** → https://developer.apple.com/metal/

### Hardware specifications

**NVIDIA RTX 40-series laptop GPUs** → https://www.nvidia.com/en-us/geforce/laptops/40-series/
**Apple Mac specifications** → https://support.apple.com/en-us/117736

Both are starting points only. The machine manifest records device-reported values, because a laptop GPU's behaviour depends on the power limit, cooling mode and display path far more than on the model name.

### Profiling and test infrastructure

| Tool | Use |
| --- | --- |
| RenderDoc | Frame capture on Vulkan; the first thing to reach for on a visual bug |
| NVIDIA Nsight Graphics / Systems | GPU timing and pipeline analysis on Profile L |
| Xcode GPU capture, Metal counters, Instruments | The equivalents on Profile A, including memory pressure and swap |
| Superluminal → https://superluminal.eu | CPU sampling profiler; used in the cooker work cited above |
| Tracy | Frame-level instrumentation with low overhead; useful for the CPU-active-frame measurement |
| GoogleTest, Google Benchmark | Correctness fixtures and the timing harness |
| Vulkan validation layers, Metal API and shader validation | Run in CI, disabled in timing runs, state recorded |

---

## 10. Test content

**Zorah geometry export** (`zorah_main_public.v2`) — **the committed geometry corpus**, used at two densities: a hashed ≈ 320 M-triangle subset carrying the demo and the streaming gates, and the full 1.63 G scene as the 48:1 measurement. **Positions and normals only.** The asset is MIT-licensed; verify the terms before relying on redistribution, and note the published version has vegetation removed to make sharing possible. Budget ≈ 120 GiB of disk once source, upstream render cache and our own output are counted — measure it rather than carrying the estimate. Cook under a measured memory admission limit, not a thread count.

**Zorah textured export** (`zorah_textured_public.v1`) — **not used, and the reason is worth recording.** It carries tangents and texture coordinates, which the geometry export lacks, but it is a 70 GB download, extracts to 31 GB of mesh plus 48 GB of textures, needs up to 64 GB of RAM to bake against Profile L's 32 GB, and loads its 4,357 textures fully into VRAM against a 2 GiB budget. Hence the corpus split in D8: geometry from Zorah, materials from a separate small UV- and tangent-equipped fixture.
→ https://developer.download.nvidia.com/ProGraphics/nvpro-samples/zorah_main_public.v2.gltf.7z
→ Pre-built cache: https://developer.download.nvidia.com/ProGraphics/nvpro-samples/zorah_main_public.v2.gltf.nvsngeo.7z

**threedscans-derived scenes** — 7–8 M triangles each, clean closed surfaces, distributed alongside the sample.
→ https://developer.download.nvidia.com/ProGraphics/nvpro-samples/threedscans_statues.zip
→ https://developer.download.nvidia.com/ProGraphics/nvpro-samples/threedscans_animals.zip
→ Original collection: https://threedscans.com/

**Stanford 3D Scanning Repository** and the **Smithsonian 3D collection** — additional public scan sources, useful for adversarial fixtures and for the seam and thin-geometry corpus that Zorah's architectural content does not exercise. Check each asset's terms individually; "publicly downloadable" and "redistributable" are not the same thing.

**McGuire Computer Graphics Archive** — a curated set of test scenes with documented provenance, standard in graphics research.

Licence discipline: every asset's licence text is read and archived in the asset manifest before R1 closes. This is on the critical path because borrowing is the committed content strategy, not a fallback.

---

## 12. Shading, shadows, simulation and statistics

These are the sources [math.typ](math.typ) cites from §16 onward. They were missing while the mathematics referred to them, which is the kind of gap that only shows up when someone checks the citation rather than the claim.

### Microfacet shading

**[core] Walter, B., Marschner, S. R., Li, H., Torrance, K. E.** *Microfacet Models for Refraction through Rough Surfaces.* Eurographics Symposium on Rendering, 2007.

The derivation of the GGX / Trowbridge-Reitz normal distribution and its shadowing-masking terms. The source for the microfacet distribution in [math §19](math.typ), and therefore for gate B3T.4a's energy-conservation fixture, which currently has a tolerance and no citation behind it.

**[core] Heitz, E.** *Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs.* Journal of Computer Graphics Techniques, 2014.

Why separable Smith masking overestimates attenuation at grazing angles, and the height-correlated form that fixes it. Read with Walter; the pair is what makes an energy-conservation test meaningful rather than a number someone picked.

**Lagarde, S., de Rousiers, C.** *Moving Frostbite to Physically Based Rendering 3.0.* SIGGRAPH 2014 Courses.

The practical counterpart: energy-conserving Cook-Torrance, analytic light integrals, Schlick Fresnel, and the remapping conventions a real implementation needs.

**Google Filament documentation.** *Physically Based Rendering in Filament.*

Perceptual roughness remapping, split-sum environment approximation for IBL, and the parameterisation choices a material pipeline has to commit to. Cited by [math §19](math.typ).

**Mikkelsen, M.** *Simulation of Wrinkled Surfaces Revisited.* University of Copenhagen, 2008. Canonical implementation: MikkTSpace.

The tangent-space generation glTF 2.0 specifies. **This is on the critical path for D8's corpus split**: the geometry corpus carries no tangents, so the separately sourced material fixture must either ship MikkTSpace-compatible tangents or have them generated to this specification, including the sign parity convention across mirrored instances.

### Shadows

**Engel, W.** *Cascaded Shadow Maps.* ShaderX5: Advanced Rendering Techniques, Charles River Media, 2006.

The split-scheme formulation for cascade selection across the frustum. The source for the cascade boundary arithmetic in [math §19](math.typ) and for the texel-space error metric the shadow views select against.

**Eisemann, E., Schwarz, M., Assarsson, U., Wimmer, M.** *Real-Time Shadows.* CRC Press, 2011.

Bias tuning, light-space texel snapping against edge shimmer, and receiver clamping. Overlaps Real-Time Rendering; open it when cascade stability misbehaves rather than reading it through.

**Hable, J.** *Analytic texture gradients with shading derivatives.* 2013.

Analytic UV gradients from screen-space barycentric differentials, without hardware quad finite-differencing. Cited by [math §18](math.typ) and required for the visibility-buffer resolve, where neighbouring pixels routinely belong to unrelated primitives and implicit derivatives are silently wrong.

### Simulation

**Fiedler, G.** *Fix Your Timestep!* Gaffer on Games, 2004.

Accumulator-based fixed-timestep simulation with interpolated presentation. Cited by [math §20](math.typ) and the basis of the deterministic replay harness, which every benchmark route depends on reproducing frame for frame.

**Amdahl, G. M.** *Validity of the Single Processor Approach to Achieving Large Scale Computing Capabilities.* AFIPS Conference Proceedings, 1967.

Two pages, and it makes D6's decision arithmetic rather than editorial. With *p* the fraction of baseline visibility time a compute rasterizer could accelerate, frame benefit is bounded by `1/(1−p)`: a 2× frame speedup needs **p > 0.50 even at infinite inner speedup**, and **p > 0.75 at a realistic 3×**. Cited by [math §21](math.typ).

### Statistics

**Clopper, C. J., Pearson, E. S.** *The Use of Confidence or Fiducial Limits Illustrated in the Case of the Binomial.* Biometrika, 1934.

The exact binomial interval. The source of the 86.1% figure quoted in the streaming gate — twenty successes from twenty trials establish only that one-sided 95% lower bound — and of the sample size a genuine 95% claim would need: `n ≥ ln(0.05)/ln(0.95)`, so **59 consecutive successes**, not twenty.

**Brown, L. D., Cai, T. T., DasGupta, A.** *Interval Estimation for a Binomial Proportion.* Statistical Science, 2001.

Why Wald intervals fail at small *n* and zero observed failures, and what Wilson and Clopper-Pearson do instead. Open it if anyone proposes a normal approximation for a 20-trial result.

**NIST/SEMATECH.** *e-Handbook of Statistical Methods.*

Quantile estimation conventions — nearest-rank against linear interpolation — and median absolute deviation for run-level dispersion. Cited by [math §21](math.typ), and it settles the question of *which* p95 the protocol means.

---

## 11. Reading order

Seventy-odd sources is not a reading list, it is a library. This section places **every one of them** at the point where it becomes load-bearing, so nothing is read speculatively and nothing is discovered too late.

Four depths, and the distinction matters more than the order:

| Depth | Meaning |
| --- | --- |
| **Read** | Cover to cover, once, with the intent of retaining the argument |
| **Skim** | Enough to know what is in it and where to look later |
| **Consult** | Never read through. Opened at a specific question and closed again |
| **On trouble** | Do not open until the named symptom appears. Opening it early costs a session and teaches nothing |

Most of the list is *consult* or *on trouble*. That is the point.

---

### Before R0 — build a map, not mastery

| Source | Depth | Why now |
| --- | --- | --- |
| Karis et al. 2021, end to end | **Read** once, without trying to retain details | Every design decision in this project is a response to it. You need the shape, not the specifics |
| Reed, *Depth Precision Visualized* | **Read** | Reversed Z touches the projection matrix, the HZB reduction direction and the visibility key. Getting it wrong at R0 is expensive at R3 |
| `vk_lod_clusters` documentation directory | **Skim** | `lod_generation.md`, `streaming.md`, `clas_allocation.md`. A working implementation's own account of its structure |
| Luebke et al., *Level of Detail for 3D Graphics* | **Skim** the error-metric and view-dependent chapters | The background every paper below assumes. Dated on hardware, not on principles |

### During R0 — toolchain, measurement, harness

| Source | Depth | Why now |
| --- | --- | --- |
| Slang target-compatibility notes | **Read** | D3 rests on it, and the 64-bit atomic question is answered here before it is answered on the device |
| Slang repository docs | **Consult** | Capability profiles and build integration, as the toolchain goes up |
| Vulkan 1.4 feature proposal | **Consult** | Only the features actually being probed |
| `VK_EXT_mesh_shader` | **Consult** | Dispatch throughput microbenchmark |
| `VkPhysicalDeviceShaderAtomicInt64Features` | **Consult** | The 64-bit atomic probe |
| Metal Feature Set Tables | **Consult**, and read the footnotes | The Apple8/Apple9 atomic min/max exceptions live in the footnotes, not the table |
| Metal tools overview | **Consult** | Counter sampling for the A-profile timing path |
| NVIDIA RTX 40-series specification | **Consult** once | A starting point only; the manifest records device-reported values |
| Apple Mac specification | **Consult** once | As above |
| `vk_lod_clusters` itself | **Build and run** | Reading it is worth less than running it on Profile L and recording the numbers |
| RenderDoc | **Learn** | First reach on any visual bug, from R0 onward |
| NVIDIA Nsight Graphics / Systems | **Learn** | The L-profile timing path |
| Xcode GPU capture, Metal counters, Instruments | **Learn** | The A-profile equivalents, including memory pressure and swap |
| Superluminal | **Learn** | CPU sampling; the cooker work is profiled with it |
| Tracy | **Learn** | Frame-level instrumentation for the CPU-active-frame measurement |
| GoogleTest, Google Benchmark | **Learn** | Correctness fixtures and the timing harness |
| Vulkan validation layers, Metal API and shader validation | **Learn** | Wired into CI at R0, disabled in timing runs, state recorded |

### Before R1 — the cooker and leaf clusters

| Source | Depth | Why now |
| --- | --- | --- |
| Garland & Heckbert 1997 | **Read** properly | The quadric error metric. Short, and everything downstream assumes it |
| meshoptimizer reference manual | **Read** the clusterization and simplification sections; **consult** the rest | The library you are building on. The codecs section waits until quantization lands |
| Kapoulkine, *Billions of triangles in minutes* | **Read** before writing the cooker's threading | Descending-size scheduling, the in-flight triangle cap and per-thread arenas come from here, and so do the budget anchors |
| Zorah geometry export | **Obtain**, read the licence, archive it | The committed geometry corpus. Licence audit is on the critical path and closes at R1 |
| Welzl 1991; Ritter 1990 | **Read** both, briefly | Cluster bounding spheres are computed here. A loose sphere lowers the conservative nearest depth and forces refinement that buys nothing |
| Mikkelsen 2008 (MikkTSpace) | **Consult** | The material fixture must carry glTF-conformant tangents, including sign parity across mirrored instances. D8's corpus split depends on it |
| Fiedler, *Fix Your Timestep!* | **Read**, one article | The playable shell and the deterministic replay every benchmark route depends on |
| threedscans-derived scenes | **Obtain** | Hero assets and the S1 fixture |
| Stanford 3D Scanning Repository, Smithsonian 3D | **Obtain** selectively | Seam and thin-geometry fixtures that Zorah's architectural content does not provide. Check terms per asset |

### During R1 — on trouble

| Source | Symptom that opens it |
| --- | --- |
| Lindstrom & Turk 1998 | Cooker memory becomes the binding constraint before the thread cap does |
| meshoptimizer codec sections | Encoding falls short of 16 B per source triangle |

### Before R2 — the DAG and GPU selection

| Source | Depth | Why now |
| --- | --- | --- |
| Karis et al. pp. 31–74 | **Read** carefully, second pass | Locked-boundary failure, regrouping, the local error predicate, and the runtime hierarchy as a structure separate from the build DAG |
| Cignoni et al. 2004, *Adaptive TetraPuzzles* | **Read** | The closest ancestor. Makes clear which parts of Nanite are new and which are seventeen years old — directly useful when deciding what to integrate versus write |
| `clusterlod.h` source | **Read** | You are integrating it and using it as a differential oracle. Both require knowing what it actually produces |
| Bevy virtual geometry, both jms55 posts | **Read** | The closest published account of a small team implementing this, including what did not work |
| Haar & Aaltonen 2015 | **Read** | Cluster culling, indirect draw, submission on the GPU. The foundation under R2's selection path |
| Kubisch, NVIDIA mesh-shader articles | **Read** | Cluster sizes, per-primitive culling, and what the hardware rewards — before committing to the mesh-shader path |
| `VK_EXT_mesh_shader` | **Consult** again, in depth this time | D3b makes it the primary raster path at R2 |
| Blelloch 1990 | **Read** | Before writing compaction and queue reservation. Bounded queues that reserve incorrectly fail as lost geometry, which looks like a hierarchy bug and is not |
| Amdahl 1967 | **Read**, two pages | Before the D6 experiment, so the threshold *p* is chosen from the frame benefit it implies rather than from the shape of the histogram |

### During R2 — on trouble

| Source | Symptom that opens it |
| --- | --- |
| Hoppe 1999, *New Quadric Metric* | UV seams or hard creases collapse; shading breaks where geometry does not |
| Garland & Heckbert 1998 | Background to the above, if the attribute formulation is unclear |
| Karypis & Kumar 1998 | Partition quality limits how much the simplifier can reduce |
| Kernighan & Lin 1970; Fiduccia & Mattheyses 1982 | You end up tuning refinement rather than just calling the partitioner |
| Cohen et al. 1996, *Simplification Envelopes* | The error bound has to become conservative rather than heuristic |
| Hoppe 1996, *Progressive Meshes* | You want the argument for why cluster granularity exists at all |
| Cignoni BDAM 2003 and *Batched Multi Triangulation* 2005 | The group abstraction feels wrong and you want the general theory |
| `nv_cluster_builder` | Cluster quality becomes a question worth a second implementation's opinion |
| niagara | You want a worked implementation of the parts written here rather than integrated |

### Before R3 — occlusion, streaming and 48:1

| Source | Depth | Why now |
| --- | --- | --- |
| Karis et al. pp. 75–77 and 122–135 | **Read** carefully | Two-pass occlusion, valid resident cuts, split-group activation, disk versus GPU representation |
| Mara & McGuire 2013 | **Read** before implementing the Hi-Z footprint | The closed form with the clipping cases worked out. An under-covering footprint is a visible hole |
| Little 1961 | **Read**, two pages | Sizes the staging ring for λW rather than λ, and explains the throttled-I/O result before you measure it |
| Barrett, *Sparse Virtual Textures* | **Read** | Virtual texturing is the direct ancestor of the page system: feedback, page tables, residency, coarse fallback |
| `vk_lod_clusters` `streaming.md`, `clas_allocation.md` | **Read** properly this time | You are about to build the same thing |
| Zorah full-scene assets | **Obtain**, check free disk | ≈ 120 GiB once source, upstream cache and our output are counted — measure it, before R3 rather than during it |
| Clopper & Pearson 1934 | **Consult** | Before writing the teleport result. Twenty of twenty gives an 86.1% bound; a genuine 95% claim needs 59 |
| Aspert et al. 2002 | **Consult** | If the Hausdorff margin in the error contract needs to become a construction rather than an estimate |

### During R3 — on trouble

| Source | Symptom that opens it |
| --- | --- |
| Mittring 2008; van Waveren 2012 | Feedback or page-table design needs more depth than Barrett gives |
| Yoon et al. 2004, *Quick-VDR* | Out-of-core management patterns beyond what the page system already does |
| Gobbetti & Marton 2005, *Far Voxels* | Foliage and aggregate geometry defeat the triangle-based approach, and you want to know what the alternative costs |

### Before R4 — materials, temporal stability, qualification

| Source | Depth | Why now |
| --- | --- | --- |
| Burns & Hunt 2013, *The Visibility Buffer* | **Read** | Store identity, shade once afterwards. The reason the router costs nothing extra in shading |
| Karis 2014, *High Quality Temporal Supersampling* | **Read** | TAA is required, not optional, at a 1-pixel error target. Neighbourhood clamping and history rejection are Q6's subject matter |
| PBRT 4th ed., texture-filtering chapter | **Read** before analytic UV gradients | A compute resolve has no quad to difference against |
| Real-Time Rendering 4th ed. | **Consult** the sampling, antialiasing and shadow chapters | Cascade setup and filtering, at the point they are being written |
| Walter et al. 2007; Heitz 2014 | **Read** both | Before the PBR resolve. Separable Smith masking leaks energy at grazing angles, and B3T.4a's energy-conservation fixture is meaningless without knowing why |
| Lagarde & de Rousiers 2014; Filament documentation | **Consult** | The practical parameterisation and remapping choices the material pipeline has to commit to |
| Hable 2013 | **Read** | Before the visibility-buffer resolve. Quad finite-differencing fails across primitive boundaries, which is every pixel in this renderer |
| Engel 2006 | **Read** | During cascade implementation. Split selection and texel-snapped stability |
| NIST e-Handbook, quantile sections | **Consult** | When the protocol has to say *which* p95 it means |
| Epic, UE 5.2 Nanite documentation | **Read** before any comparison is published | Version-pinned deliberately; the supported feature set moves between releases |

### During R4 — on trouble

| Source | Symptom that opens it |
| --- | --- |
| PBRT material and reflection chapters | The PBR path disagrees with the reference and you need the model rather than the code |
| Eisemann et al. 2011 | Cascade stability misbehaves — shimmer, bias, receiver clamping |
| Brown, Cai & DasGupta 2001 | Someone proposes a normal approximation for a twenty-trial result |
| Upchurch & Desbrun 2012 | Depth precision becomes a measurable problem rather than a settled decision |

### Only if the R2 histogram says yes

The compute rasterizer is deferred by design. None of this is read unless D6 reopens.

| Source | Depth |
| --- | --- |
| Karis et al. pp. 79–93 | **Read** |
| Abrash, *Rasterization on Larrabee* | **Read**. The best practical account of software rasterization on wide SIMD |
| Pineda 1988 | **Read**. Two pages; the edge-function formulation itself |
| Olano & Greer 1997 | **Read**. Homogeneous rasterization, avoiding the perspective divide and its clipping cases |
| Wihlidal, GDC 2016 | **Read**. The intermediate position between hardware-only and a full software path |
| `vk_lod_clusters` compute-raster path | **Read**. An existing implementation, and the source of the negative result that deferred this in the first place |

### Kept open on the desk

Consulted throughout, never read through, never scheduled:

| Source | Opened for |
| --- | --- |
| meshoptimizer reference manual | Any question about a specific function's contract |
| Vulkan and Metal API references | Any question about a specific call |
| Hardware specifications | Recording manifest fields |
| McGuire Computer Graphics Archive | An additional test scene with documented provenance |
| Profiling tools | Whenever a number is surprising |

---

**One rule that makes the rest work.** When a source is opened out of order because a problem demanded it, record which problem — in the decision log, not in memory. Over four years the useful artefact is not the reading list but the map from symptom to source, and that map can only be built while the symptom is fresh.
