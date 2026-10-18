#include "milkyway.h"
#include "realstars.h"
#include <cmath>
#include <random>

namespace {

constexpr float PI = 3.14159265358979f;
constexpr float SUN_R = 26000.0f;
constexpr float SUN_Z = 65.0f;
constexpr float DISC_R = 50000.0f;
constexpr float PITCH_K = 0.2126f;
constexpr float BAR_ANGLE = 153.0f * PI / 180.0f;
constexpr float BAR_HALF = 10000.0f;

struct Arm {
    const char* name;
    float refAngle;
    float rIn;
    float weight;
    float width;
};

const Arm ARMS[4] = {
    {"PERSEUS ARM", 119.9f * PI / 180.0f, 7000.0f, 1.0f, 1.0f},
    {"SCUTUM-CENTAURUS ARM", 299.9f * PI / 180.0f, 7000.0f, 1.0f, 1.0f},
    {"SAGITTARIUS ARM", 231.3f * PI / 180.0f, 7500.0f, 0.55f, 0.8f},
    {"NORMA ARM", 51.3f * PI / 180.0f, 7500.0f, 0.55f, 0.8f}};

void cross3(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void toSim(const float g[3], float out[3]) {
    static float ex[3], ey[3], ez[3];
    static bool ready = false;
    if (!ready) {
        const float d = PI / 180.0f;
        float lp = 96.38f * d, bp = 29.81f * d, le = 96.34f * d, be = -60.19f * d;
        ey[0] = std::cos(bp) * std::cos(lp); ey[1] = std::cos(bp) * std::sin(lp); ey[2] = std::sin(bp);
        ex[0] = std::cos(be) * std::cos(le); ex[1] = std::cos(be) * std::sin(le); ex[2] = std::sin(be);
        cross3(ex, ey, ez);
        ready = true;
    }
    out[0] = g[0] * ex[0] + g[1] * ex[1] + g[2] * ex[2];
    out[1] = g[0] * ey[0] + g[1] * ey[1] + g[2] * ey[2];
    out[2] = g[0] * ez[0] + g[1] * ez[1] + g[2] * ez[2];
}

float armAngle(const Arm& arm, float r) {
    return arm.refAngle + std::log(r / SUN_R) / PITCH_K;
}

void armPoint(const Arm& arm, float r, float out[3]) {
    float th = armAngle(arm, r);
    out[0] = r * std::cos(th);
    out[1] = r * std::sin(th);
    out[2] = 0.0f;
}

struct ClassMix { float w[6]; };

const ClassMix MIX_OLD = {{0.0f, 0.01f, 0.05f, 0.10f, 0.30f, 0.54f}};
const ClassMix MIX_DISC = {{0.01f, 0.04f, 0.10f, 0.15f, 0.25f, 0.45f}};
const ClassMix MIX_ARM = {{0.12f, 0.15f, 0.13f, 0.12f, 0.18f, 0.30f}};
const ClassMix MIX_KNOT = {{0.50f, 0.30f, 0.10f, 0.05f, 0.03f, 0.02f}};

}

const SpectralClass SPECTRAL[6] = {
    {"O/B", "3-100 MYR", {0.608f, 0.690f, 1.0f}, 7.0f},
    {"A", "~1 GYR", {0.792f, 0.843f, 1.0f}, 2.2f},
    {"F", "~3 GYR", {0.973f, 0.969f, 1.0f}, 1.0f},
    {"G", "~10 GYR (SUN)", {1.0f, 0.957f, 0.918f}, 0.6f},
    {"K", "20-50 GYR", {1.0f, 0.824f, 0.631f}, 0.4f},
    {"M", "TRILLIONS OF YRS", {1.0f, 0.690f, 0.420f}, 0.18f}};

std::vector<GalaxyPoint> makeMilkyWay(unsigned count, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    std::normal_distribution<float> gauss(0.0f, 1.0f);
    std::vector<GalaxyPoint> pts;
    pts.reserve(count);
    auto pickClass = [&](const ClassMix& m) {
        float u = uni(rng), acc = 0.0f;
        for (int c = 0; c < 5; c++) { acc += m.w[c]; if (u < acc) return c; }
        return 5;
    };
    auto push = [&](float gx, float gy, float gz, float base, const ClassMix& mix) {
        int c = pickClass(mix);
        float lum = SPECTRAL[c].lum * (0.7f + 0.6f * uni(rng));
        float tint = c == 5 ? uni(rng) : 0.0f;
        float r = SPECTRAL[c].rgb[0], g = SPECTRAL[c].rgb[1], b = SPECTRAL[c].rgb[2];
        if (c == 5) { g = 0.80f - 0.14f * tint; b = 0.44f - 0.12f * tint; lum *= 1.0f - 0.4f * tint; }
        float bright = base * lum;
        float rel[3] = {gx + SUN_R, gy, gz - SUN_Z};
        GalaxyPoint p;
        toSim(rel, p.pos);
        p.bright = bright;
        p.color[0] = r; p.color[1] = g; p.color[2] = b;
        pts.push_back(p);
    };
    unsigned nBulge = count * 14 / 100, nBar = count * 7 / 100, nDisc = count * 22 / 100;
    unsigned nSpur = count * 2 / 100, nKnot = count * 5 / 100;
    unsigned nArms = count - nBulge - nBar - nDisc - nSpur - nKnot;
    float armWeight = 0.0f;
    for (const Arm& a : ARMS) armWeight += a.weight;
    float cb = std::cos(BAR_ANGLE), sb = std::sin(BAR_ANGLE);
    for (unsigned i = 0; i < nBulge; i++) {
        float r = 2600.0f * std::fabs(gauss(rng)) * std::sqrt(1.0f + uni(rng));
        float u = 2.0f * uni(rng) - 1.0f, ph = 2.0f * PI * uni(rng), s = std::sqrt(1.0f - u * u);
        push(r * s * std::cos(ph), r * s * std::sin(ph), r * u * 0.7f, 2.4f, MIX_OLD);
    }
    for (unsigned i = 0; i < nBar; i++) {
        float t = (2.0f * uni(rng) - 1.0f) * BAR_HALF * (0.4f + 0.6f * uni(rng));
        float w = 900.0f * gauss(rng);
        float x = t * cb - w * sb, y = t * sb + w * cb;
        push(x, y, 450.0f * gauss(rng), 2.0f, MIX_OLD);
    }
    for (unsigned i = 0; i < nDisc; i++) {
        float r = -10000.0f * std::log(1.0f - uni(rng) * 0.97f) * 0.6f - 6000.0f * std::log(1.0f - uni(rng) * 0.97f) * 0.4f;
        if (r > DISC_R) { i--; continue; }
        float th = 2.0f * PI * uni(rng);
        push(r * std::cos(th), r * std::sin(th), 400.0f * gauss(rng), r < 14000.0f ? 1.0f : 0.8f,
             r < 14000.0f ? MIX_OLD : MIX_DISC);
    }
    auto armSample = [&](const Arm& arm, float& x, float& y, float& z, float& r) {
        for (;;) {
            r = arm.rIn - 15000.0f * std::log(1.0f - uni(rng) * 0.995f);
            if (r < DISC_R) break;
        }
        float th = armAngle(arm, r);
        float perp = (350.0f + 0.045f * r) * arm.width * gauss(rng);
        float rr = r + perp;
        x = rr * std::cos(th);
        y = rr * std::sin(th);
        z = (250.0f + 0.01f * r) * gauss(rng);
    };
    for (unsigned i = 0; i < nArms; i++) {
        float pick = uni(rng) * armWeight, acc = 0.0f;
        const Arm* arm = &ARMS[3];
        for (const Arm& a : ARMS) { acc += a.weight; if (pick <= acc) { arm = &a; break; } }
        float x, y, z, r;
        armSample(*arm, x, y, z, r);
        push(x, y, z, 0.9f, MIX_ARM);
    }
    for (unsigned i = 0; i < nKnot; i++) {
        const Arm& arm = ARMS[(unsigned)(uni(rng) * 3.999f)];
        float cx, cy, cz, r;
        armSample(arm, cx, cy, cz, r);
        push(cx, cy, cz, 0.8f, MIX_KNOT);
    }
    float th = 180.0f * PI / 180.0f;
    float tx = std::cos(th) - std::sin(th) / PITCH_K, ty = std::sin(th) + std::cos(th) / PITCH_K;
    float tl = std::sqrt(tx * tx + ty * ty);
    tx /= tl; ty /= tl;
    for (unsigned i = 0; i < nSpur; i++) {
        float t = -2500.0f + 9000.0f * uni(rng);
        float w = 1400.0f * gauss(rng);
        push(-SUN_R + t * tx - w * ty, t * ty + w * tx, 350.0f * gauss(rng), 0.45f, MIX_ARM);
    }
    for (const GalaxyPoint& r : realStarPoints()) pts.push_back(r);
    return pts;
}

std::vector<GalaxyLabel> milkyWayLabels() {
    std::vector<GalaxyLabel> out;
    auto add = [&](const char* text, float gx, float gy, float gz, float r, float g, float b) {
        float rel[3] = {gx + SUN_R, gy, gz - SUN_Z};
        GalaxyLabel l;
        l.text = text;
        toSim(rel, l.pos);
        l.color[0] = r; l.color[1] = g; l.color[2] = b;
        out.push_back(l);
    };
    add("SUN - YOU ARE HERE", -SUN_R, 0.0f, SUN_Z, 1.0f, 0.9f, 0.3f);
    add("SGR A* (GALACTIC CENTRE)", 0.0f, 0.0f, 0.0f, 1.0f, 0.75f, 0.45f);
    const float radii[4] = {40000.0f, 40000.0f, 40000.0f, 40000.0f};
    for (int i = 0; i < 4; i++) {
        float p[3];
        armPoint(ARMS[i], radii[i], p);
        add(ARMS[i].name, p[0], p[1], 0.0f, 0.6f, 0.8f, 1.0f);
    }
    float th = PI;
    float tx = std::cos(th) - std::sin(th) / PITCH_K, ty = std::sin(th) + std::cos(th) / PITCH_K;
    float tl = std::sqrt(tx * tx + ty * ty);
    add("ORION SPUR", -SUN_R + 6500.0f * tx / tl, 6500.0f * ty / tl, 0.0f, 0.7f, 0.85f, 1.0f);
    return out;
}

void galaxyNormal(float out[3]) {
    const float up[3] = {0.0f, 0.0f, 1.0f};
    toSim(up, out);
}

void galaxySunToSim(const float g[3], float out[3]) {
    toSim(g, out);
}

void galaxyCentreLy(float out[3]) {
    const float gc[3] = {SUN_R, 0.0f, -SUN_Z};
    toSim(gc, out);
}
