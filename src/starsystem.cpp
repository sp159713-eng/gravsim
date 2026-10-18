#include "starsystem.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {

constexpr float PI = 3.14159265f;
constexpr float KM_UNITS = 1.26372f / 1.496e8f;

struct Rng {
    std::mt19937 g;
    float u() { return (float)(g() >> 8) * (1.0f / 16777216.0f); }
    float r(float a, float b) { return a + (b - a) * u(); }
};

const float MASS_RANGE[6][2] = {{3.0f, 25.0f}, {1.6f, 2.4f}, {1.1f, 1.5f}, {0.85f, 1.1f}, {0.55f, 0.85f}, {0.12f, 0.5f}};
const float ROCK_TINT[5][3] = {{1.0f, 0.72f, 0.55f}, {0.88f, 0.88f, 0.95f}, {1.0f, 0.9f, 0.7f}, {0.8f, 1.0f, 0.85f}, {0.85f, 0.7f, 0.65f}};
const float GAS_TINT[6][3] = {{1.0f, 0.8f, 0.55f}, {0.8f, 0.9f, 1.0f}, {0.95f, 0.72f, 0.9f}, {0.75f, 1.0f, 0.85f}, {1.0f, 0.62f, 0.45f}, {1.0f, 0.95f, 0.7f}};
const float ICE_TINT[4][3] = {{0.45f, 0.72f, 1.0f}, {0.5f, 0.92f, 0.85f}, {0.55f, 0.55f, 1.0f}, {0.7f, 0.85f, 1.0f}};

float trueRadius(const PlanetSpec& p) { return p.radiusKm * KM_UNITS; }

float enlRadius(const PlanetSpec& p) {
    if (p.type == 10) return std::clamp(p.radiusKm / 50000.0f, 0.95f, 1.6f);
    if (p.type == 11) return 0.85f;
    return std::clamp(0.6f * std::sqrt(p.radiusKm / 6371.0f), 0.3f, 0.75f);
}

float starTrueRadius(const StarSystem& s) { return s.radiusSun * 5.877e-3f; }
float starEnlRadius(const StarSystem& s) { return std::clamp(1.4f + 0.6f * std::sqrt(s.radiusSun), 1.2f, 3.4f); }

void rotate(float v[3], float incl, float node) {
    float c = std::cos(incl), s = std::sin(incl);
    float y = v[1] * c - v[2] * s, z = v[1] * s + v[2] * c;
    float cn = std::cos(node), sn = std::sin(node);
    float x = v[0] * cn + z * sn;
    z = -v[0] * sn + z * cn;
    v[0] = x; v[1] = y; v[2] = z;
}

void addOrbiter(std::vector<Particle>& out, float r, float theta, float y, float mass, float gm) {
    Particle p{};
    p.pos[0] = r * std::cos(theta);
    p.pos[1] = y;
    p.pos[2] = r * std::sin(theta);
    p.pos[3] = mass;
    float v = std::sqrt(gm / r);
    p.vel[0] = -v * std::sin(theta);
    p.vel[2] = v * std::cos(theta);
    out.push_back(p);
}

float moonRadius(const PlanetSpec& p, float rel, bool trueScale) {
    if (trueScale) return trueRadius(p) * rel;
    return enlRadius(p) * (1.4f + 1.2f * std::log10(rel / 3.0f));
}

}

float orbitUnits(float au, bool trueScale, float au0) {
    if (trueScale) return au * 1.26372f;
    return 4.0f + 6.2f * std::log(std::max(au, au0) / au0);
}

float arrivalDistance(const StarSystem& s, bool trueScale) {
    float ext = orbitUnits(s.refAu * 1.4f, trueScale, s.au0);
    return std::clamp(2.3f * ext, s.au0 < 0.05f ? 0.3f : 25.0f, 1300.0f);
}

StarSystem generateSystem(uint32_t index, int cls, const float color[3]) {
    Rng R{std::mt19937(index * 2654435761u ^ 0x9E3779B9u)};
    StarSystem s{};
    s.index = index;
    s.cls = cls;
    for (int a = 0; a < 3; a++) s.color[a] = color[a];
    float lo = MASS_RANGE[cls][0], hi = MASS_RANGE[cls][1];
    s.mass = lo * std::pow(hi / lo, R.u());
    s.lum = s.mass > 0.43f ? std::pow(s.mass, 3.5f) : 0.23f * std::pow(s.mass, 2.3f);
    s.radiusSun = std::pow(s.mass, s.mass > 1.0f ? 0.75f : 0.9f);
    s.lifeGyr = 10.0f * std::pow(s.mass, -2.5f);
    s.ageGyr = std::min(R.r(0.05f, 0.9f) * s.lifeGyr, R.r(1.0f, 11.0f));
    float sl = std::sqrt(s.lum);
    s.hzIn = 0.95f * sl;
    s.hzOut = 1.37f * sl;
    float hzMid = 0.5f * (s.hzIn + s.hzOut), snow = 2.7f * sl, scale = std::min(sl, 60.0f);
    float aMin = std::cbrt(0.01f * s.mass);
    int n = std::min(8, (int)(std::pow(R.u(), 0.8f) * 9.0f));
    if (cls == 0) n = std::min(n, 3);
    s.planetCount = n;
    float a[8];
    a[0] = std::max(aMin * R.r(1.0f, 1.3f), scale * R.r(0.25f, 0.55f));
    for (int i = 1; i < 8; i++) a[i] = a[i - 1] * R.r(1.5f, 1.95f);
    bool wantHz = R.u() < 0.3f;
    if (n > 0 && wantHz) {
        int j = -1;
        float best = 1e9f;
        for (int i = 0; i < n; i++) {
            float d = std::fabs(std::log(a[i] / hzMid));
            if (a[i] < snow && d < best) { best = d; j = i; }
        }
        if (j >= 0) {
            float sc = hzMid * R.r(0.92f, 1.08f) / a[j];
            if (a[0] * sc >= aMin) for (int i = 0; i < 8; i++) a[i] *= sc;
        }
    }
    bool hot = R.u() < 0.12f && cls >= 2;
    for (int i = 0; i < n; i++) {
        PlanetSpec& p = s.planets[i];
        p.au = a[i];
        p.ecc = R.r(0.0f, 0.07f);
        p.incl = R.r(0.0f, 0.04f);
        p.node = R.r(0.0f, 2.0f * PI);
        p.phase = R.r(0.0f, 2.0f * PI);
        bool giantZone = a[i] >= snow;
        bool gas = (i == 0 && hot) || (giantZone && R.u() < (a[i] > 4.0f * snow ? 0.35f : 0.75f));
        if (gas) {
            p.type = 10;
            p.mass = (i == 0 && hot) ? R.r(0.3f, 1.2f) : 0.08f * std::pow(25.0f, R.u());
            p.radiusKm = std::clamp(70000.0f * std::pow(p.mass / 0.95f, 0.1f), 35000.0f, 90000.0f);
            const float* t = GAS_TINT[(int)(R.u() * 5.99f)];
            for (int k = 0; k < 3; k++) p.tint[k] = t[k];
            p.tilt = R.r(0.0f, 30.0f);
            p.day = R.r(0.3f, 1.0f);
        } else if (giantZone) {
            p.type = 11;
            p.mass = R.r(0.03f, 0.1f);
            p.radiusKm = 25000.0f * std::pow(p.mass / 0.05f, 0.3f);
            const float* t = ICE_TINT[(int)(R.u() * 3.99f)];
            for (int k = 0; k < 3; k++) p.tint[k] = t[k];
            p.tilt = R.r(0.0f, 98.0f);
            p.day = R.r(0.5f, 1.0f);
        } else {
            p.mass = 0.0004f * std::pow(30.0f, R.u());
            p.radiusKm = std::clamp(6371.0f * std::pow(p.mass / 0.003f, 0.28f), 2000.0f, 12000.0f);
            bool inHz = a[i] >= 0.98f * s.hzIn && a[i] <= 1.02f * s.hzOut;
            p.habitable = inHz && p.mass >= 0.0006f && p.mass <= 0.009f;
            const float* t = ROCK_TINT[(int)(R.u() * 4.99f)];
            for (int k = 0; k < 3; k++) p.tint[k] = t[k];
            if (p.habitable) {
                p.type = 3;
                p.tint[0] = p.tint[1] = p.tint[2] = 1.0f;
            } else if (a[i] < 0.8f * s.hzIn) {
                p.type = R.u() < 0.5f ? 2 : 9;
                p.tint[1] *= 0.85f;
                p.tint[2] *= 0.7f;
            } else {
                p.type = R.u() < 0.5f ? 4 : 9;
            }
            p.tilt = R.r(0.0f, 45.0f);
            p.day = R.r(0.5f, 60.0f);
        }
    }
    int moons = 0;
    for (int i = 0; i < n && moons < 5; i++) {
        const PlanetSpec& p = s.planets[i];
        int want = p.type == 10 ? 1 + (int)(R.u() * 2.99f) : p.type == 11 ? (int)(R.u() * 2.5f) : (R.u() < 0.35f ? 1 : 0);
        for (int k = 0; k < want && moons < 5; k++) {
            MoonSpec& m = s.moons[moons++];
            m.parent = i + 1;
            m.rel = p.type >= 10 ? R.r(5.0f, 30.0f) : R.r(8.0f, 60.0f);
            m.period = std::min(12.0f, 0.8f + 3.0f * std::pow(m.rel / 15.0f, 1.5f));
            m.phase = R.r(0.0f, 2.0f * PI);
        }
    }
    s.moonCount = moons;
    s.ring = n >= 6 && s.planets[5].type == 10 && R.u() < 0.7f;
    for (int i = 0; i + 1 < n && s.beltLo == 0.0f; i++)
        if (a[i + 1] / a[i] > 1.9f) {
            s.beltLo = a[i] * 1.25f;
            s.beltHi = a[i + 1] / 1.25f;
        }
    if (n == 0) {
        s.beltLo = 2.2f * std::max(scale, 0.3f);
        s.beltHi = 3.2f * std::max(scale, 0.3f);
    }
    float ref = std::max(n > 0 ? a[n - 1] : 0.0f, s.beltHi);
    s.refAu = ref;
    s.outerLo = 1.35f * ref;
    s.outerHi = 2.1f * ref;
    return s;
}

std::vector<Particle> buildSystem(const StarSystem& s, uint32_t n, bool trueScale) {
    const float starMass = 1000.0f * s.mass;
    const float gm = solarG(trueScale) * starMass;
    std::mt19937 rng(s.index * 7919u + 13u);
    std::uniform_real_distribution<float> angle(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> tilt(-0.3f, 0.3f);
    std::vector<Particle> out;
    out.reserve(n);
    Particle star{};
    star.pos[3] = starMass;
    out.push_back(star);
    for (int i = 0; i < 8; i++) {
        Particle p{};
        if (i < s.planetCount) {
            const PlanetSpec& pl = s.planets[i];
            float r = orbitUnits(pl.au, trueScale, s.au0) * (1.0f - pl.ecc);
            float v = std::sqrt(gm * (1.0f + pl.ecc) / r);
            float pos[3] = {r * std::cos(pl.phase), 0.0f, r * std::sin(pl.phase)};
            float vel[3] = {-v * std::sin(pl.phase), 0.0f, v * std::cos(pl.phase)};
            rotate(pos, pl.incl, pl.node);
            rotate(vel, pl.incl, pl.node);
            for (int a = 0; a < 3; a++) { p.pos[a] = pos[a]; p.vel[a] = vel[a]; }
            p.pos[3] = pl.mass;
        }
        out.push_back(p);
    }
    for (int k = 0; k < 5; k++) {
        Particle m{};
        if (k < s.moonCount) {
            const MoonSpec& ms = s.moons[k];
            const Particle& parent = out[ms.parent];
            float mr = moonRadius(s.planets[ms.parent - 1], ms.rel, trueScale);
            m.pos[0] = parent.pos[0] + mr * std::cos(ms.phase);
            m.pos[1] = parent.pos[1];
            m.pos[2] = parent.pos[2] + mr * std::sin(ms.phase);
            m.pos[3] = 0.000001f;
            m.vel[0] = mr;
            m.vel[1] = ms.phase;
            m.vel[2] = 2.0f * PI / ms.period;
            m.vel[3] = (float)(ms.parent + 1);
        } else {
            m.vel[3] = 1.0f;
        }
        out.push_back(m);
    }
    {
        const float peri = trueScale ? 0.75f : 3.0f, cometTilt = 0.26f;
        float apo = std::max(orbitUnits(s.refAu, trueScale, s.au0) * 1.25f, 6.0f * peri);
        float semi = 0.5f * (peri + apo);
        float v = std::sqrt(gm * (2.0f / apo - 1.0f / semi));
        Particle comet{};
        comet.pos[0] = -apo * std::cos(cometTilt);
        comet.pos[1] = -apo * std::sin(cometTilt);
        comet.pos[3] = 0.000005f;
        comet.vel[2] = -v;
        out.push_back(comet);
    }
    if (s.ring) {
        const PlanetSpec& sat = s.planets[5];
        float rp = trueScale ? trueRadius(sat) : enlRadius(sat);
        float lo = 1.3f * rp, hi = 2.3f * rp, ref = 0.5f * (lo + hi);
        std::uniform_real_distribution<float> ringRadius(lo, hi);
        for (int i = 0; i < 600 && out.size() < n; i++) {
            float r = ringRadius(rng), a = angle(rng);
            Particle ring{};
            ring.pos[0] = out[6].pos[0] + r * std::cos(a);
            ring.pos[1] = out[6].pos[1];
            ring.pos[2] = out[6].pos[2] + r * std::sin(a);
            ring.pos[3] = 0.0000003f;
            ring.vel[0] = r;
            ring.vel[1] = a;
            ring.vel[2] = 2.0f * PI / (1.2f * std::pow(r / ref, 1.5f));
            ring.vel[3] = 7.0f;
            out.push_back(ring);
        }
    }
    uint32_t rocks = n > out.size() ? n - (uint32_t)out.size() : 0;
    uint32_t inner = s.beltLo > 0.0f ? rocks * 2 / 5 : 0;
    std::uniform_real_distribution<float> belt(orbitUnits(std::max(s.beltLo, 0.05f), trueScale, s.au0), orbitUnits(std::max(s.beltHi, 0.06f), trueScale, s.au0));
    std::uniform_real_distribution<float> disc(orbitUnits(s.outerLo, trueScale, s.au0), orbitUnits(s.outerHi, trueScale, s.au0));
    const float tiltScale = trueScale ? 0.2f : 1.0f;
    for (uint32_t i = 0; i < rocks; i++)
        addOrbiter(out, i < inner ? belt(rng) : disc(rng), angle(rng), tilt(rng) * tiltScale, 0.000001f, gm);
    double momentum[3] = {0.0, 0.0, 0.0}, moment[3] = {0.0, 0.0, 0.0}, total = 0.0;
    for (const Particle& p : out) {
        if (p.vel[3] > 0.5f) continue;
        total += p.pos[3];
        for (int a = 0; a < 3; a++) {
            momentum[a] += (double)p.pos[3] * p.vel[a];
            moment[a] += (double)p.pos[3] * p.pos[a];
        }
    }
    for (int a = 0; a < 3; a++) out[0].vel[a] -= (float)(momentum[a] / starMass);
    for (Particle& p : out)
        for (int a = 0; a < 3; a++) p.pos[a] -= (float)(moment[a] / total);
    return out;
}

void systemStyles(const StarSystem& s, float out[9][8]) {
    out[0][0] = starTrueRadius(s);
    out[0][1] = starEnlRadius(s);
    out[0][2] = 12.0f;
    out[0][3] = 25.0f;
    for (int a = 0; a < 3; a++) out[0][4 + a] = s.color[a];
    out[0][7] = 7.0f;
    for (int i = 0; i < 8; i++) {
        float* o = out[i + 1];
        if (i >= s.planetCount) {
            o[0] = o[1] = -1.0f;
            o[2] = 9.0f;
            o[3] = 1.0f;
            o[4] = o[5] = o[6] = 1.0f;
            o[7] = 0.0f;
            continue;
        }
        const PlanetSpec& p = s.planets[i];
        o[0] = trueRadius(p);
        o[1] = enlRadius(p);
        o[2] = (float)p.type;
        o[3] = p.day;
        for (int a = 0; a < 3; a++) o[4 + a] = p.tint[a];
        o[7] = p.tilt;
    }
}

void homeStyles(float out[9][8]) {
    static const float tilt[9] = {7.25f, 0.03f, 177.4f, 23.4f, 25.2f, 3.1f, 26.7f, 97.8f, 28.3f};
    static const float day[9] = {25.4f, 58.6f, 243.0f, 1.0f, 1.03f, 0.41f, 0.44f, 0.72f, 0.67f};
    clearBodyRadii();
    for (int i = 0; i < 9; i++) {
        out[i][0] = bodyRadius(true, i);
        out[i][1] = bodyRadius(false, i);
        out[i][2] = (float)i;
        out[i][3] = day[i];
        out[i][4] = out[i][5] = out[i][6] = 1.0f;
        out[i][7] = tilt[i];
    }
}

void applyStyleRadii(const float styles[9][8]) {
    float t[9], e[9];
    for (int i = 0; i < 9; i++) {
        t[i] = std::max(styles[i][0], 0.0f);
        e[i] = std::max(styles[i][1], 0.0f);
    }
    setBodyRadii(t, e);
}
