// step_timer.cpp                                                     -*-C++-*-
#include <core/step_timer.h>

namespace engine::core {

void StepTimer::reset()
{
    d_startTime        = Clock::now();
    d_lastTime         = d_startTime;
    d_totalTime        = 0.0f;
    d_deltaTime        = 0.0f;
    d_totalFrames      = 0;
    d_framesPerSecond  = 0;
    d_framesThisSecond = 0;
    d_elapsedTime      = 0.0;
    d_avgMs            = 0.0;
}

void StepTimer::tick()
{
    const auto currentTime = Clock::now();

    // Calcul du temps écoulé en secondes
    const std::chrono::duration<float> timeSpan = currentTime - d_lastTime;
    d_deltaTime                                 = timeSpan.count();

    // Mise à jour du temps total
    const std::chrono::duration<float> totalSpan = currentTime - d_startTime;
    d_totalTime                                  = totalSpan.count();

    d_lastTime = currentTime;

    // Sécurité: Empêcher un dt négatif (bug de clock système rare)
    if (d_deltaTime < 0.0f) {
        d_deltaTime = 0.0f;
    }

    // FPS calculation
    d_totalFrames++;
    d_framesThisSecond++;
    d_elapsedTime += d_deltaTime;
    if (d_elapsedTime >= 1.0) {
        d_framesPerSecond = d_framesThisSecond;
        d_avgMs = (d_elapsedTime / d_framesThisSecond) * 1000.0;
        d_framesThisSecond = 0;
        d_elapsedTime -= 1.0;
    }
}

}  // close engine::core namespace