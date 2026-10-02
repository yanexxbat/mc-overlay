#pragma once
#include <algorithm>

namespace mcov::control_panel::motion {
inline constexpr float kFeedbackSeconds = .12f;
inline constexpr float kHoverSeconds = .14f;
inline constexpr float kPageSeconds = .16f;

// The UI's shared ease-out curve: cubic-bezier(0.23, 1, 0.32, 1).
// Solve x numerically; use actual elapsed time rather than frame-dependent lerp.
inline float EaseOut(float progress) {
    if (progress <= 0.f) return 0.f;
    if (progress >= 1.f) return 1.f;
    float low = 0.f, high = 1.f;
    for (int i = 0; i < 16; ++i) {
        const float t = (low + high) * .5f, u = 1.f - t;
        const float x = 3.f * u * u * t * .23f + 3.f * u * t * t * .32f + t * t * t;
        if (x < progress) low = t; else high = t;
    }
    const float t = (low + high) * .5f, u = 1.f - t;
    return 1.f - u * u * u;
}
} // namespace mcov::control_panel::motion
