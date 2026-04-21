// core_steptimer.h                                                   -*-C++-*-
#ifndef INCLUDED_ENG_CORE_STEP_TIMER
#define INCLUDED_ENG_CORE_STEP_TIMER

//@PURPOSE: Provide a high-resolution timer for engine frame updates.
//
//@CLASSES:
//  eng::core::StepTimer: Utility for tracking elapsed time and frame rates.
//
//@DESCRIPTION: This component provides the 'eng::core::StepTimer' class,
// which calculates the delta time between frames and tracks various timing
// statistics (FPS, average frame time). It is used by the main engine loop
// to ensure smooth updates and animations.

#include <chrono>

namespace eng::core {

// ============
// class StepTimer
// ============

/// This class handles high-resolution timing and performance tracking for the
/// engine's execution loop.
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

    /// Create a 'StepTimer' initialized to the current time.
    StepTimer() = default;

    // MANIPULATORS

    /// Reset the starting time to the current clock value.
    void reset();

    /// Recalculate delta time and update FPS statistics. This method should
    /// be called exactly once per frame.
    void tick();

    // ACCESSORS

    /// Return the time elapsed between the last two 'tick' calls, in seconds.
    [[nodiscard]]
    float releaseDeltaTime() const;

    /// Return the total time elapsed since the timer was created or reset,
    /// in seconds.
    [[nodiscard]]
    float totalTime() const;

    /// Return the current number of frames rendered per second.
    [[nodiscard]]
    uint32_t fps() const;

    /// Return the average duration of a frame in milliseconds.
    [[nodiscard]]
    double avgMs() const;

    /// Return the total number of frames that have occurred since startup.
    [[nodiscard]]
    uint64_t totalFrames() const;
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

}  // close package namespace
#endif  // INCLUDED_ENG_CORE_STEP_TIMER