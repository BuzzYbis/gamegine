// bench_metal.cpp                                                    -*-C++-*-
#include <bench_gpu.h>

// std
#include <algorithm>
#include <chrono>
#include <map>
#include <thread>

// third party
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

namespace gamegine {
namespace bench {
namespace {

using Clock = std::chrono::steady_clock;

std::string describe(const NS::Error* error)
{
    return error != nullptr ? error->localizedDescription()->utf8String()
                            : "no error description";
}

struct MetalPipeline {
    NS::SharedPtr<MTL::ComputePipelineState> d_compute;
    NS::SharedPtr<MTL::RenderPipelineState>  d_render;
    bool                                     d_mesh = false;
    MTL::CullMode                            d_cull = MTL::CullModeNone;

    MTL::Size d_threads = MTL::Size(1, 1, 1);
};

using Bound = std::vector<std::pair<NS::UInteger, MTL::Buffer*>>;

class MetalGpu final : public Gpu {
    // DATA
    NS::SharedPtr<MTL::Device>               d_device;
    NS::SharedPtr<MTL::CommandQueue>         d_queue;
    std::vector<NS::SharedPtr<MTL::Buffer>>  d_buffers;
    std::vector<NS::SharedPtr<MTL::Texture>> d_textures;
    std::vector<MetalPipeline>               d_pipelines;

    std::map<std::string, NS::SharedPtr<MTL::Library>> d_libraries;

    NS::SharedPtr<MTL::CounterSampleBuffer> d_samples;
    double                                  d_nsPerTick = 0.0;

    [[nodiscard]] MTL::Buffer* lookup(const Buffer b) const
    {
        return b.d_id < d_buffers.size() ? d_buffers[b.d_id].get() : nullptr;
    }

    [[nodiscard]] MTL::Texture* lookup(const Target t) const
    {
        return t.d_id < d_textures.size() ? d_textures[t.d_id].get() : nullptr;
    }

    [[nodiscard]] const MetalPipeline* lookup(const Pipeline p) const
    {
        return p.d_id < d_pipelines.size() ? &d_pipelines[p.d_id] : nullptr;
    }

    Result<NS::SharedPtr<MTL::Function>>
    function(const std::filesystem::path& artifact, const std::string& name)
    {
        auto library = d_libraries.find(artifact.string());
        if (library == d_libraries.end()) {
            NS::Error*  error = nullptr;
            const auto* url   = NS::URL::fileURLWithPath(
                NS::String::string(artifact.c_str(), NS::UTF8StringEncoding));
            auto loaded = NS::TransferPtr(d_device->newLibrary(url, &error));
            if (!loaded) {
                return fail("cannot load {}: {}",
                            artifact.string(),
                            describe(error));
            }
            library = d_libraries.emplace(artifact.string(), loaded).first;
        }
        auto f = NS::TransferPtr(library->second->newFunction(
            NS::String::string(name.c_str(), NS::UTF8StringEncoding)));
        if (!f) {
            return fail("{} has no function '{}'", artifact.string(), name);
        }
        return f;
    }

    // The buffers 'bindings' name, or which one is unknown.
    [[nodiscard]] Result<Bound>
    resolve(const std::vector<Binding>& bindings) const
    {
        Bound out;
        for (const Binding& b : bindings) {
            MTL::Buffer* buffer = lookup(b.d_buffer);
            if (buffer == nullptr) {
                return fail("binding {} names unknown buffer {}",
                            b.d_index,
                            b.d_buffer.d_id);
            }
            out.emplace_back(b.d_index, buffer);
        }
        return out;
    }

    // Each 'encode' checks its command before opening an encoder, so a
    // failed command leaves no encoder open, then records one pass. 'start'
    // and 'end' are the timestamp samples the pass takes, or
    // MTL::CounterDontSample.
    Result<void> encode(MTL::CommandBuffer* commands,
                        const Dispatch&     dispatch,
                        const NS::UInteger  start,
                        const NS::UInteger  end)
    {
        const MetalPipeline* pipeline = lookup(dispatch.d_pipeline);
        if (pipeline == nullptr || !pipeline->d_compute) {
            return fail("dispatch of pipeline {}, which is not a compute "
                        "pipeline",
                        dispatch.d_pipeline.d_id);
        }
        auto bound = resolve(dispatch.d_bindings);
        if (!bound) {
            return std::unexpected(bound.error());
        }

        const MTL::ComputePassDescriptor* pass =
            MTL::ComputePassDescriptor::computePassDescriptor();
        auto* samples = pass->sampleBufferAttachments()->object(0);
        samples->setSampleBuffer(d_samples.get());
        samples->setStartOfEncoderSampleIndex(start);
        samples->setEndOfEncoderSampleIndex(end);

        MTL::ComputeCommandEncoder* encoder = commands->computeCommandEncoder(
            pass);
        encoder->setComputePipelineState(pipeline->d_compute.get());
        for (const auto& [index, buffer] : *bound) {
            encoder->setBuffer(buffer, 0, index);
        }
        const auto& [x, y, z] = dispatch.d_groups;
        encoder->dispatchThreadgroups(MTL::Size(x, y, z), pipeline->d_threads);
        encoder->endEncoding();
        return {};
    }

    // Open a render pass that clears 'target' to transparent black. Apple
    // GPUs sample timestamps only at stage boundaries: the start of the
    // vertex (or mesh) stage and the end of the fragment stage bracket it.
    MTL::RenderCommandEncoder* begin_pass(MTL::CommandBuffer* commands,
                                          MTL::Texture*       target,
                                          const NS::UInteger  start,
                                          const NS::UInteger  end)
    {
        const MTL::RenderPassDescriptor* pass =
            MTL::RenderPassDescriptor::renderPassDescriptor();
        auto* colour = pass->colorAttachments()->object(0);
        colour->setTexture(target);
        colour->setLoadAction(MTL::LoadActionClear);
        colour->setStoreAction(MTL::StoreActionStore);
        colour->setClearColor(MTL::ClearColor(0.0, 0.0, 0.0, 0.0));

        auto* samples = pass->sampleBufferAttachments()->object(0);
        samples->setSampleBuffer(d_samples.get());
        samples->setStartOfVertexSampleIndex(start);
        samples->setEndOfVertexSampleIndex(MTL::CounterDontSample);
        samples->setStartOfFragmentSampleIndex(MTL::CounterDontSample);
        samples->setEndOfFragmentSampleIndex(end);

        MTL::RenderCommandEncoder* encoder = commands->renderCommandEncoder(
            pass);
        encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
        return encoder;
    }

    Result<void> encode(MTL::CommandBuffer* commands,
                        const Draw&         draw,
                        const NS::UInteger  start,
                        const NS::UInteger  end)
    {
        const MetalPipeline* pipeline = lookup(draw.d_pipeline);
        if (pipeline == nullptr || !pipeline->d_render || pipeline->d_mesh) {
            return fail("draw with pipeline {}, which is not a vertex "
                        "pipeline",
                        draw.d_pipeline.d_id);
        }
        MTL::Texture* target = lookup(draw.d_target);
        if (target == nullptr) {
            return fail("draw into unknown target {}", draw.d_target.d_id);
        }
        auto bound = resolve(draw.d_bindings);
        if (!bound) {
            return std::unexpected(bound.error());
        }

        MTL::RenderCommandEncoder* encoder = begin_pass(commands,
                                                        target,
                                                        start,
                                                        end);
        encoder->setRenderPipelineState(pipeline->d_render.get());
        encoder->setCullMode(pipeline->d_cull);
        for (const auto& [index, buffer] : *bound) {
            encoder->setVertexBuffer(buffer, 0, index);
            encoder->setFragmentBuffer(buffer, 0, index);
        }
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                NS::UInteger(0),
                                draw.d_vertices,
                                draw.d_instances);
        encoder->endEncoding();
        return {};
    }

    Result<void> encode(MTL::CommandBuffer* commands,
                        const MeshDraw&     draw,
                        const NS::UInteger  start,
                        const NS::UInteger  end)
    {
        const MetalPipeline* pipeline = lookup(draw.d_pipeline);
        if (pipeline == nullptr || !pipeline->d_render || !pipeline->d_mesh) {
            return fail("mesh draw with pipeline {}, which is not a mesh "
                        "pipeline",
                        draw.d_pipeline.d_id);
        }
        MTL::Texture* target = lookup(draw.d_target);
        if (target == nullptr) {
            return fail("mesh draw into unknown target {}",
                        draw.d_target.d_id);
        }
        auto bound = resolve(draw.d_bindings);
        if (!bound) {
            return std::unexpected(bound.error());
        }

        MTL::RenderCommandEncoder* encoder = begin_pass(commands,
                                                        target,
                                                        start,
                                                        end);
        encoder->setRenderPipelineState(pipeline->d_render.get());
        encoder->setCullMode(pipeline->d_cull);
        for (const auto& [index, buffer] : *bound) {
            encoder->setMeshBuffer(buffer, 0, index);
            encoder->setFragmentBuffer(buffer, 0, index);
        }
        const auto& [x, y, z] = draw.d_groups;
        encoder->drawMeshThreadgroups(MTL::Size(x, y, z),
                                      MTL::Size(1, 1, 1),  // no object stage
                                      pipeline->d_threads);
        encoder->endEncoding();
        return {};
    }

  public:
    // CREATORS
    explicit MetalGpu(NS::SharedPtr<MTL::Device> device)
    : d_device(std::move(device))
    , d_queue(NS::TransferPtr(d_device->newCommandQueue()))
    {
    }

    // ACCESSORS
    [[nodiscard]] std::string_view api() const override
    {
        return "metal";
    }

    [[nodiscard]] std::string_view shader_target() const override
    {
        return "msl";
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::string>>
    provenance() const override
    {
        // Apple families from the most capable down: the first one the
        // device supports is its family (an M3 Pro is apple9).
        static constexpr std::pair<MTL::GPUFamily, const char*> k_APPLE[] = {
            {MTL::GPUFamilyApple10, "apple10"},
            {MTL::GPUFamilyApple9, "apple9"},
            {MTL::GPUFamilyApple8, "apple8"},
            {MTL::GPUFamilyApple7, "apple7"},
            {MTL::GPUFamilyApple6, "apple6"},
            {MTL::GPUFamilyApple5, "apple5"},
            {MTL::GPUFamilyApple4, "apple4"},
            {MTL::GPUFamilyApple3, "apple3"},
            {MTL::GPUFamilyApple2, "apple2"},
            {MTL::GPUFamilyApple1, "apple1"},
        };

        std::string appleFamily = "none";
        for (const auto& [family, name] : k_APPLE) {
            if (d_device->supportsFamily(family)) {
                appleFamily = name;
                break;
            }
        }

        // operatingSystemVersionString() is autoreleased: the strings are
        // copied into 'out' before the pool drains.
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        const NS::String*    os   = NS::ProcessInfo::processInfo()
                                   ->operatingSystemVersionString();
        std::vector<std::pair<std::string, std::string>> out = {
            {"api", "metal"},
            {"device_name", d_device->name()->utf8String()},
            {"gpu_family", appleFamily},
            {
                "metal3",
                d_device->supportsFamily(MTL::GPUFamilyMetal3) ? "yes" : "no",
            },
            {
                "metal4",
                d_device->supportsFamily(MTL::GPUFamilyMetal4) ? "yes" : "no",
            },
            {"macos_version", os != nullptr ? os->utf8String() : ""},
        };
        pool->release();
        return out;
    }

    [[nodiscard]] bool supports(const Feature feature) const override
    {
        switch (feature) {
        case Feature::e_MESH_SHADERS:
            return d_device->supportsFamily(MTL::GPUFamilyMetal3);
        case Feature::e_INT64_BUFFER_ATOMICS:
        case Feature::e_INT64_FRAGMENT_ATOMICS: return true;
        }
        return false;
    }

    // MANIPULATORS

    Result<Buffer> create_buffer(const std::size_t bytes) override
    {
        auto buffer = NS::TransferPtr(d_device->newBuffer(
            std::max<std::size_t>(bytes, 1), MTL::ResourceStorageModeShared));

        if (!buffer) {
            return fail("cannot allocate a {}-byte buffer (the device allows "
                        "up to {})",
                        bytes,
                        d_device->maxBufferLength());
        }
        d_buffers.push_back(std::move(buffer));
        return Buffer{static_cast<std::uint32_t>(d_buffers.size() - 1)};
    }

    Result<void> upload(Buffer                           to,
                        const std::span<const std::byte> data) override
    {
        MTL::Buffer* buffer = lookup(to);
        if (buffer == nullptr) {
            return fail("upload into unknown buffer {}", to.d_id);
        }
        if (data.size() > buffer->length()) {
            return fail("upload of {} bytes into a {}-byte buffer",
                        data.size(),
                        buffer->length());
        }
        std::memcpy(buffer->contents(), data.data(), data.size());
        return {};
    }

    Result<void> download(Buffer from, std::span<std::byte> data) override
    {
        MTL::Buffer* buffer = lookup(from);
        if (buffer == nullptr) {
            return fail("download from unknown buffer {}", from.d_id);
        }
        if (data.size() > buffer->length()) {
            return fail("download of {} bytes from a {}-byte buffer",
                        data.size(),
                        buffer->length());
        }
        std::memcpy(data.data(), buffer->contents(), data.size());
        return {};
    }

    Result<Target> create_target(const std::uint32_t width,
                                 const std::uint32_t height) override
    {
        if (width == 0 || height == 0) {
            return fail("Cannot create texture of size {}x{}", width, height);
        }

        // Maximum allowed size on macos for texture is 16384
        if (width > 16384 || height > 16384) {
            return fail("Texture has width or height ({}x{}) greater than the "
                        "maximum allowed size of 16384.",
                        width,
                        height);
        }
        MTL::TextureDescriptor* textureDescriptor =
            MTL::TextureDescriptor::alloc()->init();
        textureDescriptor->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
        textureDescriptor->setWidth(width);
        textureDescriptor->setHeight(height);
        textureDescriptor->setUsage(MTL::TextureUsageRenderTarget);
        textureDescriptor->setStorageMode(MTL::StorageModePrivate);
        auto texture = NS::TransferPtr(
            d_device->newTexture(textureDescriptor));
        textureDescriptor->release();

        if (!texture) {
            return fail("cannot create a {}x{} render target", width, height);
        }
        d_textures.push_back(std::move(texture));
        return Target{static_cast<std::uint32_t>(d_textures.size() - 1)};
    }

    Result<void> read_target(const Target               target,
                             const std::span<std::byte> rgba8) override
    {
        const auto* texture = lookup(target);
        if (texture == nullptr) {
            return fail("read from unknown target {}", target.d_id);
        }

        const auto rowBytes = texture->width() * 4;
        const auto bytes    = rowBytes * texture->height();

        // The whole image, always: another size means the caller has the
        // wrong dimensions.
        if (rgba8.size() != bytes) {
            return fail("read of a {}x{} target into {} bytes, not {}",
                        texture->width(),
                        texture->height(),
                        rgba8.size(),
                        bytes);
        }

        // commandBuffer() and blitCommandEncoder() are autoreleased; the
        // pool drains on every return below.
        const auto pool = NS::TransferPtr(
            NS::AutoreleasePool::alloc()->init());

        // The target is Private: tiled, maybe compressed, and with no CPU
        // mapping. The GPU copies it, as linear rows, into a Shared buffer.
        const auto staging = NS::TransferPtr(
            d_device->newBuffer(bytes, MTL::ResourceStorageModeShared));
        if (!staging) {
            return fail("cannot allocate a {}-byte readback buffer", bytes);
        }

        // Queue order puts this after the draws of the last 'run', which has
        // already waited for them.
        MTL::CommandBuffer*      commands = d_queue->commandBuffer();
        MTL::BlitCommandEncoder* blit     = commands->blitCommandEncoder();
        blit->copyFromTexture(texture,
                              0,  // slice
                              0,  // mip level
                              MTL::Origin(0, 0, 0),
                              MTL::Size(texture->width(),
                                        texture->height(),
                                        1),
                              staging.get(),
                              0,  // offset in the buffer
                              rowBytes,
                              bytes);
        blit->endEncoding();
        commands->commit();
        commands->waitUntilCompleted();

        if (commands->status() != MTL::CommandBufferStatusCompleted) {
            const NS::Error* error = commands->error();
            return fail("readback of target {} failed: {}",
                        target.d_id,
                        error != nullptr
                            ? error->localizedDescription()->utf8String()
                            : "no error reported");
        }

        std::memcpy(rgba8.data(), staging->contents(), bytes);
        return {};
    }

    Result<Pipeline> create_pipeline(const PipelineDesc& desc) override
    {
        // The file URL, the names and the errors are autoreleased.
        const auto pool = NS::TransferPtr(
            NS::AutoreleasePool::alloc()->init());

        if (desc.d_mesh && desc.d_fragment.empty()) {
            return fail("mesh pipeline '{}' has no fragment entry point",
                        desc.d_entry);
        }
        auto entry = function(desc.d_artifact, desc.d_entry);
        if (!entry) {
            return std::unexpected(entry.error());
        }

        MetalPipeline pipeline;
        const auto& [x, y, z] = desc.d_threads;
        pipeline.d_threads    = MTL::Size(x, y, z);
        NS::Error* error      = nullptr;

        if (desc.d_fragment.empty()) {
            pipeline.d_compute = NS::TransferPtr(
                d_device->newComputePipelineState(entry->get(), &error));
            if (!pipeline.d_compute) {
                return fail("cannot create the '{}' compute pipeline: {}",
                            desc.d_entry,
                            describe(error));
            }
            const NS::UInteger threads = NS::UInteger(x) * y * z;
            const NS::UInteger allowed = pipeline.d_compute
                                             ->maxTotalThreadsPerThreadgroup();
            if (threads == 0 || threads > allowed) {
                return fail("'{}' needs {} threads per group, the pipeline "
                            "allows 1 to {}",
                            desc.d_entry,
                            threads,
                            allowed);
            }
        } else {
            auto fragment = function(desc.d_artifact, desc.d_fragment);
            if (!fragment) {
                return std::unexpected(fragment.error());
            }
            if (desc.d_mesh) {
                const auto d = NS::TransferPtr(
                    MTL::MeshRenderPipelineDescriptor::alloc()->init());
                d->setMeshFunction(entry->get());
                d->setFragmentFunction(fragment->get());
                d->colorAttachments()->object(0)->setPixelFormat(
                    MTL::PixelFormatRGBA8Unorm);
                pipeline.d_render = NS::TransferPtr(
                    d_device->newRenderPipelineState(
                        d.get(), MTL::PipelineOptionNone, nullptr, &error));
            } else {
                auto d = NS::TransferPtr(
                    MTL::RenderPipelineDescriptor::alloc()->init());
                d->setVertexFunction(entry->get());
                d->setFragmentFunction(fragment->get());
                d->colorAttachments()->object(0)->setPixelFormat(
                    MTL::PixelFormatRGBA8Unorm);
                pipeline.d_render = NS::TransferPtr(
                    d_device->newRenderPipelineState(d.get(), &error));
            }
            if (!pipeline.d_render) {
                return fail("cannot create the '{}' + '{}' pipeline: {}",
                            desc.d_entry,
                            desc.d_fragment,
                            describe(error));
            }
            pipeline.d_mesh = desc.d_mesh;
            pipeline.d_cull = desc.d_cullBack ? MTL::CullModeBack
                                              : MTL::CullModeNone;
        }

        d_pipelines.push_back(std::move(pipeline));
        return Pipeline{static_cast<std::uint32_t>(d_pipelines.size() - 1)};
    }

    Result<Timing> run(const std::span<const Command> commands) override
    {
        if (commands.empty()) {
            return fail("an empty run has nothing to time");
        }

        // The command buffer, the encoders, the pass descriptors and the
        // resolved samples are autoreleased. A command-line tool has no run
        // loop to drain them: this pool does, on every return.
        const auto pool = NS::TransferPtr(
            NS::AutoreleasePool::alloc()->init());

        // One pass per command, in order: Metal orders the passes of a
        // command buffer by the buffers and textures they share. Sample 0 is
        // the first pass's start, sample 1 the last pass's end.
        MTL::CommandBuffer* buffer = d_queue->commandBuffer();
        for (std::size_t i = 0; i < commands.size(); ++i) {
            const NS::UInteger start   = i == 0 ? 0 : MTL::CounterDontSample;
            const NS::UInteger end     = i + 1 == commands.size()
                                             ? 1
                                             : MTL::CounterDontSample;
            auto               encoded = std::visit(
                [&](const auto& command) {
                    return encode(buffer, command, start, end);
                },
                commands[i]);
            if (!encoded) {
                return fail("command {}: {}", i, encoded.error());
            }
        }
        buffer->commit();
        buffer->waitUntilCompleted();
        if (buffer->status() != MTL::CommandBufferStatusCompleted) {
            return fail("the command buffer failed: {}",
                        describe(buffer->error()));
        }

        const NS::Data* data = d_samples->resolveCounterRange(NS::Range(0, 2));
        MTL::CounterResultTimestamp stamps[2];
        if (data == nullptr || data->length() < sizeof(stamps)) {
            return fail("the GPU timestamps could not be resolved");
        }
        std::memcpy(stamps, data->mutableBytes(), sizeof(stamps));
        if (stamps[0].timestamp == MTL::CounterErrorValue ||
            stamps[1].timestamp == MTL::CounterErrorValue) {
            return fail("the GPU did not record a timestamp");
        }
        if (stamps[1].timestamp < stamps[0].timestamp) {
            return fail("the run ends before it starts");
        }

        Timing timing;
        timing.d_gpuNs = static_cast<double>(stamps[1].timestamp -
                                             stamps[0].timestamp) *
                         d_nsPerTick;
        timing.d_independentNs = (buffer->GPUEndTime() -
                                  buffer->GPUStartTime()) *
                                 1.0e9;
        return timing;
    }

    // Create the timestamp sample buffer and measure the GPU clock's
    // period. Called once, by 'create_metal'.
    Result<void> calibrate()
    {
        const auto pool = NS::TransferPtr(
            NS::AutoreleasePool::alloc()->init());

        if (!d_device->supportsCounterSampling(
                MTL::CounterSamplingPointAtStageBoundary)) {
            return fail("{} cannot sample timestamps at stage boundaries",
                        d_device->name()->utf8String());
        }
        const MTL::CounterSet* timestamps = nullptr;
        const NS::Array*       sets       = d_device->counterSets();
        for (NS::UInteger i = 0; sets != nullptr && i < sets->count(); ++i) {
            const auto* set = sets->object<MTL::CounterSet>(i);
            if (set->name()->isEqualToString(MTL::CommonCounterSetTimestamp)) {
                timestamps = set;
            }
        }
        if (timestamps == nullptr) {
            return fail("{} has no timestamp counter set",
                        d_device->name()->utf8String());
        }

        const auto desc = NS::TransferPtr(
            MTL::CounterSampleBufferDescriptor::alloc()->init());
        desc->setCounterSet(timestamps);
        desc->setStorageMode(MTL::StorageModeShared);
        desc->setSampleCount(2);
        NS::Error* error = nullptr;
        d_samples        = NS::TransferPtr(
            d_device->newCounterSampleBuffer(desc.get(), &error));
        if (!d_samples) {
            return fail("cannot create a timestamp sample buffer: {}",
                        describe(error));
        }

        // GPU ticks over 200 ms of host clock. The host interval also holds
        // the two sampleTimestamps calls: microseconds, against 200 ms.
        MTL::Timestamp cpu0  = 0;
        MTL::Timestamp gpu0  = 0;
        MTL::Timestamp cpu1  = 0;
        MTL::Timestamp gpu1  = 0;
        const auto     host0 = Clock::now();
        d_device->sampleTimestamps(&cpu0, &gpu0);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        d_device->sampleTimestamps(&cpu1, &gpu1);
        const auto host1 = Clock::now();
        if (gpu1 <= gpu0) {
            return fail("the GPU clock did not advance in 200 ms");
        }
        d_nsPerTick = std::chrono::duration<double, std::nano>(host1 - host0)
                          .count() /
                      static_cast<double>(gpu1 - gpu0);
        return {};
    }
};

}  // close unnamed namespace

Result<std::unique_ptr<Gpu>> create_metal()
{
    auto device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
    if (!device) {
        return fail("no Metal device");
    }
    auto metal = std::make_unique<MetalGpu>(std::move(device));
    if (auto calibrated = metal->calibrate(); !calibrated) {
        return std::unexpected(calibrated.error());
    }
    return metal;
}

}  // close namespace bench
}  // close namespace gamegine
