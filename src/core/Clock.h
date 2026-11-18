#pragma once
// ---------------------------------------------------------------------------
// Frame timing.
//
// Everything that moves is scaled by "delta time" (seconds since the last
// frame) so the game runs at the same *speed* regardless of frame rate. The
// classic bug this avoids: hard-coding `pos += 0.1f` per frame, which makes
// your player twice as fast on a 120 Hz monitor.
//
// dt is clamped, because a hitch (window drag, chunk load spike) would
// otherwise teleport the player through the floor in a single step.
// ---------------------------------------------------------------------------
#include <SDL3/SDL_timer.h>

class Clock {
public:
    Clock() : last_(SDL_GetPerformanceCounter()) {}

    float tick() {
        const Uint64 now = SDL_GetPerformanceCounter();
        const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
        float dt = static_cast<float>(static_cast<double>(now - last_) / freq);
        last_ = now;

        if (dt > kMaxDelta) dt = kMaxDelta;
        if (dt < 0.0f) dt = 0.0f;

        // Exponential moving average, purely for a stable FPS readout.
        smoothedDt_ = smoothedDt_ * 0.9f + dt * 0.1f;
        ++frameCount_;
        return dt;
    }

    float fps() const { return smoothedDt_ > 0.0f ? 1.0f / smoothedDt_ : 0.0f; }
    unsigned long long frameCount() const { return frameCount_; }

private:
    static constexpr float kMaxDelta = 0.1f;  // 10 FPS floor for simulation
    Uint64 last_;
    float smoothedDt_ = 1.0f / 60.0f;
    unsigned long long frameCount_ = 0;
};
