#pragma once
#include <vector>

struct GalaxyPoint {
    float pos[3];
    float bright;
    float color[3];
};

struct GalaxyLabel {
    const char* text;
    float pos[3];
    float color[3];
};

struct SpectralClass {
    const char* name;
    const char* life;
    float rgb[3];
    float lum;
};

extern const SpectralClass SPECTRAL[6];

constexpr float LY_PER_AU_INV = 1.0f / 63241.077f;

std::vector<GalaxyPoint> makeMilkyWay(unsigned count, unsigned seed);
std::vector<GalaxyLabel> milkyWayLabels();
void galaxyNormal(float out[3]);
void galaxySunToSim(const float g[3], float out[3]);
void galaxyCentreLy(float out[3]);
