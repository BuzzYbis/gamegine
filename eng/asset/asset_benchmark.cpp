// asset_benchmark.cpp                                                -*-C++-*-
#include <asset/asset_benchmark.h>

// std
#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

// posix
#include <sys/resource.h>
#include <sys/utsname.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

// asset
#include <asset/asset_manager.h>

// core
#include <core/core_buildinfo.h>
#include <core/core_profiler.h>
#include <core/core_steptimer.h>

// rhi
#include <rhi/rhi_contextprotocol.h>

namespace eng::asset {

namespace {

/// Version of the CSV layout. Bump it whenever a column is added, removed or
/// redefined, so that a reader can tell the rows of one layout from another.
constexpr int k_SCHEMA_VERSION = 1;

/// The phases the total duration is split into, as pairs of the CSV column
/// and of the profiling scope it reports. Adding a phase means adding an
/// entry here and bumping 'k_SCHEMA_VERSION'.
constexpr std::array<std::pair<const char*, const char*>, 9> k_PHASES = {{
    {"model_ms", "Load Model"},
    {"parse_ms", "Load Parse"},
    {"geometry_ms", "Load Geometry"},
    {"material_data_ms", "Load Material Data"},
    {"image_scan_ms", "Load Image Scan"},
    {"node_graph_ms", "Load Node Graph"},
    {"materials_ms", "Load Materials"},
    {"textures_ms", "Load Textures"},
    {"buffers_ms", "Load Buffers"},
}};

/// The counters reported, as pairs of the CSV column and of the profiling
/// counter it reports.
constexpr std::array<std::pair<const char*, const char*>, 7> k_COUNTERS = {{
    {"meshes", "Loaded Meshes"},
    {"triangles", "Loaded Triangles"},
    {"images", "Loaded Images"},
    {"textures_uploaded", "Textures Uploaded"},
    {"textures_cached", "Textures Cached"},
    {"geometry_kb", "Geometry KB"},
    {"image_kb", "Image KB"},
}};

/// Result of a single load.
struct Sample {
    /// Duration of the load, in milliseconds.
    float totalMs = 0.0f;

    /// Samples the profiler gathered during the load.
    core::FrameRecord record = {};
};

/// Return the current UTC time in ISO 8601 form.
std::string utcTimestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm           utc = {};

    gmtime_r(&now, &utc);

    std::array<char, 32> buffer = {};
    std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &utc);

    return buffer.data();
}

/// Return the name and release of the running operating system.
std::string osName()
{
    utsname info = {};

    if (uname(&info) != 0) {
        return "unknown";
    }

    return std::string(info.sysname) + " " + info.release;
}

/// Return the model of the processor, or "unknown" if it cannot be read.
std::string cpuName()
{
#ifdef __APPLE__
    std::size_t size = 0;

    if (sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0) !=
            0 ||
        size == 0) {
        return "unknown";
    }

    std::string name(size, '\0');

    if (sysctlbyname("machdep.cpu.brand_string",
                     name.data(),
                     &size,
                     nullptr,
                     0) != 0) {
        return "unknown";
    }

    name.resize(std::strlen(name.c_str()));
    return name;
#else
    return "unknown";
#endif
}

/// Return the peak resident set size of this process, in megabytes.
double peakRssMb()
{
    rusage usage = {};

    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0.0;
    }

    // 'ru_maxrss' is expressed in bytes on Apple platforms, and in kilobytes
    // everywhere else.
#ifdef __APPLE__
    return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
    return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
}

/// Return the size of the file at the specified 'path', or 0 if it cannot be
/// read.
uintmax_t fileSize(const std::string& path)
{
    std::error_code error;
    const uintmax_t size = std::filesystem::file_size(path, error);

    return error ? 0 : size;
}

/// Return the specified 'field' quoted if it holds a character that would
/// break the CSV, and unchanged otherwise.
std::string csvField(const std::string& field)
{
    if (field.find_first_of(",\"\n") == std::string::npos) {
        return field;
    }

    std::string quoted = "\"";

    for (const char character : field) {
        if (character == '"') {
            quoted += '"';
        }
        quoted += character;
    }

    return quoted + '"';
}

/// Return the value of the counter named 'name' in the specified 'record',
/// or 0 if no such counter was ever registered.
int64_t counterOf(const core::FrameRecord& record, const char* name)
{
    const core::Profiler& profiler = core::Profiler::instance();

    for (uint16_t id = 0; id < profiler.counterCount(); ++id) {
        if (std::strcmp(profiler.counterName(id), name) == 0) {
            return record.counters[id];
        }
    }

    return 0;
}

/// Return the CPU time of the scope named 'name' in the specified 'record',
/// or 0 if no such scope was ever registered.
float scopeMsOf(const core::FrameRecord& record, const char* name)
{
    const core::Profiler& profiler = core::Profiler::instance();

    for (uint16_t id = 0; id < profiler.scopeCount(); ++id) {
        if (std::strcmp(profiler.scopeName(id), name) == 0) {
            return record.cpuMs[id];
        }
    }

    return 0.0f;
}

/// Write the header of the CSV into the specified 'out'.
void writeHeader(std::ofstream& out)
{
    out << "schema_version,timestamp,version,commit,branch,dirty,build_type"
        << ",compiler,os,cpu,gpu,scene,asset_bytes,runs"
        << ",cold_ms,min_ms,med_ms";

    for (const auto& [column, scope] : k_PHASES) {
        out << ',' << column;
    }

    for (const auto& [column, counter] : k_COUNTERS) {
        out << ',' << column;
    }

    out << ",peak_rss_mb\n";
}

}  // close unnamed namespace

// CLASS METHODS

bool LoadBenchmark::run(const Config&               config,
                        rhi::ContextProtocol* const context,
                        rnd::Renderer* const        renderer)
{
    if (context == nullptr || renderer == nullptr || config.runs == 0) {
        return false;
    }

    core::Profiler& profiler = core::Profiler::instance();

    // The rows must describe the loads and nothing else.
    profiler.clearHistory();

    std::vector<Sample> samples;
    samples.reserve(config.runs);

    for (uint32_t run = 0; run < config.runs; ++run) {
        // A fresh manager per run: the one of the previous run would answer
        // out of its caches and measure nothing.
        AssetManager manager(context, renderer);

        profiler.beginFrame();
        const auto start = core::StepTimer::Clock::now();

        const LoadedModel model = manager.loadMesh(config.scenePath);

        if (model.meshes.empty()) {
            std::cerr << "[Benchmark] Failed to load " << config.scenePath
                      << "\n";
            return false;
        }

        const std::vector<rnd::MeshBatch> batches = manager.buildMeshBatches(
            model);

        const std::chrono::duration<float, std::milli> elapsed =
            core::StepTimer::Clock::now() - start;

        profiler.endFrame(elapsed.count());

        samples.push_back({elapsed.count(), profiler.lastFrame()});

        std::cout << "[Benchmark] run " << (run + 1) << '/' << config.runs
                  << ": " << elapsed.count() << " ms, " << batches.size()
                  << " batches\n";

        // The manager dies here, taking its GPU resources with it; the
        // device has to be done with them before the next run allocates.
        context->waitIdle();
    }

    std::vector<float> durations;
    durations.reserve(samples.size());

    for (const Sample& sample : samples) {
        durations.push_back(sample.totalMs);
    }

    std::sort(durations.begin(), durations.end());

    const float coldMs = samples.front().totalMs;
    const float minMs  = durations.front();
    const float medMs  = durations[durations.size() / 2];

    // The breakdown is that of the fastest run, so that the phases and the
    // 'min_ms' they add up to describe one and the same load.
    const Sample& fastest = *std::min_element(
        samples.begin(),
        samples.end(),
        [](const Sample& left, const Sample& right) {
            return left.totalMs < right.totalMs;
        });

    const bool isNewFile = !std::filesystem::exists(config.csvPath) ||
                           fileSize(config.csvPath) == 0;

    std::ofstream out(config.csvPath, std::ios::app);

    if (!out) {
        std::cerr << "[Benchmark] Cannot write " << config.csvPath << "\n";
        return false;
    }

    if (isNewFile) {
        writeHeader(out);
    }

    out << k_SCHEMA_VERSION << ',' << utcTimestamp() << ',' << core::k_VERSION
        << ',' << core::k_GIT_COMMIT << ',' << core::k_GIT_BRANCH << ','
        << (core::k_GIT_DIRTY ? "true" : "false") << ',' << core::k_BUILD_TYPE
        << ',' << csvField(core::k_COMPILER) << ',' << csvField(osName())
        << ',' << csvField(cpuName()) << ',' << csvField(context->deviceName())
        << ',' << csvField(config.scenePath) << ','
        << fileSize(config.scenePath) << ',' << config.runs << ',' << coldMs
        << ',' << minMs << ',' << medMs;

    for (const auto& [column, scope] : k_PHASES) {
        out << ',' << scopeMsOf(fastest.record, scope);
    }

    for (const auto& [column, counter] : k_COUNTERS) {
        out << ',' << counterOf(fastest.record, counter);
    }

    out << ',' << peakRssMb() << '\n';

    std::cout << "[Benchmark] cold " << coldMs << " ms, min " << minMs
              << " ms, median " << medMs << " ms -> " << config.csvPath
              << "\n";

    return true;
}

}  // close package namespace
