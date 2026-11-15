// vg_bench.cpp                                                     -*-C++-*-

//@PURPOSE: Run the B0 device probes on one GPU and report them.
//
//@DESCRIPTION: The shared half of vg_bench: everything that is not an API
// call. It loads the kernel set the shader toolchain built
// (build/shaders/index.json), runs each probe through the 'Gpu' interface of
// bench_gpu.h, checks every output on the host, and writes three CSV files
// in its output directory (results/vg_bench/<backend> by default):
//
//  run.csv           key, value: the backend, the device, and the shader
//                    hash of the set that ran
//  probes.csv        one row per probe, and one for the B0.6 timing check:
//                    its status, why, and what it measured
//  measurements.csv  every number, one per row: each timed run (and each
//                    page of page_decode's host stages), then the medians and
//                    throughputs derived from them
//
// Nothing is compiled here: the backends load the '.spv' or '.metallib' the
// toolchain produced. Buffers are bound by the names the Slang source gives
// them, resolved to indices from the index, so a reordered declaration
// cannot silently swap two buffers.
//
// A probe is PASS, FAIL, UNSUPPORTED or NOT_RUN. UNSUPPORTED -- the
// toolchain or the device cannot run it -- is a valid result for a probe
// (gate B0.2), never for the baseline kernel.

#include <bench_gpu.h>

#include <meshoptimizer.h>
#include <nlohmann/json.hpp>
#include <picosha2.h>

#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <system_error>
#include <tuple>

namespace {

using namespace gamegine::bench;

using Json = nlohmann::ordered_json;

// ---------------------------------------------------------- the shader set

struct Kernel {
    std::string           d_status;  // COMPILED, UNSUPPORTED, FAILED, ...
    std::string           d_reason;
    std::filesystem::path d_artifact;
    std::map<std::string, std::uint32_t, std::less<>> d_bindings;
    std::map<std::string, std::array<std::uint32_t, 3>, std::less<>> d_threads;
};

struct ShaderSet {
    Json d_hash;       // {"status": "OK", "value"} or {"status", "reason"}
    Json d_toolchain;  // the compilers that built the set, as the index has it
    std::map<std::string, Kernel, std::less<>> d_kernels;
};

std::string text(const Json& object, const char* key)
{
    return object.contains(key) && object[key].is_string()
               ? object[key].get<std::string>()
               : std::string();
}

// Load the kernels built for 'target' from 'directory'/index.json. Each
// artifact is digested and compared with the index's sha256, so the shader
// hash the report carries describes the bytes that actually run; a kernel
// whose artifact changed since the index was written is FAILED.
Result<ShaderSet> load_shaders(const std::filesystem::path& directory,
                               const std::string&           target)
{
    std::ifstream in(directory / "index.json");
    if (!in) {
        return fail("cannot read {}; build the shaders first ('xmake build "
                    "shaders')",
                    (directory / "index.json").string());
    }
    const Json index = Json::parse(in, nullptr, false);
    if (index.is_discarded() || !index.is_object()) {
        return fail("{} is not a shader index",
                    (directory / "index.json").string());
    }
    const Json hashes = index.value("shader_hash", Json::object());
    if (!hashes.contains(target)) {
        return fail("the index has no '{}' build; configure it with 'xmake f "
                    "--shader-targets={}' and rebuild the shaders",
                    target,
                    target);
    }

    ShaderSet set;
    set.d_hash      = hashes[target];
    set.d_toolchain = index.value("toolchain", Json::object());
    for (const Json& entry : index.value("kernels", Json::array())) {
        const Json results = entry.value("results", Json::object());
        if (!results.contains(target)) {
            continue;
        }
        const Json& record = results[target];
        Kernel      k;
        k.d_status = text(record, "status");
        k.d_reason = text(record, "reason");
        if (record.contains("artifact")) {
            k.d_artifact = directory / text(record, "artifact");
        }
        if (k.d_status == "COMPILED") {
            std::ifstream artifact(k.d_artifact, std::ios::binary);
            const std::vector<unsigned char> bytes(
                (std::istreambuf_iterator<char>(artifact)),
                std::istreambuf_iterator<char>());
            if (!artifact.is_open()) {
                k.d_status = "FAILED";
                k.d_reason = std::format("cannot read {}",
                                         k.d_artifact.string());
            } else if (picosha2::hash256_hex_string(bytes) !=
                       text(record, "sha256")) {
                k.d_status = "FAILED";
                k.d_reason = std::format("{} does not match the index's "
                                         "sha256; rebuild the shaders",
                                         k.d_artifact.string());
            }
        }
        for (const Json& b : record.value("bindings", Json::array())) {
            k.d_bindings[text(b, "name")] = b.value("index", 0u);
        }
        for (const Json& e : record.value("entry_points", Json::array())) {
            if (e.contains("threads") && e["threads"].size() == 3) {
                k.d_threads[text(e, "name")] =
                    e["threads"].get<std::array<std::uint32_t, 3>>();
            }
        }
        set.d_kernels[text(entry, "name")] = std::move(k);
    }
    return set;
}

// -------------------------------------------------------------- helpers

// One number a probe measured: a row of measurements.csv. 'd_sample' is the
// run, or the page for page_decode's per-page stages, for a raw value; it is
// -1 for a value the probe derives from them -- a median, a throughput, a
// ratio.
struct Measurement {
    std::string d_probe;
    std::string d_variant;
    std::string d_metric;
    int         d_sample;
    double      d_value;
    std::string d_unit;
};

struct Context {
    Gpu&                      d_gpu;
    const ShaderSet&          d_shaders;
    int                       d_runs;
    std::vector<Measurement>& d_measurements;
    std::string               d_probe;  // the probe running
};

// Record a number for the probe running, under 'variant': a raw value of
// sample 'sample', or, without one, a value derived from the samples.
void record(Context&         c,
            std::string_view variant,
            std::string_view metric,
            double           value,
            std::string_view unit,
            int              sample = -1)
{
    c.d_measurements.push_back({c.d_probe,
                                std::string(variant),
                                std::string(metric),
                                sample,
                                value,
                                std::string(unit)});
}

// common/bench.slang's pcgHash. The probes derive their inputs from it, and
// the host checks recompute them with it: it must stay identical.
std::uint32_t pcg_hash(std::uint32_t v)
{
    const std::uint32_t state = (v * 747796405u) + 2891336453u;
    const std::uint32_t word  = ((state >> ((state >> 28u) + 4u)) ^ state) *
                               277803737u;
    return (word >> 22u) ^ word;
}

// Return the first error among 'results', if any.
template <class... t_TYPES>
std::optional<std::string> first_error(const Result<t_TYPES>&... results)
{
    std::optional<std::string> error;
    ((error = error ? error
              : results.has_value()
                  ? std::optional<std::string>()
                  : std::optional<std::string>(results.error())),
     ...);
    return error;
}

// Resolve buffers named as in the kernel's Slang source to the indices the
// backend binds them at. Every binding must be given, and nothing else.
Result<std::vector<Binding>>
bind(const Kernel&                                              k,
     std::initializer_list<std::pair<std::string_view, Buffer>> buffers)
{
    std::vector<Binding> out;
    for (const auto& [name, buffer] : buffers) {
        auto it = k.d_bindings.find(name);
        if (it == k.d_bindings.end()) {
            return fail("the kernel has no binding named '{}'", name);
        }
        out.push_back({it->second, buffer});
    }
    if (out.size() != k.d_bindings.size()) {
        return fail("the kernel has {} bindings, {} were given",
                    k.d_bindings.size(),
                    out.size());
    }
    return out;
}

// A compute pipeline for 'entry', or a graphics one when 'fragment' is set.
Result<Pipeline> pipeline(Context&           c,
                          const Kernel&      k,
                          const std::string& entry,
                          const std::string& fragment = "",
                          bool               mesh     = false,
                          bool               cullBack = false)
{
    PipelineDesc d;
    d.d_artifact = k.d_artifact;
    d.d_entry    = entry;
    d.d_fragment = fragment;
    d.d_mesh     = mesh;
    d.d_cullBack = cullBack;
    if (mesh) {
        d.d_threads = {64, 1, 1};  // the probes' mesh shaders; see bench_gpu.h
    } else if (auto it = k.d_threads.find(entry); it != k.d_threads.end()) {
        d.d_threads = it->second;
    }
    return c.d_gpu.create_pipeline(d);
}

double median(std::vector<double> values)
{
    auto middle = values.begin() +
                  static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}

// One untimed warm-up, then the configured runs, each recorded under
// 'variant'; returns the median of each method, which it records too.
// 'reset', if given, runs untimed before every run, so that a kernel that
// accumulates into a buffer starts each run from the same state.
Result<Timing> measure(Context&                             c,
                       std::string_view                     variant,
                       std::span<const Command>             commands,
                       const std::function<Result<void>()>& reset = {})
{
    std::vector<double> gpu;
    std::vector<double> independent;
    for (int run = -1; run < c.d_runs; ++run) {
        if (reset) {
            if (auto r = reset(); !r) {
                return std::unexpected(r.error());
            }
        }
        auto t = c.d_gpu.run(commands);
        if (!t) {
            return std::unexpected(t.error());
        }
        if (run >= 0) {  // run -1 warms caches, clocks and pipelines
            gpu.push_back(t->d_gpuNs);
            independent.push_back(t->d_independentNs);
            record(c, variant, "gpu_ns", t->d_gpuNs, "ns", run);
            record(
                c, variant, "independent_ns", t->d_independentNs, "ns", run);
        }
    }
    const Timing timing{median(gpu), median(independent)};
    record(c, variant, "gpu_ns_p50", timing.d_gpuNs, "ns");
    record(c, variant, "independent_ns_p50", timing.d_independentNs, "ns");
    return timing;
}

// The 16-byte parameter block every kernel reads as 'params[0]'.
using Params = std::array<std::uint32_t, 4>;

using Clock = std::chrono::steady_clock;

template <class t_RANGE>
std::span<const std::byte> bytes_of(const t_RANGE& range)
{
    return std::as_bytes(std::span(range));
}

template <class t_TYPE>
std::span<std::byte> writable_bytes_of(std::vector<t_TYPE>& v)
{
    return std::as_writable_bytes(std::span(v));
}

double to_ms(double ns)
{
    return ns / 1.0e6;
}

double elapsed_ns(Clock::time_point from, Clock::time_point to)
{
    return std::chrono::duration<double, std::nano>(to - from).count();
}

double mib_per_second(double bytes, double ns)
{
    return ns > 0.0 ? bytes / (1024.0 * 1024.0) / (ns / 1.0e9) : 0.0;
}

// --------------------------------------------------------------- probes
//
// A probe records its numbers with 'record' and returns a 'Report', or why
// it failed -- including a wrong result: a fast wrong answer is a FAIL, not
// a number. What each kernel expects is in its header under
// shaders/probes/; what B0 measures with it is in
// doc/milestones/r0-benchmarks.md, sections 3 and 5.

// What a probe that ran says beside its numbers.
struct Report {
    std::string d_summary;      // one line for the console
    std::string d_notes;        // what was measured, in words
    std::string d_unsupported;  // set: the device refused the operation
};

Report done(std::string summary, std::string notes)
{
    return {std::move(summary), std::move(notes), {}};
}

// What a probe returns when the device refuses its pipeline: the device does
// not support the operation, the answer the vendor feature table can only
// predict. 'run_probe' reports it UNSUPPORTED, a valid result for a probe
// (B0.2; r0-benchmarks.md section 7) -- never for the baseline kernel, whose
// refused pipeline is a FAIL.
Report refused(std::string_view entry, const std::string& error)
{
    return {{},
            {},
            std::format("the device refused the {} pipeline: {}",
                        entry,
                        error)};
}

// The worked example: baseline/prefix_scan, the R0 toolchain kernel (B0.1).
// Three dispatches over 65536 uints; the host recomputes the sums.
Result<Report> prefix_scan(Context& c, const Kernel& k)
{
    constexpr std::uint32_t    count  = 65536;
    constexpr std::uint32_t    blocks = count / 256;
    std::vector<std::uint32_t> input(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        input[i] = pcg_hash(i) & 0xffffu;
    }
    const std::array<std::uint32_t, 4> params = {count, blocks, 0, 0};

    auto p    = c.d_gpu.create_buffer(sizeof(params));
    auto in   = c.d_gpu.create_buffer(std::size_t{count} * 4);
    auto out  = c.d_gpu.create_buffer(std::size_t{count} * 4);
    auto sums = c.d_gpu.create_buffer(std::size_t{blocks} * 4);
    if (auto error = first_error(p, in, out, sums)) {
        return fail("{}", *error);
    }
    auto up1   = c.d_gpu.upload(*p, std::as_bytes(std::span(params)));
    auto up2   = c.d_gpu.upload(*in, std::as_bytes(std::span(input)));
    auto a     = pipeline(c, k, "scanBlocks");
    auto b     = pipeline(c, k, "scanBlockSums");
    auto d     = pipeline(c, k, "addBlockOffsets");
    auto binds = bind(k,
                      {{"params", *p},
                       {"input", *in},
                       {"output", *out},
                       {"blockSums", *sums}});
    if (auto error = first_error(up1, up2, a, b, d, binds)) {
        return fail("{}", *error);
    }

    const std::vector<Command> commands =
        {Dispatch{*a, *binds, {blocks, 1, 1}},
         Dispatch{*b, *binds, {1, 1, 1}},
         Dispatch{*d, *binds, {blocks, 1, 1}}};
    const std::string_view variant = "elements=65536";
    auto                   timing  = measure(c, variant, commands);
    if (!timing) {
        return std::unexpected(timing.error());
    }
    std::vector<std::uint32_t> got(count);
    auto dl = c.d_gpu.download(*out, std::as_writable_bytes(std::span(got)));
    if (!dl) {
        return std::unexpected(dl.error());
    }

    std::uint32_t sum   = 0;
    std::size_t   wrong = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        sum += input[i];
        wrong += static_cast<std::size_t>(got[i] != sum);
    }
    if (wrong != 0u) {
        return fail("{} of {} prefix sums are wrong", wrong, count);
    }
    const double perMs = count / to_ms(timing->d_gpuNs);
    record(c, variant, "elements_per_ms", perMs, "elements/ms");
    return done(std::format("{:.3f} ms, {:.0f} elements/ms",
                            to_ms(timing->d_gpuNs),
                            perMs),
                "inclusive prefix sum of 65536 uints in three dispatches");
}

// ----------------------------------------- 64-bit atomic max (B0, D6b)

// What B0 records beside every atomic result (r0-benchmarks.md section 5);
// the device is in run.csv.
std::string atomic_notes(const char* shaderStage)
{
    return std::format("operation: InterlockedMax on uint64_t (MSL: "
                       "atomic_max_explicit on atomic_ulong, relaxed); "
                       "stage: {}; address space: device / storage buffer",
                       shaderStage);
}

// probes/atomic_max_u64_buffer: 65536 threads fold 16 keys each into
// 'slots' slots, from 1 (every thread on one address) to 65536. The host
// recomputes every slot's maximum.
Result<Report> atomic_max_buffer(Context& c, const Kernel& k)
{
    constexpr std::uint32_t          threads    = 65536;
    constexpr std::uint32_t          iterations = 16;
    const std::vector<std::uint64_t> zeros(threads, 0);

    auto params = c.d_gpu.create_buffer(sizeof(Params));
    auto keys   = c.d_gpu.create_buffer(threads * sizeof(std::uint64_t));
    if (auto error = first_error(params, keys)) {
        return fail("{}", *error);
    }
    auto pipe = pipeline(c, k, "atomicMaxBuffer");
    if (!pipe) {
        return refused("atomicMaxBuffer", pipe.error());
    }
    auto binds = bind(k, {{"params", *params}, {"keys", *keys}});
    if (!binds) {
        return std::unexpected(binds.error());
    }
    const std::vector<Command> commands = {
        Dispatch{*pipe, *binds, {threads / 256, 1, 1}}};
    const auto reset = [&] { return c.d_gpu.upload(*keys, bytes_of(zeros)); };

    std::string summary;
    for (std::uint32_t slots : {1u, 64u, 4096u, 65536u}) {
        const std::string variant = std::format("slots={}", slots);
        const Params      p       = {slots, iterations, 0, 0};
        auto              up      = c.d_gpu.upload(*params, bytes_of(p));
        if (!up) {
            return std::unexpected(up.error());
        }
        auto timing = measure(c, variant, commands, reset);
        if (!timing) {
            return std::unexpected(timing.error());
        }
        std::vector<std::uint64_t> got(threads);
        if (auto dl = c.d_gpu.download(*keys, writable_bytes_of(got)); !dl) {
            return std::unexpected(dl.error());
        }

        std::vector<std::uint64_t> expected(slots, 0);
        for (std::uint32_t t = 0; t < threads; ++t) {
            for (std::uint32_t i = 0; i < iterations; ++i) {
                const std::uint64_t key =
                    (std::uint64_t{pcg_hash((t * iterations) + i)} << 32) | t;
                std::uint64_t& slot = expected[(t + (i * 7919u)) % slots];
                slot                = std::max(slot, key);
            }
        }
        std::size_t wrong = 0;
        for (std::uint32_t s = 0; s < slots; ++s) {
            wrong += static_cast<std::size_t>(got[s] != expected[s]);
        }
        if (wrong != 0u) {
            return fail("{} of {} slot maxima are wrong: the operation ran, "
                        "but not as an atomic 64-bit max",
                        wrong,
                        slots);
        }

        const double atomics = threads * iterations;
        const double perMs   = atomics / to_ms(timing->d_gpuNs);
        record(c, variant, "atomics", atomics, "count");
        record(c, variant, "atomics_per_ms", perMs, "atomics/ms");
        summary += std::format("{}{} slots {:.0f} atomics/ms",
                               summary.empty() ? "" : ", ",
                               slots,
                               perMs);
    }
    return done(summary,
                std::format("{}; {} threads, {} atomics each",
                            atomic_notes("compute"),
                            threads,
                            iterations));
}

// probes/atomic_max_u64_fragment: a full-screen triangle drawn once per
// layer over a 1024 x 1024 target; every fragment folds a key into its
// pixel's slot. The host recomputes every pixel's maximum, so a pixel the
// draw missed shows as wrong.
Result<Report> atomic_max_fragment(Context& c, const Kernel& k)
{
    constexpr std::uint32_t          size   = 1024;
    constexpr std::uint32_t          layers = 8;
    constexpr std::uint32_t          pixels = size * size;
    const std::vector<std::uint64_t> zeros(pixels, 0);
    const Params                     p = {size, size, layers, 0};

    auto params = c.d_gpu.create_buffer(sizeof(Params));
    auto keys   = c.d_gpu.create_buffer(pixels * sizeof(std::uint64_t));
    auto target = c.d_gpu.create_target(size, size);
    if (auto error = first_error(params, keys, target)) {
        return fail("{}", *error);
    }
    auto pipe = pipeline(c, k, "layerVS", "atomicMaxFS");
    if (!pipe) {
        return refused("layerVS + atomicMaxFS", pipe.error());
    }
    auto up    = c.d_gpu.upload(*params, bytes_of(p));
    auto binds = bind(k, {{"params", *params}, {"keys", *keys}});
    if (auto error = first_error(up, binds)) {
        return fail("{}", *error);
    }

    const std::string          variant  = std::format("{}x{} layers={}",
                                            size,
                                            size,
                                            layers);
    const std::vector<Command> commands = {
        Draw{*pipe, *binds, *target, 3, layers}};
    auto timing = measure(c, variant, commands, [&] {
        return c.d_gpu.upload(*keys, bytes_of(zeros));
    });
    if (!timing) {
        return std::unexpected(timing.error());
    }
    std::vector<std::uint64_t> got(pixels);
    if (auto dl = c.d_gpu.download(*keys, writable_bytes_of(got)); !dl) {
        return std::unexpected(dl.error());
    }

    std::size_t wrong = 0;
    for (std::uint32_t px = 0; px < pixels; ++px) {
        std::uint64_t best = 0;
        for (std::uint32_t l = 0; l < layers; ++l) {
            best = std::max(
                best, (std::uint64_t{pcg_hash((px * layers) + l)} << 32) | l);
        }
        wrong += static_cast<std::size_t>(got[px] != best);
    }
    if (wrong != 0u) {
        return fail("{} of {} pixels hold the wrong maximum", wrong, pixels);
    }

    const double atomics = static_cast<double>(pixels) * layers;
    const double perMs   = atomics / to_ms(timing->d_gpuNs);
    record(c, variant, "fragment_atomics", atomics, "count");
    record(c, variant, "fragment_atomics_per_ms", perMs, "atomics/ms");
    return done(std::format("{:.3f} ms, {:.0f} fragment atomics/ms",
                            to_ms(timing->d_gpuNs),
                            perMs),
                atomic_notes("fragment"));
}

// -------------------------------------------------- mesh-shader probes

constexpr std::uint32_t k_CLUSTER_TRIANGLES = 98;  // common/bench.slang

// Mesh groups go in rows of at most this many: Metal silently drops a mesh
// grid past 65535 in one dimension, and nothing runs.
constexpr std::uint32_t k_GROUPS_PER_ROW = 4096;

// Both mesh kernels' parameters for 'clusters' clusters. The clusters tile
// the target on a square grid, so a square count covers all of it.
Params mesh_params(std::uint32_t clusters)
{
    const auto grid = static_cast<std::uint32_t>(
        std::ceil(std::sqrt(static_cast<double>(clusters))));
    return {clusters, grid, std::min(clusters, k_GROUPS_PER_ROW), 0};
}

std::array<std::uint32_t, 3> mesh_groups(std::uint32_t clusters)
{
    const std::uint32_t row = std::min(clusters, k_GROUPS_PER_ROW);
    return {row, (clusters + row - 1) / row, 1};
}

// The fraction of the 'size' x 'size' target the last draw covered: draws
// clear alpha to zero, and the mesh probes' fragment shaders write one.
Result<double> coverage(Context& c, Target target, std::uint32_t size)
{
    const std::size_t         pixels = std::size_t{size} * size;
    std::vector<std::uint8_t> rgba(pixels * 4);
    if (auto r = c.d_gpu.read_target(target, writable_bytes_of(rgba)); !r) {
        return std::unexpected(r.error());
    }
    std::size_t drawn = 0;
    for (std::size_t i = 3; i < rgba.size(); i += 4) {
        drawn += static_cast<std::size_t>(rgba[i] != 0);
    }
    return static_cast<double>(drawn) / static_cast<double>(pixels);
}

// probes/mesh_cluster_dispatch: 128^2, 256^2 and 512^2 clusters of 98
// triangles over a 1024 x 1024 target. Every group counts itself, so the
// counter must end equal to the clusters dispatched, and the target must be
// covered.
Result<Report> mesh_cluster_dispatch(Context& c, const Kernel& k)
{
    constexpr std::uint32_t            size = 1024;
    const std::array<std::uint32_t, 1> zero = {0};

    auto params   = c.d_gpu.create_buffer(sizeof(Params));
    auto counters = c.d_gpu.create_buffer(sizeof(std::uint32_t));
    auto target   = c.d_gpu.create_target(size, size);
    if (auto error = first_error(params, counters, target)) {
        return fail("{}", *error);
    }
    auto pipe = pipeline(c, k, "clusterMS", "clusterFS", true);
    if (!pipe) {
        return refused("clusterMS + clusterFS", pipe.error());
    }
    auto binds = bind(k, {{"params", *params}, {"counters", *counters}});
    if (!binds) {
        return std::unexpected(binds.error());
    }
    const auto reset = [&] {
        return c.d_gpu.upload(*counters, bytes_of(zero));
    };

    std::string summary;
    for (std::uint32_t count : {16384u, 65536u, 262144u}) {
        const std::string variant = std::format("clusters={}", count);
        const Params      p       = mesh_params(count);
        auto              up      = c.d_gpu.upload(*params, bytes_of(p));
        if (!up) {
            return std::unexpected(up.error());
        }
        const std::vector<Command> commands = {
            MeshDraw{*pipe, *binds, *target, mesh_groups(count)}};
        auto timing = measure(c, variant, commands, reset);
        if (!timing) {
            return std::unexpected(timing.error());
        }
        std::array<std::uint32_t, 1> ran = {0};
        auto                         dl  = c.d_gpu.download(*counters,
                                   std::as_writable_bytes(std::span(ran)));
        auto                         cov = coverage(c, *target, size);
        if (auto error = first_error(dl, cov)) {
            return fail("{}", *error);
        }
        if (ran[0] != count) {
            return fail("{} clusters dispatched, {} ran", count, ran[0]);
        }
        if (*cov < 0.99) {
            return fail("{} clusters tile the target, yet only {:.1f}% of it "
                        "was drawn",
                        count,
                        *cov * 100.0);
        }

        const double perMs = count / to_ms(timing->d_gpuNs);
        record(c,
               variant,
               "triangles",
               static_cast<double>(count) * k_CLUSTER_TRIANGLES,
               "count");
        record(c, variant, "coverage", *cov, "fraction");
        record(c, variant, "clusters_per_ms", perMs, "clusters/ms");
        record(c,
               variant,
               "triangles_per_ms",
               perMs * k_CLUSTER_TRIANGLES,
               "triangles/ms");
        summary += std::format("{}{} clusters {:.0f}/ms",
                               summary.empty() ? "" : ", ",
                               count,
                               perMs);
    }
    return done(summary,
                std::format("target {}x{} RGBA8, no depth; a cluster is 64 "
                            "vertices and 98 triangles, one 64-thread mesh "
                            "group",
                            size,
                            size));
}

// probes/mesh_double_sided: 256^2 clusters, every other one wound away from
// the viewer, drawn single-sided (back faces culled by the rasterizer) and
// double-sided (nothing culled, back faces shaded with a flipped normal).
// Double-sided must cover the target, single-sided half of it.
Result<Report> mesh_double_sided(Context& c, const Kernel& k)
{
    constexpr std::uint32_t size  = 1024;
    constexpr std::uint32_t count = 65536;
    const Params            p     = mesh_params(count);

    auto params = c.d_gpu.create_buffer(sizeof(Params));
    auto target = c.d_gpu.create_target(size, size);
    if (auto error = first_error(params, target)) {
        return fail("{}", *error);
    }
    auto up    = c.d_gpu.upload(*params, bytes_of(p));
    auto binds = bind(k, {{"params", *params}});
    if (auto error = first_error(up, binds)) {
        return fail("{}", *error);
    }

    std::array<double, 2> gpuNs{};
    std::array<double, 2> drawn{};  // [0] single, [1] double
    for (const bool cullBack : {true, false}) {
        const char* variant = cullBack ? "single_sided" : "double_sided";
        auto        pipe    = pipeline(
            c, k, "facingClusterMS", "shadeFS", true, cullBack);
        if (!pipe) {
            return refused("facingClusterMS + shadeFS", pipe.error());
        }
        const std::vector<Command> commands = {
            MeshDraw{*pipe, *binds, *target, mesh_groups(count)}};
        auto timing = measure(c, variant, commands);
        if (!timing) {
            return std::unexpected(timing.error());
        }
        auto cov = coverage(c, *target, size);
        if (!cov) {
            return std::unexpected(cov.error());
        }
        const std::size_t i = cullBack ? 0 : 1;
        gpuNs[i]            = timing->d_gpuNs;
        drawn[i]            = *cov;
        record(c, variant, "coverage", *cov, "fraction");
    }

    const double half = drawn[0] / drawn[1];
    if (drawn[1] < 0.99 || half < 0.45 || half > 0.55) {
        return fail("coverage is {:.1f}% double-sided and {:.1f}% "
                    "single-sided: culling did not remove the half facing "
                    "away",
                    drawn[1] * 100.0,
                    drawn[0] * 100.0);
    }
    const double ratio = gpuNs[1] / gpuNs[0];
    record(c, "double_over_single", "gpu_ns_p50_ratio", ratio, "ratio");
    return done(std::format("single {:.3f} ms, double {:.3f} ms, ratio {:.3f}",
                            to_ms(gpuNs[0]),
                            to_ms(gpuNs[1]),
                            ratio),
                std::format("{} clusters ({} triangles), every other one "
                            "wound away; single-sided culls back faces, "
                            "double-sided culls nothing and flips back-face "
                            "normals; target {}x{} RGBA8, no depth",
                            count,
                            count * k_CLUSTER_TRIANGLES,
                            size,
                            size));
}

// -------------------------------------------------- page decode (B0, R1)
//
// The provisional page format is the one probes/page_decode.slang
// documents: 16-byte records of 16-bit positions quantized to the page's
// bounds, an 8+8-bit octahedral normal, half-float texture coordinates and a
// cluster id. The payload is a procedural heightfield in 64 x 64-vertex
// tiles, the order a cluster builder would emit; the report names it as
// exactly that, provisional until the R1 corpus exists.

using PackedVertex = std::array<std::uint32_t, 4>;

struct PageHeader {
    std::array<float, 4> d_minimum;
    std::array<float, 4> d_extent;
};

struct DecodedVertex {
    std::array<float, 4> d_positionU;
    std::array<float, 4> d_normalV;
};

struct SourceVertex {
    std::array<float, 3> d_position;
    std::array<float, 3> d_normal;
    std::array<float, 2> d_uv;
    std::uint32_t        d_cluster;
};

std::array<float, 3> normalize3(std::array<float, 3> v)
{
    const float length = std::sqrt((v[0] * v[0]) + (v[1] * v[1]) +
                                   (v[2] * v[2]));
    return {v[0] / length, v[1] / length, v[2] / length};
}

// A 'width' x 'height' heightfield, 0.25 m apart, in 64 x 64 tiles: three
// octaves of smooth waves, with analytic normals.
std::vector<SourceVertex> make_heightfield(std::uint32_t width,
                                           std::uint32_t height)
{
    std::vector<SourceVertex> out;
    out.reserve(std::size_t{width} * height);
    for (std::uint32_t tz = 0; tz < height / 64; ++tz) {
        for (std::uint32_t tx = 0; tx < width / 64; ++tx) {
            for (std::uint32_t j = 0; j < 64; ++j) {
                for (std::uint32_t i = 0; i < 64; ++i) {
                    const std::uint32_t gx = (tx * 64) + i;
                    const std::uint32_t gz = (tz * 64) + j;
                    const float         x  = static_cast<float>(gx) * 0.25f;
                    const float         z  = static_cast<float>(gz) * 0.25f;
                    const float         h =
                        (8.0f * std::sin(x * 0.013f) * std::cos(z * 0.011f)) +
                        (3.0f * std::sin((x * 0.071f) + (z * 0.053f))) +
                        (0.7f * std::sin(x * 0.31f) * std::sin(z * 0.27f));
                    const float dx = (0.104f * std::cos(x * 0.013f) *
                                      std::cos(z * 0.011f)) +
                                     (0.213f *
                                      std::cos((x * 0.071f) + (z * 0.053f))) +
                                     (0.217f * std::cos(x * 0.31f) *
                                      std::sin(z * 0.27f));
                    const float dz = (-0.088f * std::sin(x * 0.013f) *
                                      std::sin(z * 0.011f)) +
                                     (0.159f *
                                      std::cos((x * 0.071f) + (z * 0.053f))) +
                                     (0.189f * std::sin(x * 0.31f) *
                                      std::cos(z * 0.27f));
                    out.push_back({{x, h, z},
                                   normalize3({-dx, 1.0f, -dz}),
                                   {static_cast<float>(gx) /
                                        static_cast<float>(width - 1),
                                    static_cast<float>(gz) /
                                        static_cast<float>(height - 1)},
                                   (tz * (width / 64)) + tx});
                }
            }
        }
    }
    return out;
}

// Octahedral normal encode to 8+8 bits, and the kernel's decode, operation
// for operation: Cigolle et al., JCGT 2014.
std::uint32_t oct_encode(const std::array<float, 3>& n)
{
    const float sum = std::fabs(n[0]) + std::fabs(n[1]) + std::fabs(n[2]);
    float       x   = n[0] / sum;
    float       y   = n[1] / sum;
    if (n[2] < 0.0f) {
        const float ox = (1.0f - std::fabs(y)) * (x >= 0.0f ? 1.0f : -1.0f);
        y              = (1.0f - std::fabs(x)) * (y >= 0.0f ? 1.0f : -1.0f);
        x              = ox;
    }
    const auto q = [](float v) {
        return static_cast<std::uint32_t>(
            std::lround(std::clamp((v * 0.5f) + 0.5f, 0.0f, 1.0f) * 255.0f));
    };
    return q(x) | (q(y) << 8);
}

std::array<float, 3> oct_decode(float ex, float ey)
{
    std::array<float, 3> v = {ex, ey, 1.0f - std::fabs(ex) - std::fabs(ey)};
    if (v[2] < 0.0f) {
        const float x = (1.0f - std::fabs(v[1])) *
                        (v[0] >= 0.0f ? 1.0f : -1.0f);
        v[1] = (1.0f - std::fabs(v[0])) * (v[1] >= 0.0f ? 1.0f : -1.0f);
        v[0] = x;
    }
    return normalize3(v);
}

// Half floats for texture coordinates in [0, 1]: zero and normal halves
// only, rounded to nearest even. 'half_to_float' is the kernel's integer
// decode, halfToFloat in page_decode.slang.
std::uint32_t float_to_half(float value)
{
    if (value < 6.103515625e-05f) {
        return 0;
    }
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    std::uint32_t       half = (((bits >> 23) - 112u) << 10) |
                         ((bits >> 13) & 0x3ffu);
    const std::uint32_t rest = bits & 0x1fffu;
    if (rest > 0x1000u || (rest == 0x1000u && ((half & 1u) != 0u))) {
        ++half;
    }
    return half;
}

float half_to_float(std::uint32_t h)
{
    const std::uint32_t sign     = (h & 0x8000u) << 16;
    const std::uint32_t exponent = (h >> 10) & 0x1fu;
    const std::uint32_t mantissa = h & 0x3ffu;
    if (exponent == 0) {
        const float v = static_cast<float>(mantissa) * (1.0f / 16777216.0f);
        return (sign != 0u) ? -v : v;
    }
    return std::bit_cast<float>(sign |
                                ((exponent == 31 ? 255u : exponent + 112u)
                                 << 23) |
                                (mantissa << 13));
}

// Quantize 'source' into pages of 'perPage' records, with their headers.
void quantize(std::span<const SourceVertex> source,
              std::uint32_t                 perPage,
              std::vector<PackedVertex>&    records,
              std::vector<PageHeader>&      headers)
{
    records.resize(source.size());
    headers.resize(source.size() / perPage);
    for (std::size_t page = 0; page < headers.size(); ++page) {
        const auto first = source.begin() +
                           static_cast<std::ptrdiff_t>(page * perPage);
        PageHeader& h = headers[page];
        for (std::size_t a = 0; a < 3; ++a) {
            auto [lo, hi] = std::minmax_element(
                first, first + perPage, [a](const auto& l, const auto& r) {
                    return l.d_position[a] < r.d_position[a];
                });
            h.d_minimum[a] = lo->d_position[a];
            h.d_extent[a]  = std::max(hi->d_position[a] - lo->d_position[a],
                                     std::numeric_limits<float>::min());
        }
        for (std::size_t v = page * perPage; v < (page + 1) * perPage; ++v) {
            const SourceVertex&          s = source[v];
            std::array<std::uint32_t, 3> q{};
            for (std::size_t a = 0; a < 3; ++a) {
                const float t = (s.d_position[a] - h.d_minimum[a]) /
                                h.d_extent[a];
                q[a] = static_cast<std::uint32_t>(
                    std::lround(std::clamp(t, 0.0f, 1.0f) * 65535.0f));
            }
            records[v] = {q[0] | (q[1] << 16),
                          q[2] | (oct_encode(s.d_normal) << 16),
                          float_to_half(s.d_uv[0]) |
                              (float_to_half(s.d_uv[1]) << 16),
                          s.d_cluster};
        }
    }
}

// Expand one record with its page's header, as the kernel does.
DecodedVertex decode_reference(const PackedVertex& w, const PageHeader& h)
{
    const float q[3] = {static_cast<float>(w[0] & 0xffffu) / 65535.0f,
                        static_cast<float>(w[0] >> 16) / 65535.0f,
                        static_cast<float>(w[1] & 0xffffu) / 65535.0f};
    const auto  n    = oct_decode(
        (static_cast<float>((w[1] >> 16) & 0xffu) / 255.0f * 2.0f) - 1.0f,
        (static_cast<float>(w[1] >> 24) / 255.0f * 2.0f) - 1.0f);
    return {{h.d_minimum[0] + (q[0] * h.d_extent[0]),
             h.d_minimum[1] + (q[1] * h.d_extent[1]),
             h.d_minimum[2] + (q[2] * h.d_extent[2]),
             half_to_float(w[2] & 0xffffu)},
            {n[0], n[1], n[2], half_to_float(w[2] >> 16)}};
}

// The value of rank 'q' in [0, 1], nearest rank.
double percentile(std::vector<double> values, double q)
{
    std::sort(values.begin(), values.end());
    const long rank = std::lround(q * static_cast<double>(values.size() - 1));
    return values[static_cast<std::size_t>(rank)];
}

double total(const std::vector<double>& values)
{
    double sum = 0.0;
    for (const double v : values) {
        sum += v;
    }
    return sum;
}

// A stage timed page by page: its throughput over every page, and the
// latency of one.
void stage_stats(Context&                   c,
                 std::string_view           variant,
                 std::string_view           stage,
                 double                     bytes,
                 const std::vector<double>& ns)
{
    record(c,
           variant,
           std::format("{}_mib_per_s", stage),
           mib_per_second(bytes, total(ns)),
           "MiB/s");
    record(c,
           variant,
           std::format("{}_page_ns_p50", stage),
           percentile(ns, 0.5),
           "ns");
    record(c,
           variant,
           std::format("{}_page_ns_p95", stage),
           percentile(ns, 0.95),
           "ns");
}

// probes/page_decode: a 2048 x 1024-vertex payload (32 MiB of records) cut
// into pages of 64, 128 and 256 KiB. For each size: compress every page
// (untimed, the cook), then time the four stages B0 wants separately -- disk
// read, CPU decompression, upload and GPU attribute decode.
Result<Report> page_decode(Context& c, const Kernel& k)
{
    constexpr std::uint32_t width  = 2048;
    constexpr std::uint32_t height = 1024;
    const auto              source = make_heightfield(width, height);
    const auto              count  = static_cast<std::uint32_t>(source.size());
    const double            rawBytes = static_cast<double>(count) * 16.0;

    auto params  = c.d_gpu.create_buffer(sizeof(Params));
    auto packed  = c.d_gpu.create_buffer(count * sizeof(PackedVertex));
    auto decoded = c.d_gpu.create_buffer(count * sizeof(DecodedVertex));
    if (auto error = first_error(params, packed, decoded)) {
        return fail("{}", *error);
    }
    auto pipe = pipeline(c, k, "decodePage");
    if (!pipe) {
        return refused("decodePage", pipe.error());
    }

    std::string summary;
    for (std::uint32_t kib : {64u, 128u, 256u}) {
        const std::string         variant = std::format("page={}KiB", kib);
        const std::uint32_t       perPage = kib * 1024 / 16;
        const std::uint32_t       pages   = count / perPage;
        std::vector<PackedVertex> records;
        std::vector<PageHeader>   headers;
        quantize(source, perPage, records, headers);

        // The cook, offline and untimed: compress each page.
        std::vector<std::vector<unsigned char>> blobs(pages);
        std::size_t                             compressed = 0;
        for (std::uint32_t pg = 0; pg < pages; ++pg) {
            auto& blob = blobs[pg];
            blob.resize(meshopt_encodeVertexBufferBound(perPage,
                                                        sizeof(PackedVertex)));
            blob.resize(meshopt_encodeVertexBuffer(
                blob.data(),
                blob.size(),
                records.data() + (std::size_t{pg} * perPage),
                perPage,
                sizeof(PackedVertex)));
            compressed += blob.size();
        }

        // Stage 1: disk read, one read per page, of a file just written.
        const auto file = std::filesystem::temp_directory_path() /
                          std::format("vg_bench-page-decode-{}k.bin", kib);
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            for (const auto& blob : blobs) {
                out.write(reinterpret_cast<const char*>(blob.data()),
                          static_cast<std::streamsize>(blob.size()));
            }
            if (!out) {
                return fail("cannot write {}", file.string());
            }
        }
        std::vector<std::vector<unsigned char>> pagesRead(pages);
        std::vector<double>                     readNs;
        std::ifstream                           in(file, std::ios::binary);
        for (std::uint32_t pg = 0; pg < pages; ++pg) {
            pagesRead[pg].resize(blobs[pg].size());
            const auto t0 = Clock::now();
            in.read(reinterpret_cast<char*>(pagesRead[pg].data()),
                    static_cast<std::streamsize>(pagesRead[pg].size()));
            readNs.push_back(elapsed_ns(t0, Clock::now()));
        }
        const bool readOk = static_cast<bool>(in);
        in.close();
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        if (!readOk) {
            return fail("cannot read back {}", file.string());
        }

        // Stage 2: CPU decompression, one thread, page by page.
        std::vector<PackedVertex> unpacked(count);
        std::vector<double>       cpuNs;
        int                       errors = 0;
        for (std::uint32_t pg = 0; pg < pages; ++pg) {
            const auto t0 = Clock::now();
            errors += static_cast<int>(
                meshopt_decodeVertexBuffer(unpacked.data() +
                                               (std::size_t{pg} * perPage),
                                           perPage,
                                           sizeof(PackedVertex),
                                           pagesRead[pg].data(),
                                           pagesRead[pg].size()) != 0);
            cpuNs.push_back(elapsed_ns(t0, Clock::now()));
        }
        if (errors != 0 || unpacked != records) {
            return fail("{} KiB pages do not round-trip through the codec",
                        kib);
        }

        // Stage 3: upload of every decompressed page, host time around
        // Gpu::upload, which returns once the GPU buffer holds them.
        std::vector<double> uploadNs;
        for (int run = 0; run < c.d_runs; ++run) {
            const auto t0 = Clock::now();
            auto       u  = c.d_gpu.upload(*packed, bytes_of(unpacked));
            uploadNs.push_back(elapsed_ns(t0, Clock::now()));
            if (!u) {
                return std::unexpected(u.error());
            }
        }

        // Stage 4: GPU attribute decode of every record.
        const Params p = {count, perPage, 0, 0};
        auto hdr = c.d_gpu.create_buffer(headers.size() * sizeof(PageHeader));
        if (!hdr) {
            return std::unexpected(hdr.error());
        }
        auto up1   = c.d_gpu.upload(*params, bytes_of(p));
        auto up2   = c.d_gpu.upload(*hdr, bytes_of(headers));
        auto binds = bind(k,
                          {{"params", *params},
                           {"headers", *hdr},
                           {"packed", *packed},
                           {"decoded", *decoded}});
        if (auto error = first_error(up1, up2, binds)) {
            return fail("{}", *error);
        }
        const std::vector<Command> commands = {
            Dispatch{*pipe, *binds, {count / 256, 1, 1}}};
        auto timing = measure(c, variant, commands);
        if (!timing) {
            return std::unexpected(timing.error());
        }

        // Every decoded vertex against the host decode. Metal compiles with
        // fast math, so positions get a relative tolerance and normals an
        // absolute one; texture coordinates are exact.
        std::vector<DecodedVertex> gpu(count);
        if (auto dl = c.d_gpu.download(*decoded, writable_bytes_of(gpu));
            !dl) {
            return std::unexpected(dl.error());
        }
        std::size_t wrong = 0;
        for (std::uint32_t v = 0; v < count; ++v) {
            const DecodedVertex ref = decode_reference(records[v],
                                                       headers[v / perPage]);
            bool ok = gpu[v].d_positionU[3] == ref.d_positionU[3] &&
                      gpu[v].d_normalV[3] == ref.d_normalV[3];
            for (std::size_t a = 0; a < 3; ++a) {
                const float tol = 1.0e-5f *
                                  std::max(1.0f,
                                           std::fabs(ref.d_positionU[a]));
                ok = ok &&
                     std::fabs(gpu[v].d_positionU[a] - ref.d_positionU[a]) <=
                         tol &&
                     std::fabs(gpu[v].d_normalV[a] - ref.d_normalV[a]) <=
                         1.0e-4f;
            }
            wrong += static_cast<std::size_t>(!ok);
        }
        if (wrong != 0u) {
            return fail("{} of {} GPU-decoded vertices differ from the host "
                        "decode at {} KiB",
                        wrong,
                        count,
                        kib);
        }

        // The per-page and per-run samples, then what derives from them.
        for (std::uint32_t pg = 0; pg < pages; ++pg) {
            const int page = static_cast<int>(pg);
            record(c, variant, "disk_read_ns", readNs[pg], "ns", page);
            record(c, variant, "cpu_decompress_ns", cpuNs[pg], "ns", page);
        }
        for (std::size_t run = 0; run < uploadNs.size(); ++run) {
            record(c,
                   variant,
                   "upload_ns",
                   uploadNs[run],
                   "ns",
                   static_cast<int>(run));
        }
        const double uploadP50 = median(uploadNs);
        record(c, variant, "pages", pages, "count");
        record(c, variant, "raw_bytes", rawBytes, "bytes");
        record(c,
               variant,
               "compressed_bytes",
               static_cast<double>(compressed),
               "bytes");
        record(c,
               variant,
               "compression_ratio",
               rawBytes / static_cast<double>(compressed),
               "ratio");
        stage_stats(
            c, variant, "disk_read", static_cast<double>(compressed), readNs);
        stage_stats(c, variant, "cpu_decompress", rawBytes, cpuNs);
        record(c, variant, "upload_ns_p50", uploadP50, "ns");
        record(c,
               variant,
               "upload_mib_per_s",
               mib_per_second(rawBytes, uploadP50),
               "MiB/s");
        record(c,
               variant,
               "gpu_decode_mib_per_s",
               mib_per_second(rawBytes, timing->d_gpuNs),
               "MiB/s");
        summary += std::format(
            "{}{}K: read {:.0f}, cpu {:.0f}, upload {:.0f}, "
            "gpu {:.0f} MiB/s",
            summary.empty() ? "" : "; ",
            kib,
            mib_per_second(static_cast<double>(compressed), total(readNs)),
            mib_per_second(rawBytes, total(cpuNs)),
            mib_per_second(rawBytes, uploadP50),
            mib_per_second(rawBytes, timing->d_gpuNs));
    }
    return done(
        summary,
        std::format(
            "payload: procedural heightfield, {} x {} vertices 0.25 m apart, "
            "in 64 x 64-vertex tiles (provisional; replaced by the R1 "
            "corpus); record: 16 bytes, x:16 y:16 | z:16 oct-normal:8+8 | "
            "u:half v:half | cluster:32; codec: meshoptimizer vertex codec "
            "(meshopt_encodeVertexBuffer), MESHOPTIMIZER_VERSION {}; disk "
            "read: warm, the file was written just before it was read, so "
            "the OS page cache holds it -- not storage throughput; upload: "
            "host time around Gpu::upload; GPU decode: decodePage over every "
            "record",
            width,
            height,
            MESHOPTIMIZER_VERSION));
}

// ------------------------------------------- timing agreement (B0.6)
//
// Gate B0.6: the GPU timestamps agree, within 5% on every run, with an
// independent method timing the same submission -- on Metal the command
// buffer's GPUEndTime - GPUStartTime, on Vulkan host time from vkQueueSubmit
// returning to the fence. There is no workload of known duration to check
// either against (r0-benchmarks.md section 4), so the two are compared on
// one long submission: the independent method carries a fixed cost, which
// must be small beside the scope.
//
// The workload is decodePage over 4M zeroed records, repeated to about 20
// ms in at most 48 dispatches: a few long commands rather than many short
// ones, each of which costs the Vulkan backend a descriptor set. decodePage's
// time does not depend on the values it decodes.
Result<Report> timing_agreement(Context& c, const Kernel& k)
{
    constexpr std::uint32_t count      = 1u << 22;
    constexpr std::uint32_t perPage    = 128 * 1024 / 16;
    constexpr double        scopeNs    = 20.0e6;
    constexpr double        maxRepeats = 48.0;

    const std::vector<std::byte> zeros(std::size_t{count} *
                                       sizeof(PackedVertex));
    const Params                 p = {count, perPage, 0, 0};

    auto params  = c.d_gpu.create_buffer(sizeof(Params));
    auto headers = c.d_gpu.create_buffer(count / perPage * sizeof(PageHeader));
    auto packed  = c.d_gpu.create_buffer(zeros.size());
    auto decoded = c.d_gpu.create_buffer(std::size_t{count} *
                                         sizeof(DecodedVertex));
    if (auto error = first_error(params, headers, packed, decoded)) {
        return fail("{}", *error);
    }
    auto up1   = c.d_gpu.upload(*params, bytes_of(p));
    auto up2   = c.d_gpu.upload(*headers,
                              std::span(zeros).first(count / perPage *
                                                     sizeof(PageHeader)));
    auto up3   = c.d_gpu.upload(*packed, zeros);
    auto pipe  = pipeline(c, k, "decodePage");
    auto binds = bind(k,
                      {{"params", *params},
                       {"headers", *headers},
                       {"packed", *packed},
                       {"decoded", *decoded}});
    if (auto error = first_error(up1, up2, up3, pipe, binds)) {
        return fail("{}", *error);
    }

    // One dispatch first, which also warms caches, clocks and the pipeline,
    // to find how many make the scope.
    const Dispatch             one{*pipe, *binds, {count / 256, 1, 1}};
    const std::vector<Command> single = {one};
    auto                       first  = c.d_gpu.run(single);
    if (!first) {
        return std::unexpected(first.error());
    }
    const auto                 repeats = static_cast<std::size_t>(std::clamp(
        std::ceil(scopeNs / std::max(first->d_gpuNs, 1.0)), 1.0, maxRepeats));
    const std::vector<Command> commands(repeats, one);
    const std::string          variant = std::format("dispatches={}", repeats);

    std::vector<double> gpu;
    std::vector<double> ratios;
    for (int run = -1; run < c.d_runs; ++run) {
        auto t = c.d_gpu.run(commands);
        if (!t) {
            return std::unexpected(t.error());
        }
        if (t->d_independentNs <= 0.0) {
            return fail("the independent method measured no time");
        }
        if (run < 0) {
            continue;  // warm-up
        }
        gpu.push_back(t->d_gpuNs);
        ratios.push_back(t->d_gpuNs / t->d_independentNs);
        record(c, variant, "gpu_ns", t->d_gpuNs, "ns", run);
        record(c, variant, "independent_ns", t->d_independentNs, "ns", run);
        record(c, variant, "ratio", ratios.back(), "ratio", run);
    }

    double worst = 0.0;
    for (const double r : ratios) {
        worst = std::max(worst, std::fabs(1.0 - r));
    }
    const double ratioP50 = median(ratios);
    const double scopeP50 = median(gpu);
    record(c, variant, "ratio_p50", ratioP50, "ratio");
    record(c, variant, "worst_deviation", worst, "fraction");

    // The same workload as a streaming rate: each record is read (16 bytes)
    // and written decoded (32 bytes) once, with no reuse, so the rate is the
    // memory bandwidth decodePage reaches -- the "bandwidth" workload of the
    // Apple-to-Linux ratios.
    const double streamed = static_cast<double>(count) *
                            static_cast<double>(sizeof(PackedVertex) +
                                                sizeof(DecodedVertex));
    record(c,
           variant,
           "stream_mib_per_s",
           mib_per_second(streamed, scopeP50 / static_cast<double>(repeats)),
           "MiB/s");
    if (worst > 0.05) {
        return fail("over {} dispatches ({:.1f} ms), the two methods differ "
                    "by up to {:.1f}%, beyond the 5% B0.6 allows",
                    repeats,
                    to_ms(scopeP50),
                    worst * 100.0);
    }

    const bool vulkan = c.d_gpu.api() == "vulkan";
    return done(
        std::format("{} dispatches, {:.1f} ms: timestamps / independent p50 "
                    "{:.4f}, worst deviation {:.2f}%",
                    repeats,
                    to_ms(scopeP50),
                    ratioP50,
                    worst * 100.0),
        std::format(
            "gate B0.6: GPU timestamps against {}, on one submission of "
            "decodePage over {} records, required within 5% on every run; "
            "uncertainty: {}",
            vulkan ? "host time from vkQueueSubmit returning to the fence"
                   : "the command buffer's GPUEndTime - GPUStartTime",
            count,
            vulkan ? "the independent time reads high by the host's wake-up "
                     "after the fence, and low by any work the GPU began "
                     "before vkQueueSubmit returned: fixed costs of order "
                     "0.1 ms"
                   : "the command buffer's span also covers the gaps before "
                     "its first pass and after its last, a few "
                     "microseconds"));
}

// -------------------------------------------------------------- registry

struct Probe {
    const char*            d_id;
    const char*            d_kernel;
    std::optional<Feature> d_feature;  // what the device must support
    Result<Report> (*d_run)(Context&, const Kernel&);  // null: not written
};

// constexpr: built at compile time, so nothing can throw before main().
constexpr Probe k_PROBES[] = {
    {"baseline.prefix_scan",
     "baseline/prefix_scan",
     std::nullopt,
     prefix_scan},
    {"probe.atomic_max_u64_buffer",
     "probes/atomic_max_u64_buffer",
     Feature::e_INT64_BUFFER_ATOMICS,
     atomic_max_buffer},
    {"probe.atomic_max_u64_fragment",
     "probes/atomic_max_u64_fragment",
     Feature::e_INT64_FRAGMENT_ATOMICS,
     atomic_max_fragment},
    {"probe.mesh_cluster_dispatch",
     "probes/mesh_cluster_dispatch",
     Feature::e_MESH_SHADERS,
     mesh_cluster_dispatch},
    {"probe.mesh_double_sided",
     "probes/mesh_double_sided",
     Feature::e_MESH_SHADERS,
     mesh_double_sided},
    {"probe.page_decode", "probes/page_decode", std::nullopt, page_decode},
    {"check.b0_6_timing_agreement",
     "probes/page_decode",
     std::nullopt,
     timing_agreement},
};

// The outcome of one probe: a row of probes.csv.
struct Outcome {
    std::string d_probe;
    std::string d_kernel;
    std::string d_status;         // PASS, FAIL, UNSUPPORTED or NOT_RUN
    std::string d_unsupportedBy;  // "toolchain" or "device"
    std::string d_reason;
    std::string d_summary;
    std::string d_notes;
};

Outcome run_probe(Context& c, const Probe& p)
{
    Outcome           o{p.d_id, p.d_kernel, "", "", "", "", ""};
    const std::size_t first = c.d_measurements.size();
    c.d_probe               = p.d_id;

    auto it = c.d_shaders.d_kernels.find(p.d_kernel);
    if (p.d_run == nullptr) {
        o.d_status = "NOT_RUN";
        o.d_reason = "not written yet";
    } else if (it == c.d_shaders.d_kernels.end()) {
        o.d_status = "FAIL";
        o.d_reason = "not in the shader index; rebuild the shaders";
    } else if (it->second.d_status == "UNSUPPORTED") {
        o.d_status        = "UNSUPPORTED";
        o.d_unsupportedBy = "toolchain";
        o.d_reason        = it->second.d_reason;
    } else if (it->second.d_status != "COMPILED") {
        o.d_status = "FAIL";
        o.d_reason = it->second.d_status + ": " + it->second.d_reason;
    } else if (p.d_feature && !c.d_gpu.supports(*p.d_feature)) {
        o.d_status        = "UNSUPPORTED";
        o.d_unsupportedBy = "device";
        o.d_reason        = "the device does not support what it needs";
    } else if (auto r = p.d_run(c, it->second); !r) {
        o.d_status = "FAIL";
        o.d_reason = r.error();
    } else if (!r->d_unsupported.empty()) {
        o.d_status        = "UNSUPPORTED";
        o.d_unsupportedBy = "device";
        o.d_reason        = r->d_unsupported;
    } else {
        o.d_status  = "PASS";
        o.d_summary = r->d_summary;
        o.d_notes   = r->d_notes;
    }

    // A probe that does not pass keeps no numbers: what it measured before
    // failing is not a measurement.
    if (o.d_status != "PASS") {
        c.d_measurements.erase(c.d_measurements.begin() +
                                   static_cast<std::ptrdiff_t>(first),
                               c.d_measurements.end());
    }
    std::printf("  [%s] %s  %s\n",
                o.d_status.c_str(),
                p.d_id,
                (o.d_status == "PASS" ? o.d_summary : o.d_reason).c_str());
    return o;
}

// ------------------------------------------------------------------ report

using Rows = std::vector<std::vector<std::string>>;

// A CSV field, quoted when it holds a comma, a quote or a line break.
std::string csv_field(std::string_view s)
{
    if (s.find_first_of(",\"\r\n") == std::string_view::npos) {
        return std::string(s);
    }
    std::string out = "\"";
    for (const char ch : s) {
        out += ch;
        if (ch == '"') {
            out += '"';
        }
    }
    return out + '"';
}

bool write_csv(const std::filesystem::path&    file,
               const std::vector<std::string>& header,
               const Rows&                     rows)
{
    std::ofstream csv(file);
    const auto    line = [&csv](const std::vector<std::string>& fields) {
        for (std::size_t i = 0; i < fields.size(); ++i) {
            csv << (i != 0 ? "," : "") << csv_field(fields[i]);
        }
        csv << '\n';
    };
    line(header);
    for (const auto& row : rows) {
        line(row);
    }
    return static_cast<bool>(csv);
}

const char k_USAGE[] =
    "usage: vg_bench [--backend=vulkan|metal] [--shaders=<dir>] "
    "[--out=<dir>] [--runs=<n>] [--list]\n";

// Report 'message' on stderr. If that fails too, there is nowhere left to
// report it.
void report_error(std::string_view message)
{
    static_cast<void>(std::fprintf(stderr,
                                   "vg_bench: %.*s\n",
                                   static_cast<int>(message.size()),
                                   message.data()));
}

// 'text' as a run count: a whole number of at least 1, or nothing.
std::optional<int> parse_runs(const std::string& text)
{
    int         value       = 0;
    const char* first       = text.c_str();
    const char* last        = first + text.size();
    const auto [end, error] = std::from_chars(first, last, value);
    if (error != std::errc() || end != last || value < 1) {
        return std::nullopt;
    }
    return value;
}

int run_bench(int argc, char** argv)
{
#ifdef __APPLE__
    std::string backend = "metal";
#else
    std::string backend = "vulkan";
#endif
    std::filesystem::path shaders = "build/shaders";
    std::filesystem::path out;
    int                   runs = 5;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--list") {
            for (const Probe& p : k_PROBES) {
                std::printf("%s\n", p.d_id);
            }
            return 0;
        }
        if (arg.starts_with("--backend=")) {
            backend = arg.substr(10);
            if (backend != "vulkan" && backend != "metal") {
                report_error(std::format("--backend is vulkan or metal, not "
                                         "'{}'",
                                         backend));
                return 2;
            }
        } else if (arg.starts_with("--shaders=")) {
            shaders = arg.substr(10);
        } else if (arg.starts_with("--out=")) {
            out = arg.substr(6);
        } else if (arg.starts_with("--runs=")) {
            const auto parsed = parse_runs(std::string(arg.substr(7)));
            if (!parsed) {
                report_error(std::format("--runs wants a whole number of at "
                                         "least 1, not '{}'",
                                         arg.substr(7)));
                return 2;
            }
            runs = *parsed;
        } else {
            static_cast<void>(std::fprintf(stderr, "%s", k_USAGE));
            return 2;
        }
    }
    if (out.empty()) {
        out = std::filesystem::path("results") / "vg_bench" / backend;
    }

    auto gpu = backend == "metal" ? create_metal() : create_vulkan();
    if (!gpu) {
        report_error(gpu.error());
        return 2;
    }
    auto set = load_shaders(shaders, std::string((*gpu)->shader_target()));
    if (!set) {
        report_error(set.error());
        return 2;
    }
    std::printf("vg_bench  %s, %s shaders\n",
                std::string((*gpu)->api()).c_str(),
                std::string((*gpu)->shader_target()).c_str());

    std::vector<Measurement> measurements;
    Context                  c{**gpu, *set, runs, measurements, ""};
    std::vector<Outcome>     outcomes;
    bool                     failed = false;
    for (const Probe& p : k_PROBES) {
        outcomes.push_back(run_probe(c, p));
        failed = failed || outcomes.back().d_status == "FAIL";
    }

    Rows what = {{"generator", "vg_bench"},
                 {"backend", std::string((*gpu)->api())},
                 {"shader_target", std::string((*gpu)->shader_target())},
                 {"runs", std::to_string(runs)}};
    if (text(set->d_hash, "status") == "OK") {
        what.push_back({"shader_hash", text(set->d_hash, "value")});
    } else {
        what.push_back({"shader_hash", ""});
        what.push_back({"shader_hash_reason", text(set->d_hash, "reason")});
    }
    for (const auto& [key, value] : (*gpu)->provenance()) {
        what.push_back({key, value});
    }
    // The compilers that built the shader set: with the device above, the
    // provenance B0 records beside each atomic result -- GPU family, SDK,
    // MSL version (r0-architecture.md section 6).
    for (const auto& [key, value] : set->d_toolchain.items()) {
        const Json& v = value.is_object() ? value.value("value", Json())
                                          : value;
        what.push_back({"toolchain." + key,
                        v.is_string()    ? v.get<std::string>()
                        : v.is_boolean() ? (v.get<bool>() ? "yes" : "no")
                                         : v.dump()});
    }

    Rows probes;
    for (const Outcome& o : outcomes) {
        probes.push_back({o.d_probe,
                          o.d_kernel,
                          o.d_status,
                          o.d_unsupportedBy,
                          o.d_reason,
                          o.d_summary,
                          o.d_notes});
    }

    Rows numbers;
    for (const Measurement& m : measurements) {
        numbers.push_back({m.d_probe,
                           m.d_variant,
                           m.d_metric,
                           m.d_sample < 0 ? std::string()
                                          : std::to_string(m.d_sample),
                           std::format("{}", m.d_value),
                           m.d_unit});
    }

    // run.csv: what ran, on what. probes.csv: one row per probe.
    // measurements.csv: every number, one per row; 'sample' is the run (or
    // the page) of a raw value, and empty for a value derived from them.
    std::error_code ec;
    std::filesystem::create_directories(out, ec);
    const std::tuple<const char*, std::vector<std::string>, const Rows*>
        files[] = {
            {"run.csv", {"key", "value"}, &what},
            {"probes.csv",
             {"probe",
              "kernel",
              "status",
              "unsupported_by",
              "reason",
              "summary",
              "notes"},
             &probes},
            {"measurements.csv",
             {"probe", "variant", "metric", "sample", "value", "unit"},
             &numbers},
        };
    for (const auto& [name, header, rows] : files) {
        const auto file = out / name;
        if (ec || !write_csv(file, header, *rows)) {
            report_error(std::format("cannot write {}", file.string()));
            return 2;
        }
        std::printf("  wrote %s\n", file.string().c_str());
    }
    return failed ? 1 : 0;
}

}  // close unnamed namespace

int main(int argc, char** argv)
{
    // Every failure the bench expects comes back as a Result. Anything else
    // -- the allocator, a filesystem error -- is reported too, rather than
    // ending the run without a word.
    try {
        return run_bench(argc, argv);
    } catch (const std::exception& e) {
        report_error(e.what());
    } catch (...) {
        report_error("unknown exception");
    }
    return 2;
}
