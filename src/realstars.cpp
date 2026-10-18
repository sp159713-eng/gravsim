#include "realstars.h"
#include <algorithm>
#include <cmath>

namespace {

const RealStar STARS[] = {
    {"ALPHA CENTAURI A", 14.660f, -60.835f, 4.37f, 5790, 1.52f, 1.10f, 1.22f, 5.3f, 0.0f, 0, {}, 0, 0},
    {"ALPHA CENTAURI B", 14.660f, -60.835f, 4.37f, 5260, 0.50f, 0.91f, 0.86f, 5.3f, 0.00036f, 0, {}, 0, 0},
    {"PROXIMA CENTAURI", 14.495f, -62.680f, 4.24f, 3040, 0.0017f, 0.122f, 0.154f, 4.8f, 0.0f, 2,
     {{"B", 0.0485f, 0.11f, 11.19f, 1.1f, 1.07f}, {"D", 0.0289f, 0.04f, 5.12f, 0.8f, 0.26f}}, 0, 0},
    {"BARNARD'S STAR", 17.963f, 4.693f, 5.96f, 3270, 0.0035f, 0.144f, 0.196f, 10.0f, 0.0f, 1,
     {{"B", 0.0229f, 0.0f, 3.15f, 0.9f, 0.37f}}, 0, 0},
    {"WOLF 359", 10.941f, 7.009f, 7.86f, 2800, 0.00137f, 0.11f, 0.144f, 0.5f, 0.0f, 0, {}, 0, 0},
    {"LALANDE 21185", 11.056f, 35.97f, 8.30f, 3600, 0.026f, 0.39f, 0.39f, 8.0f, 0.0f, 1,
     {{"B", 0.0689f, 0.0f, 12.95f, 1.5f, 2.69f}}, 0, 0},
    {"SIRIUS A", 6.7525f, -16.716f, 8.60f, 9940, 25.4f, 2.06f, 1.71f, 0.24f, 0.0f, 0, {}, 0, 0},
    {"SIRIUS B", 6.7525f, -16.716f, 8.60f, 25200, 0.056f, 1.02f, 0.0084f, 0.24f, 0.0003f, 0, {}, 0, 0},
    {"LUYTEN 726-8", 1.650f, -17.95f, 8.73f, 2670, 0.0006f, 0.1f, 0.14f, 1.0f, 0.0f, 0, {}, 0, 0},
    {"ROSS 154", 18.830f, -23.836f, 9.69f, 3340, 0.0038f, 0.17f, 0.24f, 1.0f, 0.0f, 0, {}, 0, 0},
    {"EPSILON ERIDANI", 3.549f, -9.458f, 10.47f, 5084, 0.34f, 0.82f, 0.74f, 0.6f, 0.0f, 1,
     {{"B", 3.39f, 0.07f, 2690.0f, 11.2f, 207.0f}}, 0, 0},
    {"TAU CETI", 1.7344f, -15.937f, 11.91f, 5344, 0.52f, 0.78f, 0.79f, 5.8f, 0.0f, 2,
     {{"E (CANDIDATE)", 0.538f, 0.0f, 162.9f, 1.7f, 3.93f}, {"F (CANDIDATE)", 1.334f, 0.16f, 636.1f, 1.7f, 3.93f}}, 0, 0},
    {"PROCYON A", 7.655f, 5.225f, 11.46f, 6530, 6.9f, 1.5f, 2.05f, 2.0f, 0.0f, 0, {}, 0, 0},
    {"61 CYGNI A", 21.115f, 38.749f, 11.40f, 4400, 0.15f, 0.70f, 0.67f, 6.0f, 0.0f, 0, {}, 0, 0},
    {"VEGA", 18.616f, 38.784f, 25.04f, 9600, 40.0f, 2.1f, 2.36f, 0.45f, 0.0f, 0, {}, 80.0f, 120.0f},
    {"ALTAIR", 19.846f, 8.868f, 16.73f, 7550, 10.6f, 1.86f, 1.79f, 1.2f, 0.0f, 0, {}, 0, 0},
    {"ARCTURUS", 14.261f, 19.182f, 36.7f, 4286, 170.0f, 1.08f, 25.4f, 7.1f, 0.0f, 0, {}, 0, 0},
    {"BETELGEUSE", 5.919f, 7.407f, 550.0f, 3600, 100000.0f, 16.5f, 764.0f, 0.01f, 0.0f, 0, {}, 0, 0},
    {"RIGEL", 5.2423f, -8.202f, 860.0f, 12100, 120000.0f, 21.0f, 78.9f, 0.008f, 0.0f, 0, {}, 0, 0},
    {"POLARIS", 2.5303f, 89.264f, 433.0f, 6015, 2000.0f, 5.4f, 46.0f, 0.07f, 0.0f, 0, {}, 0, 0},
    {"DENEB", 20.691f, 45.280f, 2600.0f, 8525, 196000.0f, 19.0f, 203.0f, 0.01f, 0.0f, 0, {}, 0, 0},
    {"ANTARES", 16.490f, -26.432f, 550.0f, 3660, 76000.0f, 12.0f, 680.0f, 0.012f, 0.0f, 0, {}, 0, 0},
    {"ALDEBARAN", 4.599f, 16.509f, 65.3f, 3910, 439.0f, 1.16f, 44.2f, 6.4f, 0.0f, 0, {}, 0, 0},
    {"CAPELLA", 5.278f, 45.998f, 42.9f, 4940, 78.7f, 2.57f, 11.98f, 0.6f, 0.0f, 0, {}, 0, 0},
    {"SPICA", 13.420f, -11.161f, 250.0f, 22400, 12000.0f, 11.4f, 7.5f, 0.012f, 0.0f, 0, {}, 0, 0},
    {"FOMALHAUT", 22.961f, -29.622f, 25.1f, 8590, 16.6f, 1.92f, 1.84f, 0.44f, 0.0f, 0, {}, 133.0f, 158.0f},
    {"TRAPPIST-1", 23.108f, -5.041f, 40.7f, 2566, 0.00055f, 0.0898f, 0.1192f, 7.6f, 0.0f, 7,
     {{"B", 0.01154f, 0.006f, 1.511f, 1.116f, 1.374f}, {"C", 0.0158f, 0.007f, 2.422f, 1.097f, 1.308f},
      {"D", 0.02227f, 0.008f, 4.049f, 0.788f, 0.388f}, {"E", 0.02925f, 0.005f, 6.101f, 0.920f, 0.692f},
      {"F", 0.03849f, 0.01f, 9.208f, 1.045f, 1.039f}, {"G", 0.04683f, 0.002f, 12.352f, 1.129f, 1.321f},
      {"H", 0.06189f, 0.006f, 18.773f, 0.755f, 0.326f}}, 0, 0},
    {"KEPLER-452", 19.7336f, 44.277f, 1400.0f, 5757, 1.2f, 1.04f, 1.11f, 6.0f, 0.0f, 1,
     {{"B", 1.046f, 0.0f, 384.8f, 1.63f, 5.0f}}, 0, 0},
    {"KEPLER-186", 19.910f, 43.955f, 580.0f, 3788, 0.04f, 0.54f, 0.52f, 4.0f, 0.0f, 5,
     {{"B", 0.0343f, 0.0f, 3.887f, 1.07f, 1.2f}, {"C", 0.0451f, 0.0f, 7.267f, 1.25f, 2.0f},
      {"D", 0.0781f, 0.0f, 13.34f, 1.40f, 2.5f}, {"E", 0.1432f, 0.0f, 22.41f, 1.27f, 2.0f},
      {"F", 0.432f, 0.04f, 129.94f, 1.17f, 1.7f}}, 0, 0},
    {"GLIESE 581", 15.324f, -7.722f, 20.4f, 3480, 0.013f, 0.30f, 0.30f, 8.0f, 0.0f, 3,
     {{"E", 0.0282f, 0.03f, 3.149f, 1.3f, 1.7f}, {"B", 0.0406f, 0.03f, 5.369f, 2.5f, 15.7f},
      {"C", 0.0721f, 0.07f, 12.92f, 1.9f, 5.5f}}, 0, 0},
    {"GLIESE 667 C", 17.316f, -34.99f, 23.6f, 3350, 0.0137f, 0.31f, 0.42f, 2.0f, 0.0f, 2,
     {{"B", 0.0504f, 0.0f, 7.2f, 1.9f, 5.4f}, {"C", 0.1251f, 0.02f, 28.1f, 1.6f, 3.8f}}, 0, 0},
    {"HD 209458", 22.053f, 18.884f, 159.0f, 6065, 1.6f, 1.12f, 1.16f, 3.5f, 0.0f, 1,
     {{"B", 0.04747f, 0.01f, 3.5247f, 15.1f, 219.0f}}, 0, 0},
    {"51 PEGASI", 22.957f, 20.769f, 50.9f, 5790, 1.37f, 1.11f, 1.27f, 4.8f, 0.0f, 1,
     {{"B", 0.0527f, 0.0f, 4.231f, 13.0f, 149.0f}}, 0, 0},
    {"KEPLER-22", 19.281f, 47.884f, 620.0f, 5518, 0.79f, 0.97f, 0.98f, 4.0f, 0.0f, 1,
     {{"B", 0.849f, 0.0f, 289.9f, 2.4f, 9.0f}}, 0, 0},
    {"LHS 1140", 0.7497f, -15.27f, 49.0f, 3216, 0.0038f, 0.19f, 0.21f, 5.0f, 0.0f, 2,
     {{"C", 0.0273f, 0.0f, 3.78f, 1.27f, 1.8f}, {"B", 0.0875f, 0.06f, 24.74f, 1.73f, 5.6f}}, 0, 0},
    {"TEEGARDEN'S STAR", 2.8836f, 16.88f, 12.5f, 2904, 0.00073f, 0.09f, 0.107f, 8.0f, 0.0f, 3,
     {{"B", 0.0259f, 0.0f, 4.91f, 1.0f, 1.05f}, {"C", 0.0455f, 0.0f, 11.4f, 1.0f, 1.11f}, {"D", 0.0791f, 0.0f, 26.1f, 0.9f, 0.82f}}, 0, 0},
    {"ROSS 128", 11.796f, 0.80f, 11.0f, 3190, 0.0036f, 0.168f, 0.197f, 9.5f, 0.0f, 1,
     {{"B", 0.0496f, 0.0f, 9.866f, 1.1f, 1.4f}}, 0, 0},
    {"WOLF 1061", 16.505f, -12.664f, 14.05f, 3400, 0.0013f, 0.25f, 0.31f, 8.0f, 0.0f, 3,
     {{"B", 0.0375f, 0.0f, 4.89f, 1.2f, 1.4f}, {"C", 0.0890f, 0.0f, 17.9f, 1.6f, 3.4f}, {"D", 0.47f, 0.0f, 217.0f, 2.0f, 7.7f}}, 0, 0},
    {"K2-18", 11.504f, 7.589f, 124.0f, 3457, 0.023f, 0.36f, 0.45f, 2.4f, 0.0f, 1,
     {{"B", 0.1429f, 0.0f, 32.94f, 2.61f, 8.6f}}, 0, 0},
};

const int COUNT = (int)(sizeof(STARS) / sizeof(STARS[0]));

void tempRgb(float t, float out[3]) {
    float k = std::clamp(t, 1500.0f, 40000.0f) / 100.0f;
    float r = k <= 66.0f ? 255.0f : 329.7f * std::pow(k - 60.0f, -0.1332f);
    float g = k <= 66.0f ? 99.47f * std::log(k) - 161.12f : 288.12f * std::pow(k - 60.0f, -0.0755f);
    float b = k >= 66.0f ? 255.0f : (k <= 19.0f ? 0.0f : 138.52f * std::log(k - 10.0f) - 305.04f);
    out[0] = std::clamp(r, 0.0f, 255.0f) / 255.0f;
    out[1] = std::clamp(g, 0.0f, 255.0f) / 255.0f;
    out[2] = std::clamp(b, 0.0f, 255.0f) / 255.0f;
    float m = std::max(out[0], std::max(out[1], out[2]));
    for (int a = 0; a < 3; a++) out[a] = 0.25f + 0.75f * out[a] / m;
}

int classOfTemp(float t) {
    return t > 10000.0f ? 0 : t > 7500.0f ? 1 : t > 6000.0f ? 2 : t > 5200.0f ? 3 : t > 3900.0f ? 4 : 5;
}

}

int realStarCount() { return COUNT; }

const RealStar& realStarAt(int i) { return STARS[i]; }

void realStarPos(int i, float out[3]) {
    const RealStar& s = STARS[i];
    const double d2r = 3.14159265358979323846 / 180.0;
    double a = s.raH * 15.0 * d2r, dec = s.decDeg * d2r;
    double an = 192.85948 * d2r, dn = 27.12825 * d2r, ln = 122.93192 * d2r;
    double sb = std::sin(dec) * std::sin(dn) + std::cos(dec) * std::cos(dn) * std::cos(a - an);
    double b = std::asin(sb);
    double l = ln - std::atan2(std::cos(dec) * std::sin(a - an), std::sin(dec) * std::cos(dn) - std::cos(dec) * std::sin(dn) * std::cos(a - an));
    float g[3] = {(float)(s.distLy * std::cos(b) * std::cos(l) + s.sepLy), (float)(s.distLy * std::cos(b) * std::sin(l)), (float)(s.distLy * sb)};
    galaxySunToSim(g, out);
}

std::vector<GalaxyPoint> realStarPoints() {
    std::vector<GalaxyPoint> pts;
    for (int i = 0; i < COUNT; i++) {
        GalaxyPoint p{};
        realStarPos(i, p.pos);
        p.bright = 3.4f + 1.0f * std::log10(1.0f + STARS[i].lum);
        tempRgb(STARS[i].tempK, p.color);
        pts.push_back(p);
    }
    return pts;
}

StarSystem realSystem(int i) {
    const RealStar& r = STARS[i];
    float col[3];
    tempRgb(r.tempK, col);
    int cls = classOfTemp(r.tempK);
    StarSystem s = generateSystem(1000u + (uint32_t)i, cls, col);
    s.real = i;
    s.mass = r.mass;
    s.lum = r.lum;
    s.radiusSun = r.radius;
    s.ageGyr = r.ageGyr;
    s.lifeGyr = std::max(r.ageGyr * 1.2f, 10.0f * std::pow(r.mass, -2.5f));
    float sl = std::sqrt(r.lum);
    s.hzIn = 0.95f * sl;
    s.hzOut = 1.37f * sl;
    s.planetCount = r.planetCount;
    s.moonCount = 0;
    s.ring = false;
    float last = 0.0f, inner = 0.05f;
    for (int k = 0; k < r.planetCount; k++) {
        const RealPlanet& rp = r.planets[k];
        PlanetSpec p{};
        p.au = rp.au;
        p.ecc = rp.ecc;
        p.incl = 0.01f * (float)k;
        p.node = 1.3f * (float)k;
        p.phase = 2.1f * (float)k + 0.7f;
        p.mass = rp.massEarth * 0.003f;
        p.radiusKm = rp.radiusEarth * 6371.0f;
        p.name = rp.name;
        bool inHz = rp.au >= 0.98f * s.hzIn && rp.au <= 1.02f * s.hzOut;
        float gas[3] = {0.9f, 0.75f, 0.55f}, ice[3] = {0.5f, 0.75f, 1.0f}, rock[3] = {0.85f, 0.75f, 0.68f};
        const float* t = rock;
        if (rp.radiusEarth >= 8.0f) { p.type = 10; t = gas; }
        else if (rp.radiusEarth >= 2.0f) { p.type = 11; t = ice; }
        else if (inHz && rp.massEarth <= 5.0f) { p.type = 3; p.habitable = true; }
        else if (rp.au < 0.8f * s.hzIn) p.type = rp.au < 0.5f * s.hzIn ? 2 : 9;
        else p.type = 4;
        for (int a = 0; a < 3; a++) p.tint[a] = p.type == 3 ? 1.0f : t[a];
        p.tilt = 0.0f;
        p.day = rp.periodDays < 20.0f ? rp.periodDays : 1.0f;
        s.planets[k] = p;
        last = std::max(last, rp.au);
        if (k == 0) inner = rp.au;
    }
    s.au0 = std::min(0.05f, 0.9f * inner);
    if (r.planetCount == 0) {
        float sc = std::max(std::min(sl, 60.0f), 0.3f);
        s.beltLo = 2.2f * sc;
        s.beltHi = 3.2f * sc;
        if (r.beltLo > 0.0f) { s.beltLo = r.beltLo; s.beltHi = r.beltHi; }
        last = s.beltHi;
    } else {
        s.beltLo = s.beltHi = 0.0f;
    }
    s.refAu = last;
    s.outerLo = 1.35f * last;
    s.outerHi = 2.1f * last;
    return s;
}
