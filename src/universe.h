#pragma once
#include <cstdint>
#include <vector>

struct UniPoint {
    float pos[3];
    float radius;
    float color[3];
    float bright;
    float fadeD;
};

struct UniLabel {
    const char* text;
    const char* sub;
    float pos[3];
    float color[3];
    float lo, hi;
    bool dynamic;
};

constexpr float UNIVERSE_MAX_MLY = 120000.0f;
constexpr float CMB_RADIUS_MLY = 46000.0f;
constexpr float GALAXY_CAP_MLY = 32000.0f;

struct UniGalaxy {
    float pos[3];
    float fadeD;
    uint32_t point;
    int named;
};

struct UniverseData {
    std::vector<UniPoint> pts;
    std::vector<UniGalaxy> gals;
    unsigned mwCount = 0;
    unsigned hercFirst = 0, hercCount = 0;
};

UniverseData makeUniverse();
std::vector<UniLabel> universeLabels();
