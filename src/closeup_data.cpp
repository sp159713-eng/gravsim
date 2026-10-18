#include "closeup_data.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr float PI = 3.14159265f;

const Binary BINARIES[] = {
    {"CYGNUS X-1", "CYGNUS X-1  BLACK HOLE  7,200 LY", "BLACK HOLE + BLUE SUPERGIANT",
     "21 SUN HOLE + 41 SUN STAR", "5.6 DAY ORBIT, GAS FEEDS A DISC", "FIRST BH CANDIDATE, 1971",
     71.335f, 3.067f, 7200.0f, 21.2f, 40.6f, 5.599f, 0.02f, 0.2438f, 0.104f, 63.0f, {0.55f, 0.7f, 1.0f}, 1.0f, 0.7f, 62.0f},
    {"GAIA BH1", "GAIA BH1  BLACK HOLE  1,560 LY", "DORMANT BLACK HOLE + SUN-LIKE STAR",
     "NEAREST KNOWN BLACK HOLE, 2022", "186 DAY ORBIT, NOT FEEDING", "STAR WOBBLE GAVE IT AWAY",
     22.637f, 18.052f, 1560.0f, 9.62f, 0.93f, 185.6f, 0.45f, 1.4f, 0.0043f, 28.0f, {1.0f, 0.93f, 0.75f}, 0.0f, 20.0f, 55.0f},
};

}

const StarOrbit S_STARS[S_COUNT] = {
    {"S2", 16.05f, 0.886f, 1030.0f, 134.2f, 226.9f, 65.5f, 0.78f, {1.0f, 0.85f, 0.35f}},
    {"S62", 9.9f, 0.976f, 741.0f, 72.0f, 122.0f, 42.0f, 0.30f, {0.5f, 0.85f, 1.0f}},
    {"S4714", 12.0f, 0.985f, 842.0f, 127.7f, 129.6f, 357.0f, 0.55f, {1.0f, 0.45f, 0.75f}},
    {"S29", 101.8f, 0.728f, 3500.0f, 104.0f, 300.0f, 20.0f, 0.10f, {0.6f, 1.0f, 0.55f}},
    {"S38", 19.2f, 0.820f, 1152.0f, 170.0f, 101.0f, 215.0f, 0.65f, {1.0f, 0.6f, 0.3f}},
    {"S55", 12.8f, 0.721f, 879.0f, 150.0f, 342.0f, 105.0f, 0.90f, {0.8f, 0.6f, 1.0f}},
};

int binaryCount() { return (int)(sizeof(BINARIES) / sizeof(BINARIES[0])); }

const Binary& binaryAt(int i) { return BINARIES[i]; }

float solveKepler(float M, float ecc) {
    M = std::fmod(M, 2.0f * PI);
    if (M < 0.0f) M += 2.0f * PI;
    float E = ecc > 0.8f ? PI : M;
    for (int i = 0; i < 40; i++) {
        float f = E - ecc * std::sin(E) - M;
        float d = 1.0f - ecc * std::cos(E);
        E -= f / std::max(d, 0.02f);
    }
    return E;
}

void orbitPoint(float aAu, float ecc, float incDeg, float nodeDeg, float argDeg, float E, float out[3]) {
    float x = aAu * (std::cos(E) - ecc), y = aAu * std::sqrt(1.0f - ecc * ecc) * std::sin(E);
    float w = argDeg * PI / 180.0f, i = incDeg * PI / 180.0f, n = nodeDeg * PI / 180.0f;
    float x1 = x * std::cos(w) - y * std::sin(w), y1 = x * std::sin(w) + y * std::cos(w);
    float y2 = y1 * std::cos(i), z2 = y1 * std::sin(i);
    float X = x1 * std::cos(n) - y2 * std::sin(n), Y = x1 * std::sin(n) + y2 * std::cos(n);
    out[0] = X;
    out[1] = z2;
    out[2] = -Y;
}

void starPosition(const StarOrbit& s, double tYr, float out[3]) {
    double frac = tYr / s.periodYr + s.phase;
    frac -= std::floor(frac);
    float E = solveKepler((float)(frac * 2.0 * PI), s.ecc);
    orbitPoint(s.aAu, s.ecc, s.incDeg, s.nodeDeg, s.argDeg, E, out);
}

float starSpeedKms(const StarOrbit& s, const float pos[3]) {
    float r = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
    float v = 29.7847f * std::sqrt(SGR_MASS_MSUN * (2.0f / r - 1.0f / s.aAu));
    return v;
}

void binaryPositions(const Binary& b, double tDays, float bh[3], float star[3]) {
    double frac = tDays / b.periodDays + 0.15;
    frac -= std::floor(frac);
    float E = solveKepler((float)(frac * 2.0 * PI), b.ecc);
    float rel[3];
    orbitPoint(b.aAu, b.ecc, b.incDeg, 35.0f, 20.0f, E, rel);
    float tot = b.mBh + b.mStar;
    for (int a = 0; a < 3; a++) {
        star[a] = rel[a] * b.mBh / tot;
        bh[a] = -rel[a] * b.mStar / tot;
    }
}

float horizonAu() {
    return 2.953f * SGR_MASS_MSUN / 1.496e8f;
}
