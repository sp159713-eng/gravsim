#pragma once
#include <array>
#include <vector>

struct SbPt {
    double x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0, m = 0, r = 0;
};

struct SbBody : SbPt {
    float col[3] = {1, 1, 1};
    int type = 0;
    bool sun = false;
    int role = 0;
    const char* name = nullptr;
    std::vector<std::array<float, 3>> trail;
};

struct SbPart {
    double x, y, z, vx, vy, vz, age;
    bool disc;
};

struct SbFlash {
    double x, y, z;
    float r, age;
    float col[3];
};

struct SandboxState {
    bool ready = false;
    bool demo = false;
    std::vector<SbBody> bodies;
    std::vector<SbFlash> flashes;
    std::vector<std::array<float, 3>> preview;
    float cam[7] = {0.6f, 0.5f, 170.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int type = 0;
    int mk = 0;
    bool massPrev[2] = {false, false};
    bool dragging = false, vertical = false;
    double sx = 0, sy = 0, sz = 0, ex = 0, ey = 0, ez = 0;
    double pressPx = 0, pressPy = 0;
    bool leftPrev = false, rightPrev = false, midPrev = false;
    double panX = 0, panY = 0;
    bool keyPrev[6] = {false, false, false, false, false, false};
    float clock = 0;
    float speed = 1;
    bool paused = false;
    uint32_t rng = 20250611;
    int demoStep = 0;
    bool full = false;
    int preset = -1;
    char capName[48] = {0};
    char capFact[120] = {0};
    float capT = 0;
    size_t trailCap = 80;
    std::vector<SbPart> parts;
    double emitAcc = 0;
};
