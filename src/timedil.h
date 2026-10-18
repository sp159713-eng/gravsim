#pragma once
#include <vulkan/vulkan.h>
#include <cmath>

struct TimeDilState {
    bool ready = false;
    bool paused = false;
    bool miller = false;
    float speed = 1.0f;
    float x = 4.5951f;
    float rapidity = 0.0f;
    float clock = 0.0f;
    double tau = 0.0;
    double far = 0.0;
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
};

namespace td {

constexpr float DIST = 11.7f;
constexpr float HOLE_R = 1.5f;
constexpr float MILLER_FACTOR = 61320.0f;
constexpr float RAPID_MAX = 5.3f;
constexpr float X_MIN = -4.6052f;
constexpr float X_MAX = 4.5951f;
constexpr float SECS_PER_SEC = 60.0f;

inline float radiusOf(float x) { return 1.0f + std::exp(x); }
inline float xOf(float r) { return std::log(r - 1.0f); }
inline float speedOf(float rapidity) { return std::tanh(rapidity); }
inline float gammaOf(float rapidity) { return std::cosh(rapidity); }

inline float gravFactor(const TimeDilState& s) {
    if (s.miller) return MILLER_FACTOR;
    float r = radiusOf(s.x);
    return 1.0f / std::sqrt(1.0f - 1.0f / r);
}

inline float totalFactor(const TimeDilState& s) { return gravFactor(s) * gammaOf(s.rapidity); }

inline float shadowPx(float H) { return HOLE_R * 0.5f * H * 1.7320508f / DIST; }

inline float probeDistPx(const TimeDilState& s, float H) {
    float r = s.miller ? 1.01f : radiusOf(s.x);
    return shadowPx(H) * (1.3f + 1.45f * std::log10(r));
}

}

void tdRestart(struct AppContext& app);
