// bench_gpu.h                                                      -*-C++-*-
#ifndef INCLUDED_BENCH_GPU_H
#define INCLUDED_BENCH_GPU_H

//@PURPOSE: Provide the GPU interface vg_bench's probes are written against.
//
//@CLASSES:
//  gamegine::bench::Gpu: a GPU that runs precompiled kernels, one per API
//
//@DESCRIPTION: vg_bench has one implementation of this interface per API --
// 'bench_vulkan.cpp' and 'bench_metal.cpp' -- and everything else lives in
// one shared file, 'vg_bench.cpp': the probes, their parameters, the host
// checks and the report. Both backends therefore run exactly the same work,
// which the per-workload Apple-to-Linux ratios of B0 depend on.
//
// The interface is deliberately small, and it is not the engine's RHI:
// handles are plain ids, every call returns a 'Result', and 'run' waits for
// the GPU before returning. Nothing is compiled here: a pipeline is created
// from the artifact the shader toolchain built ('.spv' or '.metallib').
// Bindings arrive already resolved, by the shared code, to the index each
// backend expects -- the Vulkan binding in descriptor set 0, or the Metal
// buffer index -- as build/shaders/index.json records them.

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace gamegine {
namespace bench {

// Errors are values: a probe that cannot run is a result, not a crash.
template <class t_TYPE> using Result = std::expected<t_TYPE, std::string>;

template <class... t_ARGS>
std::unexpected<std::string> fail(std::format_string<t_ARGS...> format,
                                  t_ARGS&&... args)
{
    return std::unexpected<std::string>(
        std::format(format, std::forward<t_ARGS>(args)...));
}

// Handles, owned by the backend that created them.
struct Buffer {
    std::uint32_t d_id;
};

struct Target {  // an offscreen RGBA8 colour target, no depth
    std::uint32_t d_id;
};

struct Pipeline {
    std::uint32_t d_id;
};

struct PipelineDesc {
    std::filesystem::path d_artifact;  // the .spv or .metallib to load
    std::string           d_entry;     // compute, vertex or mesh entry point
    std::string           d_fragment;  // fragment entry point; empty: compute
    bool                  d_mesh     = false;  // d_entry is a mesh shader
    bool                  d_cullBack = false;  // else nothing is culled

    // Threads per group. Metal needs it when dispatching; for compute it
    // comes from the index, for mesh shaders the caller knows it (Slang's
    // reflection reports none).
    std::array<std::uint32_t, 3> d_threads = {1, 1, 1};
};

struct Binding {
    std::uint32_t d_index;  // Vulkan binding in set 0, or Metal buffer index
    Buffer        d_buffer;
};

struct Dispatch {
    Pipeline                     d_pipeline;
    std::vector<Binding>         d_bindings;
    std::array<std::uint32_t, 3> d_groups;
};

// Both draws clear their target to transparent black first. Front faces
// are counter-clockwise, and the Vulkan backend flips its viewport so that
// both APIs draw the same image from the same clip coordinates.
struct Draw {
    Pipeline             d_pipeline;
    std::vector<Binding> d_bindings;
    Target               d_target;
    std::uint32_t        d_vertices;
    std::uint32_t        d_instances;
};

struct MeshDraw {
    Pipeline             d_pipeline;
    std::vector<Binding> d_bindings;
    Target               d_target;

    // Keep each dimension at or under 65535: Metal silently drops a larger
    // mesh grid, and nothing runs.
    std::array<std::uint32_t, 3> d_groups;
};

using Command = std::variant<Dispatch, Draw, MeshDraw>;

// The timing of one 'run', over all its commands.
struct Timing {
    double d_gpuNs = 0.0;  // GPU timestamps: first start to last end

    // A second, independent measure of the same work, for gate B0.6:
    // GPUEndTime - GPUStartTime on Metal; on Vulkan, host time from
    // vkQueueSubmit returning to the fence signalling.
    double d_independentNs = 0.0;
};

enum class Feature {
    e_INT64_BUFFER_ATOMICS,
    e_INT64_FRAGMENT_ATOMICS,
    e_MESH_SHADERS
};

class Gpu {
  public:
    virtual ~Gpu() = default;

    // "vulkan" or "metal", and the shader target it loads: "spirv" or "msl".
    virtual std::string_view api() const           = 0;
    virtual std::string_view shader_target() const = 0;

    // What the report records about the device: name, driver, versions,
    // GPU family... as key and value.
    virtual std::vector<std::pair<std::string, std::string>>
    provenance() const = 0;

    // Whether the device can run a probe needing 'feature'. A probe the
    // device cannot run is reported UNSUPPORTED, not failed.
    virtual bool supports(Feature feature) const = 0;

    virtual Result<Buffer> create_buffer(std::size_t bytes)               = 0;
    virtual Result<void>   upload(Buffer                     to,
                                  std::span<const std::byte> data)        = 0;
    virtual Result<void> download(Buffer from, std::span<std::byte> data) = 0;

    virtual Result<Target> create_target(std::uint32_t width,
                                         std::uint32_t height)     = 0;
    virtual Result<void>   read_target(Target               from,
                                       std::span<std::byte> rgba8) = 0;

    virtual Result<Pipeline> create_pipeline(const PipelineDesc& desc) = 0;

    // Run 'commands' in order, each after the previous one's writes, wait
    // for them, and return their timing.
    virtual Result<Timing> run(std::span<const Command> commands) = 0;
};

Result<std::unique_ptr<Gpu>> create_vulkan();

#ifdef __APPLE__
Result<std::unique_ptr<Gpu>> create_metal();
#else
inline Result<std::unique_ptr<Gpu>> create_metal()
{
    return fail("Metal exists only on Apple platforms");
}
#endif

}  // close namespace bench
}  // close namespace gamegine

#endif
