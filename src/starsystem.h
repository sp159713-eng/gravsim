#pragma once
#include "particles.h"
#include <vector>

struct PlanetSpec {
    float au, ecc, incl, node, phase, mass, radiusKm, tilt, day, tint[3];
    int type;
    bool habitable;
    const char* name = nullptr;
};

struct MoonSpec {
    int parent;
    float rel, period, phase;
};

struct StarSystem {
    uint32_t index;
    int cls;
    float mass, lum, radiusSun, ageGyr, lifeGyr, color[3];
    float hzIn, hzOut, beltLo, beltHi, outerLo, outerHi, refAu;
    int planetCount, moonCount;
    int real = -1;
    float au0 = 0.05f;
    bool ring;
    PlanetSpec planets[8];
    MoonSpec moons[5];
};

StarSystem generateSystem(uint32_t index, int cls, const float color[3]);
std::vector<Particle> buildSystem(const StarSystem& s, uint32_t n, bool trueScale);
float orbitUnits(float au, bool trueScale, float au0 = 0.05f);
float arrivalDistance(const StarSystem& s, bool trueScale);
void systemStyles(const StarSystem& s, float out[9][8]);
void homeStyles(float out[9][8]);
void applyStyleRadii(const float styles[9][8]);
