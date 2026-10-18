#pragma once
#include "milkyway.h"
#include "starsystem.h"
#include <vector>

struct RealPlanet {
    const char* name;
    float au, ecc, periodDays, radiusEarth, massEarth;
};

struct RealStar {
    const char* name;
    float raH, decDeg, distLy, tempK, lum, mass, radius, ageGyr, sepLy;
    int planetCount;
    RealPlanet planets[8];
    float beltLo, beltHi;
};

int realStarCount();
const RealStar& realStarAt(int i);
std::vector<GalaxyPoint> realStarPoints();
StarSystem realSystem(int i);
void realStarPos(int i, float out[3]);
