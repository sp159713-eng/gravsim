#pragma once
#include <cmath>

namespace em {

constexpr float DIST = 6.0f;
constexpr float MOON_R = 0.3f;
constexpr float TILT = 0.40841f;
constexpr float INCL = 0.2443f;
constexpr float AMP_M = 1.0f;
constexpr float AMP_S = 0.46f;
constexpr float SYNODIC = 29.53f;
constexpr float TWO_PI = 6.2831853f;

struct V3 {
    float x, y, z;
};

inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float p2(float x) { return 0.5f * (3.0f * x * x - 1.0f); }
inline V3 sunDir() { return {1.0f, 0.0f, 0.0f}; }
inline float phiOf(float days) { return TWO_PI * days / SYNODIC; }
inline V3 moonDir(float phi) { return {std::cos(phi) * std::cos(INCL), std::cos(phi) * std::sin(INCL), -std::sin(phi)}; }
inline V3 moonPos(float phi) {
    V3 d = moonDir(phi);
    return {d.x * DIST, d.y * DIST, d.z * DIST};
}

inline V3 cityNormal(float theta) {
    float c = std::cos(theta), s = std::sin(theta);
    float bx = 1.0f, bz = 0.0f;
    float x = bx * c + bz * s, z = -bx * s + bz * c;
    V3 e1 = {std::cos(TILT), -std::sin(TILT), 0.0f};
    return {x * e1.x, x * e1.y, z};
}

inline float tideAt(V3 n, V3 m) { return AMP_M * p2(dot(n, m)) + AMP_S * p2(dot(n, sunDir())); }

inline float illum(float phi) { return 0.5f * (1.0f - dot(moonDir(phi), sunDir())); }
inline bool waxing(float phi) { return std::sin(phi) > 0.0f; }

inline const char* phaseName(float phi) {
    float f = illum(phi);
    bool wx = waxing(phi);
    if (f < 0.04f) return "NEW MOON";
    if (f > 0.96f) return "FULL MOON";
    if (std::fabs(f - 0.5f) < 0.04f) return wx ? "FIRST QUARTER" : "LAST QUARTER";
    if (f < 0.5f) return wx ? "WAXING CRESCENT" : "WANING CRESCENT";
    return wx ? "WAXING GIBBOUS" : "WANING GIBBOUS";
}

inline float bulgeFraction(float phi) {
    V3 m = moonDir(phi);
    float c = p2(dot(m, sunDir()));
    float a = AMP_M + AMP_S * c, b = AMP_S + AMP_M * c;
    return (a > b ? a : b) / (AMP_M + AMP_S);
}

}
