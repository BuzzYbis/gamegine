// core_profiler.cpp                                                  -*-C++-*-
#include <core/core_profiler.h>

// std
#include <algorithm>
#include <cstring>

namespace eng::core {

namespace {

/// Return the statistics of the specified 'count' first samples of the
/// specified 'samples' buffer. The buffer is reordered in the process.
ScopeStats statsOf(float* samples, const uint32_t count)
{
    ScopeStats stats;
    stats.sampleCount = count;

    if (count == 0) {
        return stats;
    }

    double sum = 0.0;
    for (uint32_t i = 0; i < count; ++i) {
        sum += samples[i];
    }

    stats.avgMs = static_cast<float>(sum / count);
    stats.minMs = *std::min_element(samples, samples + count);
    stats.maxMs = *std::max_element(samples, samples + count);

    // The 99th percentile: the value only one sample in a hundred exceeds.
    // A partial ordering is enough here, a full sort is not needed.
    const uint32_t rank = std::min(count - 1, (count * 99) / 100);
    std::nth_element(samples, samples + rank, samples + count);
    stats.p99Ms = samples[rank];

    return stats;
}

}  // close unnamed namespace

// CLASS METHODS

Profiler& Profiler::instance()
{
    static Profiler s_profiler;
    return s_profiler;
}

// MANIPULATORS

ScopeId Profiler::registerScope(const char* name)
{
    for (uint16_t i = 0; i < d_scopeCount; ++i) {
        // Call sites overwhelmingly pass the very same literal, so the
        // pointer comparison short-circuits the string comparison.
        if (d_scopes[i].name_p == name ||
            std::strcmp(d_scopes[i].name_p, name) == 0) {
            return i;
        }
    }

    if (d_scopeCount == k_MAX_SCOPES) {
        return k_INVALID_ID;
    }

    const ScopeId id    = d_scopeCount++;
    d_scopes[id].name_p = name;
    d_scopes[id].depth  = 0;

    return id;
}

CounterId Profiler::registerCounter(const char* name)
{
    for (uint16_t i = 0; i < d_counterCount; ++i) {
        if (d_counterNames[i] == name ||
            std::strcmp(d_counterNames[i], name) == 0) {
            return i;
        }
    }

    if (d_counterCount == k_MAX_COUNTERS) {
        return k_INVALID_ID;
    }

    const CounterId id = d_counterCount++;
    d_counterNames[id] = name;

    return id;
}

void Profiler::pushScope(const ScopeId id)
{
    if (id >= d_scopeCount) {
        return;
    }

    if (d_depth < k_MAX_DEPTH) {
        d_scopes[id].depth = d_depth;
    }

    ++d_depth;
}

void Profiler::popScope(const ScopeId id, const double ms)
{
    if (id >= d_scopeCount) {
        return;
    }

    if (d_depth > 0) {
        --d_depth;
    }

    d_current.cpuMs[id] += static_cast<float>(ms);
    ++d_current.hits[id];
}

void Profiler::addGpuTime(const ScopeId id, const double ms)
{
    if (id >= d_scopeCount) {
        return;
    }

    d_current.gpuMs[id] += static_cast<float>(ms);
}

void Profiler::addCounter(const CounterId id, const int64_t value)
{
    if (id >= d_counterCount) {
        return;
    }

    d_current.counters[id] += value;
}

void Profiler::beginFrame()
{
    d_current.cpuMs.fill(0.0f);
    d_current.gpuMs.fill(0.0f);
    d_current.hits.fill(0);
    d_current.counters.fill(0);
    d_current.totalMs    = 0.0f;
    d_current.frameIndex = d_frameIndex;

    d_depth = 0;
}

void Profiler::endFrame(const float totalMs)
{
    d_current.frameIndex = d_frameIndex;
    d_current.totalMs    = totalMs;

    if (!d_paused) {
        d_history[d_head] = d_current;
        d_head            = (d_head + 1) % k_HISTORY_SIZE;

        if (d_count < k_HISTORY_SIZE) {
            ++d_count;
        }
    }

    ++d_frameIndex;
}

void Profiler::clearHistory()
{
    d_head  = 0;
    d_count = 0;
}

// ACCESSORS

const FrameRecord& Profiler::frameAt(const uint32_t index) const
{
    // 'd_head' is the next slot to be written, hence one past the newest
    // record; the oldest one sits 'd_count' slots behind it.
    const uint32_t oldest = (d_head + k_HISTORY_SIZE - d_count) %
                            k_HISTORY_SIZE;

    return d_history[(oldest + index) % k_HISTORY_SIZE];
}

const FrameRecord& Profiler::lastFrame() const
{
    return frameAt(d_count - 1);
}

ScopeStats Profiler::cpuStats(const ScopeId id) const
{
    if (id >= d_scopeCount) {
        return ScopeStats();
    }

    std::array<float, k_HISTORY_SIZE> samples = {};
    for (uint32_t i = 0; i < d_count; ++i) {
        samples[i] = frameAt(i).cpuMs[id];
    }

    return statsOf(samples.data(), d_count);
}

ScopeStats Profiler::frameStats() const
{
    std::array<float, k_HISTORY_SIZE> samples = {};
    for (uint32_t i = 0; i < d_count; ++i) {
        samples[i] = frameAt(i).totalMs;
    }

    return statsOf(samples.data(), d_count);
}

}  // close package namespace
