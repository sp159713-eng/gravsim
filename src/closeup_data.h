#pragma once

struct StarOrbit {
    const char* name;
    float periodYr, ecc, aAu, incDeg, nodeDeg, argDeg, phase;
    float color[3];
};

struct Binary {
    const char* name;
    const char* label;
    const char* type;
    const char* fact1;
    const char* fact2;
    const char* fact3;
    float l, b, distLy;
    float mBh, mStar;
    float periodDays, ecc, aAu, starRadiusAu, horizonKm;
    float starColor[3];
    float stream;
    float defRate;
    float incDeg;
};

constexpr int S_COUNT = 6;
constexpr float SGR_MASS_MSUN = 4.15e6f;
constexpr float SGR_DIST_LY = 26000.0f;
extern const StarOrbit S_STARS[S_COUNT];
int binaryCount();
const Binary& binaryAt(int i);
void orbitPoint(float aAu, float ecc, float incDeg, float nodeDeg, float argDeg, float E, float out[3]);
float solveKepler(float M, float ecc);
void starPosition(const StarOrbit& s, double tYr, float out[3]);
float starSpeedKms(const StarOrbit& s, const float pos[3]);
void binaryPositions(const Binary& b, double tDays, float bh[3], float star[3]);
float horizonAu();
