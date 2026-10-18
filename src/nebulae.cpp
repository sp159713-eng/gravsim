#include "nebulae.h"
#include "milkyway.h"
#include <cmath>
#include <cstdio>
#include <random>

namespace {

constexpr float PI = 3.14159265f;

enum Kind { EMISSION, REMNANT, PLANETARY, PILLARS };

struct Def {
    const char* name;
    const char* type;
    const char* fact1;
    const char* fact2;
    float l, b, distLy, radiusLy;
    Kind kind;
    int sprites;
    float c1[3], c2[3];
};

const Def DEFS[] = {
    {"ORION NEBULA", "EMISSION NEBULA", "A NURSERY WHERE NEW STARS", "ARE BORN, VISIBLE BY EYE.", 209.01f, -19.38f, 1344.0f, 12.0f, EMISSION, 70, {1.0f, 0.35f, 0.5f}, {0.35f, 0.55f, 1.0f}},
    {"EAGLE NEBULA", "EMISSION NEBULA + PILLARS", "HOME OF THE PILLARS OF", "CREATION, STAR-BIRTH COLUMNS.", 16.95f, 0.83f, 7000.0f, 35.0f, PILLARS, 70, {0.9f, 0.45f, 0.3f}, {0.4f, 1.0f, 0.6f}},
    {"CRAB NEBULA", "SUPERNOVA REMNANT", "DEBRIS OF A STAR SEEN TO", "EXPLODE IN THE YEAR 1054.", 184.56f, -5.78f, 6500.0f, 5.5f, REMNANT, 60, {1.0f, 0.55f, 0.2f}, {0.5f, 0.7f, 1.0f}},
    {"RING NEBULA", "PLANETARY NEBULA", "THE SHED SHELL OF A DYING", "SUN-LIKE STAR.", 63.17f, 13.98f, 2300.0f, 1.3f, PLANETARY, 50, {0.4f, 1.0f, 0.8f}, {1.0f, 0.4f, 0.35f}},
    {"CARINA NEBULA", "EMISSION NEBULA", "A HUGE STAR-FORMING CLOUD,", "FOUR TIMES LARGER THAN ORION.", 287.6f, -0.65f, 8500.0f, 150.0f, EMISSION, 90, {1.0f, 0.4f, 0.4f}, {0.45f, 0.65f, 1.0f}},
    {"HORSEHEAD NEBULA", "DARK NEBULA", "A DARK DUST CLOUD SILHOUETTED", "AGAINST GLOWING GAS.", 206.0f, -16.5f, 1375.0f, 4.0f, EMISSION, 40, {1.0f, 0.25f, 0.2f}, {0.9f, 0.4f, 0.3f}},
    {"HELIX NEBULA", "PLANETARY NEBULA", "A DYING STAR'S SHELL, ONE OF", "THE NEAREST PLANETARY NEBULAE.", 36.2f, -57.1f, 650.0f, 1.5f, PLANETARY, 50, {0.3f, 0.9f, 1.0f}, {1.0f, 0.45f, 0.3f}},
};

const int COUNT = (int)(sizeof(DEFS) / sizeof(DEFS[0]));

void centre(const Def& d, float out[3]) {
    float l = d.l * PI / 180.0f, b = d.b * PI / 180.0f;
    float g[3] = {d.distLy * std::cos(b) * std::cos(l), d.distLy * std::cos(b) * std::sin(l), d.distLy * std::sin(b)};
    galaxySunToSim(g, out);
}

std::string commas(long long v) {
    std::string s = std::to_string(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

}

const std::vector<NebulaInfo>& nebulaInfos() {
    static std::vector<NebulaInfo> infos;
    if (infos.empty()) {
        infos.reserve(COUNT);
        for (const Def& d : DEFS) {
            NebulaInfo n{};
            n.name = d.name;
            n.type = d.type;
            n.fact1 = d.fact1;
            n.fact2 = d.fact2;
            n.distLy = d.distLy;
            n.diameterLy = d.radiusLy * 2.0f;
            centre(d, n.pos);
            for (int a = 0; a < 3; a++) n.color[a] = d.c1[a];
            n.label = std::string(d.name) + "  " + d.type + "  " + commas((long long)d.distLy) + " LY";
            infos.push_back(n);
        }
    }
    return infos;
}

std::vector<NebulaVertex> makeNebulae() {
    std::vector<NebulaVertex> out;
    const std::vector<NebulaInfo>& infos = nebulaInfos();
    std::mt19937 rng(4242);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    std::normal_distribution<float> gauss(0.0f, 1.0f);
    for (int i = 0; i < COUNT; i++) {
        const Def& d = DEFS[i];
        float R = d.radiusLy;
        float axis[3], u[3], v[3];
        float tz = uni(rng) * 2.0f - 1.0f, ta = uni(rng) * 2.0f * PI, tr = std::sqrt(1.0f - tz * tz);
        axis[0] = tr * std::cos(ta); axis[1] = tr * std::sin(ta); axis[2] = tz;
        u[0] = -axis[1]; u[1] = axis[0]; u[2] = 0.0f;
        float ul = std::sqrt(u[0] * u[0] + u[1] * u[1]);
        if (ul < 1e-4f) { u[0] = 1.0f; u[1] = 0.0f; ul = 1.0f; }
        for (int a = 0; a < 3; a++) u[a] /= ul;
        v[0] = axis[1] * u[2] - axis[2] * u[1];
        v[1] = axis[2] * u[0] - axis[0] * u[2];
        v[2] = axis[0] * u[1] - axis[1] * u[0];
        for (int k = 0; k < d.sprites; k++) {
            float off[3] = {0.0f, 0.0f, 0.0f};
            float size = R * (0.33f + 0.5f * uni(rng));
            float mix = uni(rng);
            float gain = 1.0f;
            if (d.kind == EMISSION) {
                float rr = R * 0.55f;
                for (int a = 0; a < 3; a++) off[a] = rr * gauss(rng);
                float core = std::sqrt(off[0] * off[0] + off[1] * off[1] + off[2] * off[2]) / R;
                size *= 1.0f - 0.4f * std::min(core, 1.0f);
                gain = 1.2f - 0.5f * std::min(core, 1.0f);
            } else if (d.kind == REMNANT) {
                float th = uni(rng) * 2.0f * PI, ph = std::acos(uni(rng) * 2.0f - 1.0f);
                float rr = R * (0.55f + 0.4f * uni(rng));
                float p[3] = {rr * std::sin(ph) * std::cos(th) * 1.25f, rr * std::sin(ph) * std::sin(th), rr * std::cos(ph) * 0.8f};
                for (int a = 0; a < 3; a++) off[a] = p[a];
                size = R * (0.15f + 0.3f * uni(rng));
                if (k < 8) { for (int a = 0; a < 3; a++) off[a] *= 0.15f; size = R * 0.4f; mix = 1.0f; }
            } else if (d.kind == PLANETARY) {
                float th = uni(rng) * 2.0f * PI;
                float rr = R * (0.8f + 0.25f * uni(rng));
                float ring = k % 5 == 0 ? 0.6f : 1.0f;
                for (int a = 0; a < 3; a++) off[a] = ring * rr * (std::cos(th) * u[a] + std::sin(th) * v[a]) + 0.12f * R * gauss(rng) * axis[a];
                size = R * (0.22f + 0.25f * uni(rng));
                mix = ring < 1.0f ? 1.0f : uni(rng) * 0.4f;
            } else {
                float h = uni(rng) * 2.0f - 1.0f, w = gauss(rng) * 0.12f * R;
                int pillar = k % 3;
                float px = (pillar - 1) * 0.35f * R, py = h * R * 0.7f;
                for (int a = 0; a < 3; a++) off[a] = px * u[a] + py * v[a] + w * axis[a];
                size = R * (0.12f + 0.2f * uni(rng));
                gain = 1.1f;
                if (k % 4 == 0) { for (int a = 0; a < 3; a++) off[a] = 0.9f * R * gauss(rng) * (a == 0 ? u[a] : a == 1 ? v[a] : axis[a]); size = R * 0.5f; gain = 0.5f; }
            }
            NebulaVertex nv{};
            for (int a = 0; a < 3; a++) {
                nv.pos[a] = infos[i].pos[a] + off[a];
                nv.color[a] = (d.c1[a] * (1.0f - mix) + d.c2[a] * mix) * gain;
            }
            nv.size = size;
            nv.seed = uni(rng);
            out.push_back(nv);
        }
    }
    return out;
}
