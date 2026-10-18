#include "galaxygen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

namespace {

constexpr float PI = 3.14159265358979f;

struct ClassMix { float w[6]; };

const ClassMix MIX_OLD = {{0.0f, 0.01f, 0.05f, 0.10f, 0.30f, 0.54f}};
const ClassMix MIX_RED = {{0.0f, 0.0f, 0.02f, 0.08f, 0.35f, 0.55f}};
const ClassMix MIX_DISC = {{0.01f, 0.04f, 0.10f, 0.15f, 0.25f, 0.45f}};
const ClassMix MIX_ARM = {{0.12f, 0.15f, 0.13f, 0.12f, 0.18f, 0.30f}};
const ClassMix MIX_KNOT = {{0.50f, 0.30f, 0.10f, 0.05f, 0.03f, 0.02f}};
const ClassMix MIX_IRR = {{0.22f, 0.20f, 0.14f, 0.12f, 0.14f, 0.18f}};

struct V3 { float x, y, z; };

V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 norm(V3 a) {
    float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return l > 1e-12f ? V3{a.x / l, a.y / l, a.z / l} : V3{0.0f, 0.0f, 1.0f};
}

const char* const BARRED[2] = {"SPIRAL", "BARRED SPIRAL"};

struct Named {
    const char* name;
    const char* type;
    GalType t;
    float radius;
    float bh;
    float stars;
    int arms;
    float pitch;
    bool bar;
};

const Named NAMED[4] = {
    {"ANDROMEDA (M31)", "BARRED SPIRAL", GalType::SPIRAL, 110000.0f, 1.4e8f, 1000.0f, 2, 14.0f, true},
    {"TRIANGULUM (M33)", "SPIRAL (FLOCCULENT)", GalType::SPIRAL, 30000.0f, 0.0f, 40.0f, 2, 22.0f, false},
    {"LARGE MAGELLANIC CLOUD", "BARRED IRREGULAR", GalType::IRREGULAR, 7000.0f, 6.0e5f, 30.0f, 0, 0.0f, true},
    {"SMALL MAGELLANIC CLOUD", "IRREGULAR", GalType::IRREGULAR, 3500.0f, 0.0f, 3.0f, 0, 0.0f, false}};

}

std::string fmtMass(float m) {
    char buf[48];
    if (m <= 0.0f) return "NONE DETECTED";
    if (m < 1.0e6f) std::snprintf(buf, sizeof(buf), "%.0f THOUSAND SUNS", m / 1.0e3f);
    else if (m < 1.0e9f) std::snprintf(buf, sizeof(buf), m < 1.0e7f ? "%.1f MILLION SUNS" : "%.0f MILLION SUNS", m / 1.0e6f);
    else std::snprintf(buf, sizeof(buf), "%.1f BILLION SUNS", m / 1.0e9f);
    return buf;
}

GalaxySpec galaxySpec(uint32_t index, int named, const double centreMly[3]) {
    GalaxySpec S;
    S.index = index;
    S.named = named;
    for (int a = 0; a < 3; a++) S.centreMly[a] = centreMly[a];
    S.distMly = (float)std::sqrt(centreMly[0] * centreMly[0] + centreMly[1] * centreMly[1] + centreMly[2] * centreMly[2]);
    S.seed = index * 2654435761u ^ 0x5bd1e995u;
    std::mt19937 rng(S.seed);
    auto u = [&]() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng); };
    auto g = [&]() { return std::normal_distribution<float>(0.0f, 1.0f)(rng); };
    float tsel = u();
    S.type = tsel < 0.60f ? GalType::SPIRAL : tsel < 0.85f ? GalType::ELLIPTICAL : tsel < 0.95f ? GalType::IRREGULAR : GalType::LENTICULAR;
    S.radiusLy = 10000.0f * std::pow(10.0f, u());
    float armSel = u();
    S.arms = armSel < 0.45f ? 2 : armSel < 0.80f ? 3 : 4;
    S.pitchDeg = 10.0f + 15.0f * u();
    S.bar = u() < 0.45f;
    S.ellN = (int)(8.0f * std::pow(u(), 1.6f));
    float bhJitter = std::exp(0.8f * g());
    V3 n = norm({g(), g(), g()});
    V3 h = std::fabs(n.y) < 0.9f ? V3{0.0f, 1.0f, 0.0f} : V3{1.0f, 0.0f, 0.0f};
    V3 e1 = norm(cross(n, h)), e2 = cross(n, e1);
    S.normal[0] = n.x; S.normal[1] = n.y; S.normal[2] = n.z;
    S.e1[0] = e1.x; S.e1[1] = e1.y; S.e1[2] = e1.z;
    S.e2[0] = e2.x; S.e2[1] = e2.y; S.e2[2] = e2.z;
    float scale = S.radiusLy / MW_RADIUS_LY;
    float tf = S.type == GalType::ELLIPTICAL ? 10.0f : S.type == GalType::LENTICULAR ? 3.0f : S.type == GalType::IRREGULAR ? 0.1f : 1.0f;
    float sf = S.type == GalType::ELLIPTICAL ? 2.0f : S.type == GalType::IRREGULAR ? 0.1f : 1.0f;
    S.bhMsun = std::min(2.0e10f, std::max(2.0e5f, SGR_MSUN * std::pow(scale, 2.5f) * tf * bhJitter));
    S.starsBillion = 200.0f * std::pow(scale, 2.5f) * sf;
    char nm[32];
    std::snprintf(nm, sizeof(nm), "GALAXY #%u", index);
    S.name = nm;
    char ty[40];
    switch (S.type) {
        case GalType::SPIRAL: std::snprintf(ty, sizeof(ty), "%s, %d ARMS", BARRED[S.bar ? 1 : 0], S.arms); break;
        case GalType::ELLIPTICAL: std::snprintf(ty, sizeof(ty), "ELLIPTICAL E%d", S.ellN); break;
        case GalType::IRREGULAR: std::snprintf(ty, sizeof(ty), "IRREGULAR"); break;
        default: std::snprintf(ty, sizeof(ty), "LENTICULAR"); break;
    }
    S.typeName = ty;
    if (named >= 1 && named <= 4) {
        const Named& N = NAMED[named - 1];
        S.name = N.name;
        S.typeName = N.type;
        S.type = N.t;
        S.radiusLy = N.radius;
        S.bhMsun = N.bh;
        S.starsBillion = N.stars;
        S.arms = N.arms;
        S.pitchDeg = N.pitch;
        S.bar = N.bar;
    }
    return S;
}

std::vector<GalaxyPoint> generateGalaxy(GalaxySpec& S, unsigned count) {
    std::mt19937 rng(S.seed ^ 0xA5F1C3u);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    std::normal_distribution<float> gauss(0.0f, 1.0f);
    std::vector<GalaxyPoint> pts;
    pts.reserve(count);
    const float R = S.radiusLy, s = R / MW_RADIUS_LY;
    const V3 n{S.normal[0], S.normal[1], S.normal[2]}, e1{S.e1[0], S.e1[1], S.e1[2]}, e2{S.e2[0], S.e2[1], S.e2[2]};
    auto pickClass = [&](const ClassMix& m) {
        float u = uni(rng), acc = 0.0f;
        for (int c = 0; c < 5; c++) { acc += m.w[c]; if (u < acc) return c; }
        return 5;
    };
    auto push = [&](float x, float y, float z, float base, const ClassMix& mix) {
        int c = pickClass(mix);
        float lum = SPECTRAL[c].lum * (0.7f + 0.6f * uni(rng));
        float r = SPECTRAL[c].rgb[0], g = SPECTRAL[c].rgb[1], b = SPECTRAL[c].rgb[2];
        if (c == 5) {
            float tint = uni(rng);
            g = 0.80f - 0.14f * tint; b = 0.44f - 0.12f * tint; lum *= 1.0f - 0.4f * tint;
        }
        GalaxyPoint p;
        p.pos[0] = e1.x * x + e2.x * y + n.x * z;
        p.pos[1] = e1.y * x + e2.y * y + n.y * z;
        p.pos[2] = e1.z * x + e2.z * y + n.z * z;
        p.bright = base * lum;
        p.color[0] = r; p.color[1] = g; p.color[2] = b;
        pts.push_back(p);
    };
    auto sphere = [&](float& x, float& y, float& z) {
        float u = 2.0f * uni(rng) - 1.0f, ph = 2.0f * PI * uni(rng), sn = std::sqrt(1.0f - u * u);
        x = sn * std::cos(ph); y = sn * std::sin(ph); z = u;
    };
    if (S.type == GalType::SPIRAL || S.type == GalType::LENTICULAR) {
        bool lent = S.type == GalType::LENTICULAR;
        float bulgeF = lent ? 0.28f : 0.10f + 0.14f * uni(rng);
        unsigned nBulge = (unsigned)(count * bulgeF), nBar = (!lent && S.bar) ? count * 7 / 100 : 0;
        unsigned nKnot = lent ? 0 : count * 5 / 100, nDisc = lent ? count - nBulge : count * 22 / 100;
        unsigned nArms = lent ? 0 : count - nBulge - nBar - nDisc - nKnot;
        float tanP = std::tan(S.pitchDeg * PI / 180.0f);
        float barAng = 2.0f * PI * uni(rng), barHalf = 0.2f * R;
        float cb = std::cos(barAng), sb = std::sin(barAng);
        float rIn = S.bar ? 0.18f * R : 0.08f * R;
        float weights[4] = {1.0f, 1.0f, 0.6f, 0.6f};
        float wsum = 0.0f;
        for (int k = 0; k < S.arms; k++) wsum += weights[k];
        float bs = 2600.0f * s * (lent ? 1.4f : 0.7f + 0.6f * uni(rng));
        for (unsigned i = 0; i < nBulge; i++) {
            float r = bs * std::fabs(gauss(rng)) * std::sqrt(1.0f + uni(rng));
            float x, y, z;
            sphere(x, y, z);
            push(r * x, r * y, r * z * 0.7f, 2.4f, MIX_OLD);
        }
        for (unsigned i = 0; i < nBar; i++) {
            float t = (2.0f * uni(rng) - 1.0f) * barHalf * (0.4f + 0.6f * uni(rng));
            float w = 900.0f * s * gauss(rng);
            push(t * cb - w * sb, t * sb + w * cb, 450.0f * s * gauss(rng), 2.0f, MIX_OLD);
        }
        float dScale = lent ? 0.22f * R : 10000.0f * s;
        for (unsigned i = 0; i < nDisc; i++) {
            float r = -dScale * std::log(1.0f - uni(rng) * 0.97f) * 0.6f - dScale * 0.6f * std::log(1.0f - uni(rng) * 0.97f) * 0.4f;
            if (r > R) { i--; continue; }
            float th = 2.0f * PI * uni(rng);
            push(r * std::cos(th), r * std::sin(th), (lent ? 300.0f : 400.0f) * s * gauss(rng),
                 r < 0.28f * R ? 1.0f : 0.8f, lent ? MIX_OLD : (r < 0.28f * R ? MIX_OLD : MIX_DISC));
        }
        auto armSample = [&](float& x, float& y, float& z) {
            float pick = uni(rng) * wsum, acc = 0.0f;
            int k = S.arms - 1;
            for (int j = 0; j < S.arms; j++) { acc += weights[j]; if (pick <= acc) { k = j; break; } }
            float r;
            for (;;) {
                r = 0.5f * rIn - 0.3f * R * std::log(1.0f - uni(rng) * 0.995f);
                if (r < R) break;
            }
            float th = barAng + 2.0f * PI * k / S.arms + std::log(r / rIn) / tanP;
            float rr = r + (350.0f * s + 0.045f * r) * gauss(rng);
            x = rr * std::cos(th);
            y = rr * std::sin(th);
            z = (250.0f * s + 0.01f * r) * gauss(rng);
        };
        for (unsigned i = 0; i < nArms; i++) {
            float x, y, z;
            armSample(x, y, z);
            push(x, y, z, 0.9f, MIX_ARM);
        }
        for (unsigned i = 0; i < nKnot; i++) {
            float x, y, z;
            armSample(x, y, z);
            push(x, y, z, 0.8f, MIX_KNOT);
        }
    } else if (S.type == GalType::ELLIPTICAL) {
        float q = 1.0f - 0.1f * (float)S.ellN, a = R / 4.5f;
        for (unsigned i = 0; i < count; i++) {
            float u = std::sqrt(uni(rng) * 0.9999f);
            float r = a * u / (1.0f - u);
            if (r > R) { i--; continue; }
            float x, y, z;
            sphere(x, y, z);
            push(r * x, r * y * q, r * z * q, 2.6f, MIX_RED);
        }
    } else {
        int clumps = 6 + (int)(uni(rng) * 5.0f);
        struct Clump { float x, y, z, sig, w; int young; };
        std::vector<Clump> cl;
        float wsum = 0.0f;
        for (int k = 0; k < clumps; k++) {
            float r = 0.7f * R * std::sqrt(uni(rng)), th = 2.0f * PI * uni(rng);
            float w = 0.4f + uni(rng);
            cl.push_back({r * std::cos(th), r * std::sin(th) * 0.8f, 0.05f * R * gauss(rng), R * (0.04f + 0.08f * uni(rng)), w, uni(rng) < 0.6f ? 1 : 0});
            wsum += w;
        }
        float barAng = 2.0f * PI * uni(rng), cb = std::cos(barAng), sb = std::sin(barAng);
        for (unsigned i = 0; i < count; i++) {
            float roll = uni(rng);
            if (roll < 0.35f) {
                float x = R * 0.4f * gauss(rng), y = R * 0.4f * gauss(rng);
                if (S.bar && S.named == 3) { float t = R * 0.5f * (2.0f * uni(rng) - 1.0f), w = R * 0.09f * gauss(rng); x = t * cb - w * sb; y = t * sb + w * cb; }
                push(x, y, R * 0.06f * gauss(rng), 0.45f, MIX_DISC);
                continue;
            }
            float pick = uni(rng) * wsum, acc = 0.0f;
            const Clump* c = &cl.back();
            for (const Clump& k : cl) { acc += k.w; if (pick <= acc) { c = &k; break; } }
            push(c->x + c->sig * gauss(rng), c->y + c->sig * gauss(rng), c->z + 0.5f * c->sig * gauss(rng),
                 c->young ? 0.5f : 0.4f, c->young ? (uni(rng) < 0.4f ? MIX_KNOT : MIX_IRR) : MIX_DISC);
        }
    }
    float lo = S.type == GalType::IRREGULAR ? 0.08f : 0.28f, hi = S.type == GalType::IRREGULAR ? 0.4f : 0.45f;
    size_t start = (size_t)(uni(rng) * (float)pts.size());
    size_t pick = start;
    for (size_t k = 0; k < pts.size(); k++) {
        size_t i = (start + k) % pts.size();
        const float* p = pts[i].pos;
        float r = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        if (r >= lo * R && r <= hi * R) { pick = i; break; }
    }
    for (int a = 0; a < 3; a++) S.anchor[a] = pts[pick].pos[a] + 0.3f * (uni(rng) - 0.5f);
    for (GalaxyPoint& p : pts)
        for (int a = 0; a < 3; a++) p.pos[a] -= S.anchor[a];
    return pts;
}
