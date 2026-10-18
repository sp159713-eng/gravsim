#include "universe.h"
#include "milkyway.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

namespace {

constexpr float PI = 3.14159265f;

struct V3 { float x, y, z; };
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float len(V3 a) { return std::sqrt(dot(a, a)); }
V3 norm(V3 a) { float l = len(a); return l > 1e-12f ? a * (1.0f / l) : V3{0.0f, 0.0f, 1.0f}; }

void basis(V3 n, V3& e1, V3& e2) {
    V3 h = std::fabs(n.y) < 0.9f ? V3{0.0f, 1.0f, 0.0f} : V3{1.0f, 0.0f, 0.0f};
    e1 = norm(cross(n, h));
    e2 = cross(n, e1);
}

V3 centreMly() {
    float c[3];
    galaxyCentreLy(c);
    return {c[0] * 1e-6f, c[1] * 1e-6f, c[2] * 1e-6f};
}

V3 galPos(float lDeg, float bDeg, float dMly) {
    const float d = PI / 180.0f;
    float g[3] = {dMly * std::cos(bDeg * d) * std::cos(lDeg * d), dMly * std::cos(bDeg * d) * std::sin(lDeg * d), dMly * std::sin(bDeg * d)};
    float s[3];
    galaxySunToSim(g, s);
    return V3{s[0], s[1], s[2]} - centreMly();
}

V3 tilted(V3 pos, float inclDeg, float paDeg) {
    V3 dir = norm(pos), u, v;
    basis(dir, u, v);
    float i = inclDeg * PI / 180.0f, p = paDeg * PI / 180.0f;
    return norm(dir * std::cos(i) + (u * std::cos(p) + v * std::sin(p)) * std::sin(i));
}

struct Gen {
    std::mt19937 rng{20260919};
    std::vector<UniPoint>& out;
    std::vector<UniGalaxy>& gals;
    float fadeD = 1e9f;
    bool autoGal = false;
    Gen(std::vector<UniPoint>& o, std::vector<UniGalaxy>& g) : out(o), gals(g) {}
    float u() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng); }
    float g() { return std::normal_distribution<float>(0.0f, 1.0f)(rng); }
    V3 sphere() {
        float z = 2.0f * u() - 1.0f, ph = 2.0f * PI * u(), s = std::sqrt(1.0f - z * z);
        return {s * std::cos(ph), s * std::sin(ph), z};
    }
    void add(V3 p, float radius, V3 col, float bright) {
        if (autoGal) gals.push_back({{p.x, p.y, p.z}, fadeD, (uint32_t)out.size(), 0});
        out.push_back({{p.x, p.y, p.z}, radius, {col.x, col.y, col.z}, bright, fadeD});
    }
    void named(V3 p, int id) {
        gals.push_back({{p.x, p.y, p.z}, fadeD, (uint32_t)(out.size() - 1), id});
    }
    V3 galColour() {
        float t = u();
        return t < 0.6f ? V3{1.0f, 0.82f + 0.1f * u(), 0.58f + 0.15f * u()} : V3{0.72f + 0.1f * u(), 0.82f, 1.0f};
    }
};

void spiral(Gen& G, V3 c, V3 n, float R, unsigned count, int arms, float bulgeFrac, float bright, float ptR) {
    V3 e1, e2;
    basis(n, e1, e2);
    const V3 armCol = {0.62f, 0.76f, 1.0f}, coreCol = {1.0f, 0.82f, 0.55f};
    for (unsigned i = 0; i < count; i++) {
        if (G.u() < bulgeFrac) {
            V3 p = e1 * (G.g() * R * 0.09f) + e2 * (G.g() * R * 0.09f) + n * (G.g() * R * 0.05f);
            G.add(c + p, ptR, coreCol, bright * (0.7f + 0.6f * G.u()));
            continue;
        }
        float r = R * std::min(1.0f, -std::log(1.0f - G.u() * 0.97f) * 0.3f);
        int k = (int)(G.u() * arms * 0.9999f);
        float th = 2.0f * PI * k / arms + 3.2f * std::log(1.0f + r / (0.18f * R)) + G.g() * 0.3f;
        float mixc = std::exp(-r / (0.3f * R));
        V3 col = armCol * (1.0f - mixc) + coreCol * mixc;
        V3 p = e1 * (r * std::cos(th)) + e2 * (r * std::sin(th)) + n * (G.g() * R * 0.012f);
        G.add(c + p, ptR, col, bright * (0.7f + 0.6f * G.u()));
    }
    G.add(c, R * 0.3f, coreCol, bright * 4.0f);
}

void blob(Gen& G, V3 c, float sigma, unsigned count, float ptR, V3 col, float bright, float bar = 0.0f) {
    V3 ax = G.sphere();
    for (unsigned i = 0; i < count; i++) {
        V3 p = {G.g() * sigma, G.g() * sigma, G.g() * sigma};
        p = p + ax * (G.g() * sigma * bar);
        G.add(c + p, ptR, col, bright * (0.6f + 0.8f * G.u()));
    }
}

void cluster(Gen& G, V3 c, float a, unsigned count, float ptR, float bright) {
    G.autoGal = true;
    for (unsigned i = 0; i < count; i++) {
        float x = std::max(G.u(), 1e-4f);
        float r = std::min(a / std::sqrt(std::pow(x, -2.0f / 3.0f) - 1.0f + 1e-4f), 6.0f * a);
        G.add(c + G.sphere() * r, ptR, G.galColour(), bright * (0.5f + G.u()));
    }
    G.autoGal = false;
}

void filament(Gen& G, V3 a, V3 b, float bendMly, unsigned count, float ptR, float bright) {
    G.autoGal = true;
    V3 d = norm(b - a), e1, e2;
    basis(d, e1, e2);
    V3 mid = (a + b) * 0.5f + (e1 * G.g() + e2 * G.g()) * bendMly;
    for (unsigned i = 0; i < count; i++) {
        float t = std::pow(G.u(), 0.6f);
        float s = 1.0f - t;
        V3 p = a * (s * s) + mid * (2.0f * s * t) + b * (t * t);
        float sig = 4.0f + 9.0f * t;
        p = p + V3{G.g() * sig, G.g() * sig, G.g() * sig};
        G.add(p, ptR, G.galColour(), bright * (0.5f + G.u()));
    }
    G.autoGal = false;
}

void sheet(Gen& G, V3 c, V3 axis, float length, float width, float thick, unsigned count, float ptR, float bright, V3 col) {
    G.autoGal = true;
    V3 a = norm(axis), n = norm(c), e1, e2;
    a = norm(a - n * dot(a, n));
    e2 = cross(n, a);
    e1 = a;
    const int K = 70;
    V3 cl[K];
    for (int k = 0; k < K; k++) cl[k] = e1 * ((G.u() - 0.5f) * length) + e2 * ((G.u() - 0.5f) * width);
    for (unsigned i = 0; i < count; i++) {
        V3 p;
        if (G.u() < 0.65f) p = cl[(int)(G.u() * (K - 0.001f))] + V3{G.g(), G.g(), G.g()} * (0.035f * length);
        else p = e1 * ((G.u() - 0.5f) * length) + e2 * ((G.u() - 0.5f) * width);
        p = p + n * (G.g() * thick);
        G.add(c + p, ptR, col * (0.85f + 0.3f * G.u()), bright * (0.5f + G.u()));
    }
    G.autoGal = false;
}

uint32_t hash3(int x, int y, int z) {
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; h *= 0x27d4eb2fu; h ^= h >> 16;
    return h;
}

V3 seedOf(int x, int y, int z) {
    uint32_t h = hash3(x, y, z);
    auto f = [](uint32_t v) { return 0.12f + 0.76f * ((v & 1023u) / 1023.0f); };
    return {(float)x + f(h), (float)y + f(h >> 10), (float)z + f(h >> 20)};
}

float faceGap(V3 q) {
    int cx = (int)std::floor(q.x), cy = (int)std::floor(q.y), cz = (int)std::floor(q.z);
    float f1 = 1e9f, f2 = 1e9f;
    for (int dz = -1; dz <= 1; dz++)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                V3 d = seedOf(cx + dx, cy + dy, cz + dz) - q;
                float l = dot(d, d);
                if (l < f1) { f2 = f1; f1 = l; }
                else if (l < f2) f2 = l;
            }
    return std::sqrt(f2) - std::sqrt(f1);
}

void web(Gen& G, unsigned count, float bright) {
    G.autoGal = true;
    const int BINS = 512;
    const float rMin = 300.0f, rMax = GALAXY_CAP_MLY;
    static const float cells[6] = {130.0f, 200.0f, 300.0f, 450.0f, 650.0f, 900.0f};
    float cdf[BINS + 1];
    cdf[0] = 0.0f;
    for (int i = 0; i < BINS; i++) {
        float r = rMin + (rMax - rMin) * ((float)i + 0.5f) / BINS;
        float q = r / 8000.0f;
        cdf[i + 1] = cdf[i] + r * r / (1.0f + q * q);
    }
    unsigned made = 0, tries = 0;
    while (made < count && tries < count * 60u) {
        tries++;
        float x = G.u() * cdf[BINS];
        int lo = 0, hi = BINS;
        while (hi - lo > 1) { int m = (lo + hi) / 2; if (cdf[m] <= x) lo = m; else hi = m; }
        float r = rMin + (rMax - rMin) * ((float)lo + G.u()) / BINS;
        V3 pos = G.sphere() * r;
        float lat = std::clamp(std::log(r / 1200.0f) / std::log(1.8f) + 1.5f, 0.0f, 4.999f);
        int band = std::min(5, (int)(lat + G.u()));
        float cell = cells[band];
        float ptR = cell * 0.06f;
        if (G.u() < 0.04f) {
            G.add(pos, ptR, G.galColour(), bright * 0.35f);
            made++;
            continue;
        }
        V3 q = pos * (1.0f / cell);
        int cx = (int)std::floor(q.x), cy = (int)std::floor(q.y), cz = (int)std::floor(q.z);
        int ox = (int)(G.u() * 2.999f) - 1, oy = (int)(G.u() * 2.999f) - 1, oz = (int)(G.u() * 2.999f) - 1;
        if (!ox && !oy && !oz) continue;
        V3 s1 = seedOf(cx, cy, cz), s2 = seedOf(cx + ox, cy + oy, cz + oz);
        V3 n = norm(s2 - s1), e1, e2;
        basis(n, e1, e2);
        V3 P = (s1 + s2) * 0.5f + e1 * (G.g() * 0.3f) + e2 * (G.g() * 0.3f) + n * (G.g() * 0.025f);
        float gap = faceGap(P);
        if (gap > 0.07f || len(P * cell) > GALAXY_CAP_MLY) continue;
        G.add(P * cell, ptR, G.galColour(), bright * (0.45f + 0.9f * G.u()) * (1.0f - gap * 6.0f));
        made++;
    }
    G.autoGal = false;
}

}

UniverseData makeUniverse() {
    UniverseData data;
    data.pts.reserve(560000);
    Gen G(data.pts, data.gals);
    float mwN[3];
    galaxyNormal(mwN);
    G.fadeD = 40.0f;
    spiral(G, {0.0f, 0.0f, 0.0f}, {mwN[0], mwN[1], mwN[2]}, 0.05f, 6000, 4, 0.2f, 0.9f, 0.0012f);
    data.mwCount = (unsigned)data.pts.size();

    V3 m31 = galPos(121.17f, -21.57f, 2.54f), m33 = galPos(133.6f, -31.3f, 2.73f);
    spiral(G, m31, tilted(m31, 77.0f, 38.0f), 0.085f, 7000, 2, 0.22f, 0.9f, 0.002f);
    G.named(m31, 1);
    spiral(G, m33, tilted(m33, 54.0f, 20.0f), 0.03f, 2500, 2, 0.05f, 0.8f, 0.0012f);
    G.named(m33, 2);
    V3 lmc = galPos(280.5f, -32.9f, 0.163f), smc = galPos(302.8f, -44.3f, 0.2f);
    blob(G, lmc, 0.004f, 700, 0.0007f, {0.8f, 0.88f, 1.0f}, 0.8f, 1.2f);
    G.add(lmc, 0.008f, {0.8f, 0.88f, 1.0f}, 2.2f);
    G.named(lmc, 3);
    blob(G, smc, 0.0025f, 350, 0.0006f, {0.8f, 0.88f, 1.0f}, 0.7f, 0.6f);
    G.add(smc, 0.005f, {0.8f, 0.88f, 1.0f}, 1.5f);
    G.named(smc, 4);
    for (int i = 0; i < 34; i++) {
        V3 home = i < 14 ? V3{0.0f, 0.0f, 0.0f} : i < 28 ? m31 : (m31 * 0.5f);
        float spread = i < 14 ? 0.3f : i < 28 ? 0.35f : 0.9f;
        V3 c = home + G.sphere() * (spread * std::cbrt(G.u()));
        float s = 0.0015f + 0.003f * G.u();
        blob(G, c, s, 10, 0.0008f, G.galColour(), 0.6f);
        G.add(c, 0.004f + 0.004f * G.u(), {1.0f, 0.88f, 0.7f}, 1.0f);
        G.named(c, 0);
    }

    V3 virgo = galPos(283.8f, 74.5f, 54.0f), coma = galPos(58.1f, 88.0f, 321.0f), ga = galPos(325.0f, -7.0f, 220.0f);
    G.fadeD = 400.0f;
    cluster(G, virgo, 1.1f, 1500, 0.07f, 0.9f);
    G.add(virgo, 0.5f, {1.0f, 0.85f, 0.6f}, 1.5f);
    G.fadeD = 1500.0f;
    cluster(G, coma, 2.2f, 2600, 0.12f, 0.9f);
    G.add(coma, 1.0f, {1.0f, 0.85f, 0.6f}, 1.8f);
    G.fadeD = 3000.0f;
    for (int i = 0; i < 9; i++) {
        V3 start = i == 0 ? virgo : i == 1 ? V3{0.0f, 0.0f, 0.0f} : ga + G.sphere() * (190.0f + 70.0f * G.u());
        filament(G, start, ga, 55.0f, 3200, 0.9f, 0.85f);
    }
    for (int i = 0; i < 8000; i++) {
        V3 p = ga + V3{G.g(), G.g(), G.g()} * 85.0f;
        G.add(p, 0.9f, G.galColour(), 0.45f + 0.4f * G.u());
    }
    cluster(G, ga, 7.0f, 2500, 0.9f, 1.0f);
    G.add(ga, 8.0f, {1.0f, 0.8f, 0.55f}, 3.0f);

    G.fadeD = 7000.0f;
    V3 sloan = galPos(245.0f, 52.0f, 1000.0f);
    sheet(G, sloan, cross(norm(sloan), {0.0f, 0.0f, 1.0f}), 1400.0f, 380.0f, 45.0f, 16000, 3.5f, 1.1f, {0.9f, 0.9f, 1.0f});
    G.fadeD = 18000.0f;
    V3 herc = galPos(60.0f, 45.0f, 10000.0f);
    data.hercFirst = (unsigned)data.pts.size();
    sheet(G, herc, cross(norm(herc), {0.3f, 0.9f, 0.1f}), 9500.0f, 2600.0f, 260.0f, 38000, 40.0f, 0.06f, {1.0f, 0.9f, 0.85f});
    data.hercCount = (unsigned)data.pts.size() - data.hercFirst;
    G.fadeD = 1e9f;
    V3 phoenix = galPos(345.0f, -65.0f, 5800.0f), gordo = galPos(297.0f, -67.0f, 7000.0f);
    cluster(G, phoenix, 7.0f, 2600, 2.5f, 1.0f);
    blob(G, phoenix, 0.22f, 1800, 0.03f, {1.0f, 0.82f, 0.55f}, 1.6f);
    G.add(phoenix, 60.0f, {1.0f, 0.85f, 0.6f}, 4.0f);
    G.add(phoenix, 0.5f, {1.0f, 0.85f, 0.6f}, 3.0f);
    cluster(G, gordo, 9.0f, 900, 5.0f, 1.0f);
    G.add(gordo, 70.0f, {1.0f, 0.8f, 0.55f}, 5.0f);

    web(G, 420000, 0.7f);
    return data;
}

std::vector<UniLabel> universeLabels() {
    std::vector<UniLabel> out;
    auto add = [&](const char* text, const char* sub, V3 p, float r, float g, float b, float lo, float hi, bool dyn = false) {
        out.push_back({text, sub, {p.x, p.y, p.z}, {r, g, b}, lo, hi, dyn});
    };
    V3 m31 = galPos(121.17f, -21.57f, 2.54f), m33 = galPos(133.6f, -31.3f, 2.73f);
    V3 lmc = galPos(280.5f, -32.9f, 0.163f), smc = galPos(302.8f, -44.3f, 0.2f);
    V3 virgo = galPos(283.8f, 74.5f, 54.0f), coma = galPos(58.1f, 88.0f, 321.0f), ga = galPos(325.0f, -7.0f, 220.0f);
    add("MILKY WAY - YOU ARE HERE", nullptr, {0.0f, 0.0f, 0.0f}, 1.0f, 0.9f, 0.3f, 0.9f, 400000.0f);
    add("DARK AGES - NO GALAXIES YET", nullptr, V3{40000.0f, 0.85f, 0.0f}, 0.45f, 0.4f, 0.62f, 60000.0f, 400000.0f, true);
    add("CMB - EDGE OF THE OBSERVABLE UNIVERSE (46 GLY)", nullptr, {0.0f, 0.0f, 0.0f}, 1.0f, 0.62f, 0.4f, 2000.0f, 400000.0f, true);
    add("ANDROMEDA", "M31 - 2.5 MLY", m31, 0.7f, 0.85f, 1.0f, 0.4f, 60.0f);
    add("TRIANGULUM", "M33 - 2.7 MLY", m33, 0.7f, 0.85f, 1.0f, 0.4f, 25.0f);
    add("MAGELLANIC CLOUDS", nullptr, (lmc + smc) * 0.5f, 0.7f, 0.95f, 0.8f, 0.3f, 10.0f);
    add("LOCAL GROUP", nullptr, m31 * 0.45f + V3{0.0f, 0.5f, 0.0f}, 0.55f, 0.75f, 1.0f, 1.5f, 70.0f);
    add("VIRGO CLUSTER", "54 MLY - 1,500 GALAXIES", virgo, 1.0f, 0.85f, 0.6f, 8.0f, 600.0f);
    add("COMA CLUSTER", "320 MLY", coma, 1.0f, 0.85f, 0.6f, 60.0f, 4000.0f);
    add("GREAT ATTRACTOR", "220 MLY", ga, 1.0f, 0.75f, 0.5f, 40.0f, 1800.0f);
    add("LANIAKEA", "SUPERCLUSTER - 520 MLY ACROSS", ga + V3{0.0f, 150.0f, 0.0f}, 0.65f, 0.9f, 1.0f, 150.0f, 9000.0f);
    add("SLOAN GREAT WALL", "1.4 GLY LONG", galPos(245.0f, 52.0f, 1000.0f), 0.8f, 0.85f, 1.0f, 300.0f, 25000.0f);
    add("TON 618", "QUASAR - 10.8 GLY", galPos(190.0f, 84.0f, 10800.0f), 0.75f, 0.85f, 1.0f, 1500.0f, 80000.0f);
    add("PHOENIX CLUSTER", "5.8 GLY", galPos(345.0f, -65.0f, 5800.0f), 1.0f, 0.85f, 0.6f, 1500.0f, 50000.0f);
    add("EL GORDO", "MOST MASSIVE CLUSTER", galPos(297.0f, -67.0f, 7000.0f), 1.0f, 0.8f, 0.55f, 1500.0f, 80000.0f);
    add("HERCULES-CORONA BOREALIS GREAT WALL", "BIGGEST KNOWN STRUCTURE (DISPUTED)", galPos(60.0f, 45.0f, 10000.0f), 1.0f, 0.85f, 0.8f, 3000.0f, 200000.0f);
    return out;
}
