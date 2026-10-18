#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "particles.h"

constexpr uint32_t THROW_SLOTS = 24;
constexpr uint32_t DWARF_STARS = 320;
constexpr uint32_t DWARF_POOL = 4;

struct ThrowBody {
    double p[3] = {0, 0, 0}, v[3] = {0, 0, 0};
    double m = 0;
    float size = 1.0f;
    float col[3] = {1, 1, 1};
    int kind = 0;
    uint32_t slot = 0;
    int age = 0;
    std::vector<std::array<float, 3>> trail;
};

struct ThrowFlash {
    double p[3];
    float r, age;
    float col[3];
};

struct ThrowWrite {
    uint32_t first;
    std::vector<Particle> data;
};

struct ThrowState {
    bool on = false;
    int scene = -1;
    int type = 0;
    int mk = 0;
    bool dragging = false, vertical = false, leftPrev = false, rightPrev = false;
    double sx = 0, sy = 0, sz = 0, ex = 0, ey = 0, ez = 0;
    double pressPx = 0, pressPy = 0, panX = 0, panY = 0;
    bool keyPrev[8] = {false, false, false, false, false, false, false, false};
    bool massPrev[2] = {false, false};
    std::vector<ThrowBody> bodies;
    std::vector<ThrowFlash> flashes;
    std::vector<std::array<float, 3>> preview;
    std::vector<ThrowWrite> writes;
    std::vector<Particle> initial;
    uint32_t recCount[2] = {0, 0};
    uint32_t starsUsed = 0;
    bool full = false;
    float clock = 0.0f;
    float msgT = 0.0f;
    int demo = 0;
    int demoStep = 0;
    double vc = 0, speed = 0;
    bool orbit = false;
    bool forceV = false;
    double fv[3] = {0, 0, 0};
    uint32_t rng = 987654321u;
};

inline int throwKind(bool galaxy, int type) {
    static const int g[4] = {2, 3, 5, 4};
    return galaxy ? g[type] : type;
}

inline double throwDefMass(bool galaxy, int type) {
    static const double s[4] = {0.003, 0.95, 500.0, 5000.0}, g[4] = {2.0, 40.0, 800.0, 100.0};
    return galaxy ? g[type] : s[type];
}

inline double throwMsun(bool galaxy, double m) {
    return galaxy ? m * 4000.0 : m * 0.001;
}

extern int g_throwDemo;
