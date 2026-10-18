#include "belts.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {

const double PI_D = 3.14159265358979323846;
const double DEG = PI_D / 180.0;
const double GAUSS_K = 0.01720209895;
const double YEAR_D = 365.25;

const BeltNamed NAMED[BELT_NAMED] = {
    {"CERES", "DWARF PLANET", 2.7675, 0.0758, 10.593, 80.39, 73.60, 7.3, 940.0, {0.78f, 0.82f, 0.9f}, 4.0, false, true,
     "DISCOVERED 1801", "CERES HOLDS A THIRD OF ALL THE MASS IN THE BELT. NASA'S DAWN ORBITED IT IN 2015-2018 AND FOUND BRIGHT SALT SPOTS IN OCCATOR CRATER."},
    {"VESTA", "ASTEROID", 2.3615, 0.0887, 7.14, 103.85, 151.2, 25.0, 525.0, {1.0f, 0.82f, 0.6f}, 4.0, false, true,
     "DISCOVERED 1807", "VESTA IS THE BRIGHTEST ASTEROID - SOMETIMES VISIBLE TO THE NAKED EYE. DAWN VISITED IT IN 2011-2012."},
    {"PALLAS", "ASTEROID", 2.7725, 0.2313, 34.84, 173.09, 310.0, 250.0, 512.0, {0.7f, 0.85f, 1.0f}, 4.0, false, true,
     "DISCOVERED 1802", "PALLAS HAS A STEEPLY TILTED ORBIT - 35 DEGREES ABOVE THE PLANETS' PLANE - SO IT NEVER MEETS THE OTHERS."},
    {"HYGIEA", "ASTEROID", 3.142, 0.1146, 3.84, 283.2, 312.3, 60.0, 434.0, {0.8f, 0.7f, 0.85f}, 4.0, false, true,
     "DISCOVERED 1849", "HYGIEA MAY BE ROUND ENOUGH TO COUNT AS A DWARF PLANET, MAKING IT THE FOURTH LARGEST OBJECT IN THE BELT."},
    {"PSYCHE", "METAL ASTEROID", 2.9239, 0.1337, 3.10, 150.3, 229.9, 100.0, 226.0, {1.0f, 0.9f, 0.45f}, 4.0, false, true,
     "DISCOVERED 1852", "PSYCHE MAY BE THE EXPOSED IRON-NICKEL CORE OF A SHATTERED PROTOPLANET. NASA'S PSYCHE PROBE LAUNCHED IN OCT 2023 AND ARRIVES IN 2029."},
    {"PLUTO", "DWARF PLANET", 39.482, 0.2488, 17.14, 110.30, 113.77, 14.86, 2377.0, {0.95f, 0.8f, 0.65f}, 400.0, true, true,
     "DISCOVERED 1930", "PLUTO CIRCLES THE SUN TWICE FOR EVERY THREE NEPTUNE ORBITS, SO THEY NEVER COLLIDE. NEW HORIZONS FLEW BY IN 2015."},
    {"ERIS", "DWARF PLANET", 67.67, 0.4407, 44.04, 35.95, 151.64, 196.0, 2326.0, {0.85f, 0.9f, 1.0f}, 400.0, true, true,
     "DISCOVERED 2005", "ERIS IS ABOUT PLUTO'S SIZE BUT MORE MASSIVE. ITS DISCOVERY LED TO PLUTO BEING RECLASSIFIED AS A DWARF PLANET."},
    {"HAUMEA", "DWARF PLANET", 43.13, 0.1949, 28.21, 122.17, 239.0, 195.0, 1632.0, {0.9f, 0.95f, 1.0f}, 400.0, true, true,
     "DISCOVERED 2004", "HAUMEA SPINS ONCE EVERY 4 HOURS, STRETCHING IT INTO A FOOTBALL SHAPE. IT HAS A RING AND TWO MOONS."},
    {"MAKEMAKE", "DWARF PLANET", 45.43, 0.161, 29.0, 79.3, 297.0, 175.0, 1430.0, {1.0f, 0.7f, 0.5f}, 400.0, true, true,
     "DISCOVERED 2005", "MAKEMAKE IS ONE OF THE BRIGHTEST KUIPER BELT OBJECTS, COATED IN FROZEN METHANE."},
    {"ARROKOTH", "KUIPER BELT OBJECT", 44.2, 0.037, 2.45, 159.0, 174.4, 330.0, 36.0, {1.0f, 0.55f, 0.4f}, 150.0, true, true,
     "DISCOVERED 2014", "ARROKOTH IS TWO LOBES STUCK TOGETHER. NEW HORIZONS FLEW PAST ON 1 JAN 2019 - THE MOST DISTANT FLYBY EVER MADE."},
    {"SEDNA", "DISTANT PLANETOID", 506.0, 0.8496, 11.93, 144.5, 311.5, 357.5, 995.0, {1.0f, 0.4f, 0.35f}, 1500.0, true, false,
     "DISCOVERED 2003", "SEDNA TAKES 11,400 YEARS TO ORBIT THE SUN."}
};

struct Rng {
    std::mt19937 g{20260131u};
    double u() { return std::uniform_real_distribution<double>(0.0, 1.0)(g); }
    double range(double a, double b) { return a + (b - a) * u(); }
    double normal(double m, double s) { return std::normal_distribution<double>(m, s)(g); }
    double rayleigh(double s, double clip) {
        for (int i = 0; i < 20; i++) {
            double v = s * std::sqrt(-2.0 * std::log(1.0 - 0.999999 * u()));
            if (v <= clip) return v;
        }
        return clip * u();
    }
};

double gapFactor(double a, double a0, double w, double depth) {
    double x = (a - a0) / w;
    return 1.0 - depth * std::exp(-x * x);
}

double beltDensity(double a) {
    double d = 1.0 + 0.35 * std::exp(-std::pow((a - 2.3) / 0.12, 2)) + 0.5 * std::exp(-std::pow((a - 2.72) / 0.2, 2)) + 0.6 * std::exp(-std::pow((a - 3.13) / 0.08, 2));
    d *= gapFactor(a, 2.50, 0.08, 0.995);
    d *= gapFactor(a, 2.82, 0.06, 0.99);
    d *= gapFactor(a, 2.95, 0.04, 0.97);
    d *= gapFactor(a, 3.27, 0.07, 0.995);
    double lo = std::clamp((a - 2.06) / 0.1, 0.0, 1.0);
    return d * lo;
}

double sampleBeltA(Rng& r) {
    for (;;) {
        double a = r.range(2.1, 3.3);
        if (r.u() * 2.3 < beltDensity(a)) return a;
    }
}

double nRad(double a) {
    return GAUSS_K / std::pow(a, 1.5);
}

BeltVertex make(double a, double e, double inc, double node, double arg, double m0, double n, int mode, double libA, double libPh, double libW, int cls, const float col[3], double radiusAu) {
    BeltVertex v;
    v.a[0] = (float)a;
    v.a[1] = (float)e;
    v.a[2] = (float)inc;
    v.a[3] = (float)node;
    v.b[0] = (float)arg;
    v.b[1] = (float)m0;
    v.b[2] = (float)libA;
    v.b[3] = (float)libPh;
    v.c[0] = (float)n;
    v.c[1] = (float)libW;
    v.c[2] = (float)mode;
    v.c[3] = (float)cls;
    v.d[0] = col[0];
    v.d[1] = col[1];
    v.d[2] = col[2];
    v.d[3] = (float)radiusAu;
    return v;
}

double wrap2(double x) {
    x = std::fmod(x, 2.0 * PI_D);
    return x < 0.0 ? x + 2.0 * PI_D : x;
}

void shade(float out[3], const float base[3], double k) {
    for (int i = 0; i < 3; i++) out[i] = (float)std::min(1.0, base[i] * k);
}

void addMainBelt(std::vector<BeltVertex>& v, Rng& r, int count) {
    const float inner[3] = {0.9f, 0.68f, 0.45f}, mid[3] = {0.78f, 0.7f, 0.58f}, outer[3] = {0.6f, 0.64f, 0.7f};
    for (int i = 0; i < count; i++) {
        double a = sampleBeltA(r);
        double e = r.rayleigh(r.u() < 0.94 ? 0.015 : 0.09, 0.3);
        double inc = r.rayleigh(6.0, 20.0) * DEG;
        const float* b = a < 2.5 ? inner : (a < 2.95 ? mid : outer);
        float c[3];
        shade(c, b, r.range(0.55, 1.0));
        v.push_back(make(a, e, inc, r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), nRad(a), 0, 0, 0, 0, 0, c, 0.0));
    }
}

void addResonant(std::vector<BeltVertex>& v, Rng& r, int count, int mode, double aMean, double aSig, double eMean, double eSig, double incSig, double incClip, double libMax, double libW, const float base[3], int cls) {
    for (int i = 0; i < count; i++) {
        double a = r.normal(aMean, aSig);
        double e = std::clamp(r.normal(eMean, eSig), 0.01, 0.35);
        double inc = r.rayleigh(incSig, incClip) * DEG;
        double node = r.range(0, 2 * PI_D);
        double varpi = r.range(0, 2 * PI_D);
        double arg = wrap2(varpi - node);
        double lib = r.range(0.1, libMax);
        double m0, amp;
        if (mode == 2) {
            m0 = -1.5 * varpi + PI_D * std::floor(r.u() * 2.0);
            amp = lib / 2.0;
        } else {
            m0 = (PI_D + 2.0 * PI_D * std::floor(r.u() * 3.0)) / 3.0 - varpi * 2.0 / 3.0;
            amp = lib / 3.0;
        }
        float c[3];
        shade(c, base, r.range(0.6, 1.0));
        v.push_back(make(a, e, inc, node, arg, m0, 0.0, mode, amp, r.range(0, 2 * PI_D), libW, cls, c, 0.0));
    }
}

void addTrojans(std::vector<BeltVertex>& v, Rng& r, int count, double off) {
    const float base[3] = {0.88f, 0.52f, 0.42f};
    for (int i = 0; i < count; i++) {
        double a = r.normal(5.2029, 0.07);
        double e = r.rayleigh(0.04, 0.14);
        double inc = r.rayleigh(9.0, 30.0) * DEG;
        double node = r.range(0, 2 * PI_D), varpi = r.range(0, 2 * PI_D);
        double lib = std::min(0.75, r.rayleigh(0.28, 0.75));
        float c[3];
        shade(c, base, r.range(0.6, 1.0));
        v.push_back(make(a, e, inc, node, wrap2(varpi - node), off - varpi, 0.0, 1, lib, r.range(0, 2 * PI_D), 2.0 * PI_D / (150.0 * YEAR_D), 0, c, 0.0));
    }
}

void addKuiper(std::vector<BeltVertex>& v, Rng& r) {
    const float cold[3] = {0.92f, 0.48f, 0.34f}, hot[3] = {0.78f, 0.72f, 0.66f}, twot[3] = {0.8f, 0.6f, 0.9f}, scat[3] = {0.55f, 0.95f, 0.7f}, plut[3] = {0.5f, 0.78f, 1.0f};
    for (int i = 0; i < 4300; i++) {
        double a = r.range(42.0, 48.0);
        float c[3];
        shade(c, cold, r.range(0.6, 1.0));
        v.push_back(make(a, r.rayleigh(0.035, 0.12), r.rayleigh(1.8, 5.0) * DEG, r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), nRad(a), 0, 0, 0, 0, 1, c, 0.0));
    }
    for (int i = 0; i < 2000; i++) {
        double a = r.range(40.5, 47.5);
        float c[3];
        shade(c, hot, r.range(0.6, 1.0));
        v.push_back(make(a, r.rayleigh(0.09, 0.25), r.rayleigh(10.0, 30.0) * DEG, r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), nRad(a), 0, 0, 0, 0, 1, c, 0.0));
    }
    for (int i = 0; i < 500; i++) {
        double a = r.normal(47.75, 0.25);
        float c[3];
        shade(c, twot, r.range(0.6, 1.0));
        v.push_back(make(a, std::clamp(r.normal(0.2, 0.05), 0.05, 0.3), r.rayleigh(8.0, 25.0) * DEG, r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), nRad(a), 0, 0, 0, 0, 1, c, 0.0));
    }
    addResonant(v, r, 2400, 3, 39.45, 0.22, 0.22, 0.05, 9.0, 30.0, 1.4, 2.0 * PI_D / (20000.0 * YEAR_D), plut, 1);
    for (int i = 0; i < 800; i++) {
        double q = r.range(32.0, 38.0);
        double a = 40.0 + 60.0 * std::pow(r.u(), 2.0);
        float c[3];
        shade(c, scat, r.range(0.6, 1.0));
        v.push_back(make(a, std::min(0.9, 1.0 - q / a), r.rayleigh(14.0, 40.0) * DEG, r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), r.range(0, 2 * PI_D), nRad(a), 0, 0, 0, 0, 1, c, 0.0));
    }
}

}

BeltState g_belts;

const BeltNamed& beltNamed(int i) {
    return NAMED[i];
}

double beltsLamJ(double t) {
    return (34.39644051 + 3034.74612775 * t / 36525.0) * DEG;
}

double beltsLamN(double t) {
    return (-55.12002969 + 218.45945325 * t / 36525.0) * DEG;
}

double beltsRingAu(int k) {
    const double ring[8] = {2.5, 2.82, 2.95, 3.27, 3.97, 2.7, 45.0, 39.4};
    return ring[k];
}

std::vector<BeltVertex> beltsGenerate() {
    Rng r;
    std::vector<BeltVertex> v;
    v.reserve(64000);
    addMainBelt(v, r, 48000);
    const float hilda[3] = {0.8f, 0.62f, 0.55f};
    addResonant(v, r, 1800, 2, 3.97, 0.045, 0.16, 0.04, 6.0, 20.0, 1.2, 2.0 * PI_D / (280.0 * YEAR_D), hilda, 0);
    addTrojans(v, r, 1700, PI_D / 3.0);
    addTrojans(v, r, 1700, -PI_D / 3.0);
    addKuiper(v, r);
    for (int i = 0; i < BELT_NAMED; i++) {
        const BeltNamed& n = NAMED[i];
        v.push_back(make(n.a, n.e, n.inc * DEG, n.node * DEG, n.arg * DEG, n.m0 * DEG, nRad(n.a), 0, 0, 0, 0, 2, n.col, n.diameterKm * 0.5 / 149597870.7));
    }
    return v;
}
