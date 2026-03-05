// step_timer.h                                                       -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_STEP_TIMER
#define INCLUDED_ENGINE_CORE_STEP_TIMER

#include <chrono>

namespace engine::core {

class StepTimer {
  public:
    // TYPES

    using Clock = std::chrono::high_resolution_clock;

  private:
    // DATA

    Clock::time_point d_startTime = Clock::now();
    Clock::time_point d_lastTime  = Clock::now();

    float d_deltaTime = 0.0f;
    float d_totalTime = 0.0f;

    // FPS calculation members
    uint64_t d_totalFrames      = 0;
    uint32_t d_framesPerSecond  = 0;
    uint32_t d_framesThisSecond = 0;
    double   d_elapsedTime      = 0.0;
    double   d_avgMs            = 0.0;

  public:
    // CREATORS

    StepTimer() = default;

    // MANIPULATORS

    /// Reset the timer to the current time.
    void reset();

    /// Update the timer. Should be called once per frame.
    void tick();

    // ACCESSORS

    /// Get the time elapsed since the last tick (in seconds).
    [[nodiscard]] float releaseDeltaTime() const;

    /// Get the total time elapsed since the start/reset (in seconds).
    [[nodiscard]] float totalTime() const;

    /// Get the current frames per second.
    [[nodiscard]] uint32_t fps() const;

    /// Get the average frame time in milliseconds.
    [[nodiscard]] double avgMs() const;

    /// Get the total number of frames elapsed.
    [[nodiscard]] uint64_t totalFrames() const;
};

// ============================================================================
//                           INLINE DEFINITIONS
// ============================================================================

inline float StepTimer::releaseDeltaTime() const
{
    return d_deltaTime;
}

inline float StepTimer::totalTime() const
{
    return d_totalTime;
}

inline uint32_t StepTimer::fps() const
{
    return d_framesPerSecond;
}

inline double StepTimer::avgMs() const
{
    return d_avgMs;
}

inline uint64_t StepTimer::totalFrames() const
{
    return d_totalFrames;
}

}  // close engine::core namespace
#endif