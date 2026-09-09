// core_profiler.h                                                    -*-C++-*-
#ifndef INCLUDED_ENG_CORE_PROFILER_H
#define INCLUDED_ENG_CORE_PROFILER_H

//@PURPOSE: Provide a per-frame aggregating CPU/GPU profiler service.
//
//@CLASSES:
//  eng::core::FrameRecord: Timings and counters gathered for a single frame.
//  eng::core::ScopeStats: Statistics of one series over the history window.
//  eng::core::Profiler: Central store of profiling samples.
//  eng::core::ProfileScope: Scoped guard timing a region of code.
//
//@MACROS:
//  ENG_PROFILE_SCOPE(name): Time the enclosing scope under 'name'.
//  ENG_PROFILE_SCOPE_ID(id): Time the enclosing scope under a known id.
//  ENG_PROFILE_COUNTER(name, value): Accumulate 'value' under 'name'.
//
//@DESCRIPTION: This component provides the 'eng::core::Profiler' class, a
// central store for the timings and counters collected during a frame, and
// 'eng::core::ProfileScope', the RAII guard that feeds it.
//
// The profiler is an *aggregating* profiler, not a tracing one: every sample
// of a given scope within a frame is accumulated into a single slot instead
// of being appended to an event list. Memory is therefore bounded and known
// at compile time, at the cost of losing the chronological order of the
// samples. Each committed frame is stored in a fixed-size ring buffer so that
// the UI can plot a history and derive percentiles from it.
//
// Scopes are identified by a small integer allocated once by 'registerScope'.
// The 'ENG_PROFILE_SCOPE' macro caches that identifier in a function-local
// 'static', so a scope name is only ever looked up once per call site; the
// per-frame cost of a sample is a clock read and an addition.
//
// GPU timings are stored alongside the CPU ones under the same scope
// identifiers, so that both can be displayed side by side. Because GPU
// timestamps are read back with a latency of a couple of frames, a GPU sample
// is attributed to the frame in which it is *read*, not to the frame it was
// recorded in; consumers must treat those values as slightly delayed.
//
// The profiler assumes a single-threaded engine loop and deliberately avoids
// any synchronization. Should work be spread over several threads, each
// thread will need its own accumulator, merged when the frame is committed.
//
/// Usage
///-----
// Instrument a region of the frame:
//..
//  void Renderer::renderScene(CommandListProtocol *cmd, Scene& scene) const
//  {
//      ENG_PROFILE_SCOPE("Record");
//      // ...
//  }
//..
// Then bracket the frame from the main loop:
//..
//  Profiler::instance().beginFrame();
//  // ... run the frame ...
//  Profiler::instance().endFrame(frameDurationMs);
//..

// std
#include <array>
#include <cstdint>

// core
#include <core/core_steptimer.h>

namespace eng::core {

// TYPES

/// Identifier of a registered profiling scope.
using ScopeId = uint16_t;

/// Identifier of a registered profiling counter.
using CounterId = uint16_t;

// CONSTANTS

/// Maximum number of distinct scopes that can be registered.
inline constexpr uint16_t k_MAX_SCOPES = 64;

/// Maximum number of distinct counters that can be registered.
inline constexpr uint16_t k_MAX_COUNTERS = 16;

/// Number of frames kept in the history ring buffer.
inline constexpr uint32_t k_HISTORY_SIZE = 256;

/// Maximum nesting level tracked for the scopes.
inline constexpr uint8_t k_MAX_DEPTH = 16;

/// Value returned when no scope or counter slot is left.
inline constexpr uint16_t k_INVALID_ID = 0xFFFF;

// ==================
// struct FrameRecord
// ==================

/// This simply constrained attribute type holds every sample gathered during
/// a single frame.
struct FrameRecord {
    // PUBLIC DATA

    /// Monotonic index of the frame this record describes.
    uint64_t frameIndex = 0;

    /// Wall-clock duration of the whole frame, in milliseconds.
    float totalMs = 0.0f;

    /// CPU time accumulated by each scope, in milliseconds.
    std::array<float, k_MAX_SCOPES> cpuMs = {};

    /// GPU time reported for each scope, in milliseconds.
    std::array<float, k_MAX_SCOPES> gpuMs = {};

    /// Number of times each scope was entered.
    std::array<uint16_t, k_MAX_SCOPES> hits = {};

    /// Value accumulated by each counter.
    std::array<int64_t, k_MAX_COUNTERS> counters = {};
};

// =================
// struct ScopeStats
// =================

/// This simply constrained attribute type holds the statistics of a single
/// series of samples over the history window.
struct ScopeStats {
    // PUBLIC DATA

    /// Arithmetic mean of the samples, in milliseconds.
    float avgMs = 0.0f;

    /// Smallest sample, in milliseconds.
    float minMs = 0.0f;

    /// Largest sample, in milliseconds.
    float maxMs = 0.0f;

    /// 99th percentile of the samples, in milliseconds. For frame times this
    /// is the "1% low": the duration only one frame in a hundred exceeds.
    float p99Ms = 0.0f;

    /// Number of samples the statistics were computed from.
    uint32_t sampleCount = 0;
};

// ==============
// class Profiler
// ==============

/// This class is responsible for accumulating the profiling samples of the
/// current frame and for keeping a bounded history of the committed frames.
class Profiler {
  private:
    // PRIVATE TYPES

    /// Descriptor of a registered scope.
    struct ScopeInfo {
        /// Name of the scope. Expected to be a string literal.
        const char* name_p = nullptr;

        /// Nesting level observed the last time the scope was entered.
        uint8_t depth = 0;
    };

    // DATA

    /// Descriptors of the registered scopes.
    std::array<ScopeInfo, k_MAX_SCOPES> d_scopes = {};

    /// Number of registered scopes.
    uint16_t d_scopeCount = 0;

    /// Names of the registered counters.
    std::array<const char*, k_MAX_COUNTERS> d_counterNames = {};

    /// Number of registered counters.
    uint16_t d_counterCount = 0;

    /// Samples gathered for the frame being recorded.
    FrameRecord d_current = {};

    /// Ring buffer of the committed frames.
    std::array<FrameRecord, k_HISTORY_SIZE> d_history = {};

    /// Index of the next slot to be written in 'd_history'.
    uint32_t d_head = 0;

    /// Number of valid records in 'd_history'.
    uint32_t d_count = 0;

    /// Index of the frame being recorded.
    uint64_t d_frameIndex = 0;

    /// Number of scopes currently entered.
    uint8_t d_depth = 0;

    /// Whether committing frames to the history is suspended.
    bool d_paused = false;

    // PRIVATE CREATORS

    /// Create an empty profiler.
    Profiler() = default;

  public:
    // CLASS METHODS

    /// Return a reference to the process-wide profiler.
    static Profiler& instance();

    // CREATORS

    ~Profiler()                          = default;
    Profiler(const Profiler&)            = delete;
    Profiler& operator=(const Profiler&) = delete;

    // MANIPULATORS

    /// Return the identifier of the scope with the specified 'name',
    /// registering it if it is not already known. Return 'k_INVALID_ID' if
    /// no slot is left. The behavior is undefined unless 'name' outlives
    /// this object; string literals are expected.
    ScopeId registerScope(const char* name);

    /// Return the identifier of the counter with the specified 'name',
    /// registering it if it is not already known. Return 'k_INVALID_ID' if
    /// no slot is left. The behavior is undefined unless 'name' outlives
    /// this object; string literals are expected.
    CounterId registerCounter(const char* name);

    /// Record the entry of the scope with the specified 'id'.
    void pushScope(ScopeId id);

    /// Record the exit of the scope with the specified 'id' and accumulate
    /// the specified 'ms' into its CPU time for the current frame.
    void popScope(ScopeId id, double ms);

    /// Accumulate the specified 'ms' into the GPU time of the scope with the
    /// specified 'id' for the current frame. GPU samples are attributed to
    /// the frame in which they are read back, not to the frame they were
    /// recorded in.
    void addGpuTime(ScopeId id, double ms);

    /// Accumulate the specified 'value' into the counter with the specified
    /// 'id' for the current frame.
    void addCounter(CounterId id, int64_t value);

    /// Discard the samples of the current frame and start gathering a new
    /// one. This method is to be called exactly once per frame, before any
    /// sample is taken.
    void beginFrame();

    /// Commit the samples of the current frame to the history, stamping them
    /// with the specified 'totalMs' frame duration. This method is to be
    /// called exactly once per frame, after every sample has been taken. The
    /// frame is dropped if this profiler is paused.
    void endFrame(float totalMs);

    /// Suspend the recording of new frames into the history if the specified
    /// 'paused' is 'true', and resume it otherwise.
    void setPaused(bool paused);

    /// Remove every frame from the history.
    void clearHistory();

    // ACCESSORS

    /// Return the number of registered scopes.
    [[nodiscard]]
    uint16_t scopeCount() const;

    /// Return the name of the scope with the specified 'id'. The behavior is
    /// undefined unless 'id' is a registered scope identifier.
    [[nodiscard]]
    const char* scopeName(ScopeId id) const;

    /// Return the nesting level of the scope with the specified 'id'. The
    /// behavior is undefined unless 'id' is a registered scope identifier.
    [[nodiscard]]
    uint8_t scopeDepth(ScopeId id) const;

    /// Return the number of registered counters.
    [[nodiscard]]
    uint16_t counterCount() const;

    /// Return the name of the counter with the specified 'id'. The behavior
    /// is undefined unless 'id' is a registered counter identifier.
    [[nodiscard]]
    const char* counterName(CounterId id) const;

    /// Return the number of frames held in the history.
    [[nodiscard]]
    uint32_t historySize() const;

    /// Return a reference to the frame at the specified 'index' in the
    /// history, index 0 being the oldest one. The behavior is undefined
    /// unless 'index < historySize()'.
    [[nodiscard]]
    const FrameRecord& frameAt(uint32_t index) const;

    /// Return a reference to the most recently committed frame. The behavior
    /// is undefined unless 'historySize() > 0'.
    [[nodiscard]]
    const FrameRecord& lastFrame() const;

    /// Return the statistics of the CPU time of the scope with the specified
    /// 'id' over the history window.
    [[nodiscard]]
    ScopeStats cpuStats(ScopeId id) const;

    /// Return the statistics of the frame duration over the history window.
    [[nodiscard]]
    ScopeStats frameStats() const;

    /// Return 'true' if the recording of new frames is suspended, and 'false'
    /// otherwise.
    [[nodiscard]]
    bool isPaused() const;
};

// ==================
// class ProfileScope
// ==================

/// This mechanism times the region of code it is declared in and reports the
/// result to the process-wide profiler upon destruction.
class ProfileScope {
  private:
    // DATA

    /// Identifier of the profiled scope.
    ScopeId d_id;

    /// Time at which the scope was entered.
    StepTimer::Clock::time_point d_start;

  public:
    // CREATORS

    /// Create a guard timing the scope with the specified 'id'.
    explicit ProfileScope(ScopeId id);

    /// Report the elapsed time to the profiler and destroy this guard.
    ~ProfileScope();

    ProfileScope(const ProfileScope&)            = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
};

// ===========================================================================
//                           INLINE DEFINITIONS
// ===========================================================================

// --------------
// class Profiler
// --------------

inline void Profiler::setPaused(const bool paused)
{
    d_paused = paused;
}

inline uint16_t Profiler::scopeCount() const
{
    return d_scopeCount;
}

inline const char* Profiler::scopeName(const ScopeId id) const
{
    return d_scopes[id].name_p;
}

inline uint8_t Profiler::scopeDepth(const ScopeId id) const
{
    return d_scopes[id].depth;
}

inline uint16_t Profiler::counterCount() const
{
    return d_counterCount;
}

inline const char* Profiler::counterName(const CounterId id) const
{
    return d_counterNames[id];
}

inline uint32_t Profiler::historySize() const
{
    return d_count;
}

inline bool Profiler::isPaused() const
{
    return d_paused;
}

// ------------------
// class ProfileScope
// ------------------

inline ProfileScope::ProfileScope(const ScopeId id)
: d_id(id)
, d_start(StepTimer::Clock::now())
{
    Profiler::instance().pushScope(d_id);
}

inline ProfileScope::~ProfileScope()
{
    const std::chrono::duration<double, std::milli> elapsed =
        StepTimer::Clock::now() - d_start;

    Profiler::instance().popScope(d_id, elapsed.count());
}

}  // close package namespace

// ===========================================================================
//                                 MACROS
// ===========================================================================

#define ENG_PROFILE_CAT_IMPL(a, b) a##b
#define ENG_PROFILE_CAT(a, b) ENG_PROFILE_CAT_IMPL(a, b)

#ifdef ENG_ENABLE_PROFILING

/// Time the enclosing scope under the specified 'name', which must be a
/// string literal.
#define ENG_PROFILE_SCOPE(name)                                               \
    static const eng::core::ScopeId ENG_PROFILE_CAT(engScopeId_, __LINE__) =  \
        eng::core::Profiler::instance().registerScope(name);                  \
    const eng::core::ProfileScope ENG_PROFILE_CAT(engScope_, __LINE__)(       \
        ENG_PROFILE_CAT(engScopeId_, __LINE__))

/// Time the enclosing scope under the scope identifier specified by 'id'.
#define ENG_PROFILE_SCOPE_ID(id)                                              \
    const eng::core::ProfileScope ENG_PROFILE_CAT(engScope_, __LINE__)(id)

/// Accumulate the specified 'value' into the counter with the specified
/// 'name', which must be a string literal.
#define ENG_PROFILE_COUNTER(name, value)                                      \
    do {                                                                      \
        static const eng::core::CounterId ENG_PROFILE_CAT(engCntId_,          \
                                                          __LINE__) =         \
            eng::core::Profiler::instance().registerCounter(name);            \
        eng::core::Profiler::instance().addCounter(ENG_PROFILE_CAT(engCntId_, \
                                                                   __LINE__), \
                                                   (value));                  \
    } while (false)

#else

#define ENG_PROFILE_SCOPE(name) ((void)0)
#define ENG_PROFILE_SCOPE_ID(id) ((void)0)
#define ENG_PROFILE_COUNTER(name, value) ((void)0)

#endif  // ENG_ENABLE_PROFILING

#endif  // INCLUDED_ENG_CORE_PROFILER_H
