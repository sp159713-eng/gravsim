#include "impact.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

const ImpactPreset IM_PRESETS[IM_PRESETS_N] = {
    {"CHELYABINSK 2013", "15 FEB 2013 - AIRBURST OVER RUSSIA, 1500 HURT BY BROKEN GLASS", 19.0f, 19.0f, 18.0f, 0, false},
    {"TUNGUSKA 1908", "30 JUN 1908 - AIRBURST OVER SIBERIA, 2000 KM2 OF FOREST FLATTENED", 55.0f, 15.0f, 30.0f, 0, false},
    {"METEOR CRATER", "ARIZONA, ~50000 YEARS AGO - THE REAL CRATER IS 1.2 KM WIDE", 36.0f, 12.8f, 45.0f, 1, false},
    {"CHICXULUB", "66 MILLION YEARS AGO - THE DINOSAUR KILLER", 10000.0f, 20.0f, 45.0f, 0, false},
    {"APOPHIS 2029", "13 APR 2029 - WILL MISS EARTH BY ~32000 KM, INSIDE THE RING OF GEOSTATIONARY SATELLITES", 370.0f, 12.6f, 45.0f, 0, true},
};

float imDensity(int type) {
    static const float d[IM_TYPES] = {3000.0f, 8000.0f, 1000.0f};
    return d[type % IM_TYPES];
}

const char* imTypeName(int type) {
    static const char* n[IM_TYPES] = {"STONE", "IRON", "COMET ICE"};
    return n[type % IM_TYPES];
}

namespace {

const double PI = 3.14159265358979;

struct Anchor {
    double mt, yr;
};

double recurrence(double mt) {
    static const Anchor a[] = {{0.001, 0.19}, {0.5, 50.0}, {10.0, 300.0}, {1500.0, 6.0e4}, {7.5e4, 5.0e5}, {7.5e7, 1.0e8}, {1.0e9, 7.5e8}};
    const int n = (int)(sizeof a / sizeof a[0]);
    int i = 0;
    while (i < n - 2 && mt > a[i + 1].mt) i++;
    double x0 = std::log10(a[i].mt), x1 = std::log10(a[i + 1].mt);
    double y0 = std::log10(a[i].yr), y1 = std::log10(a[i + 1].yr);
    double y = y0 + (y1 - y0) * (std::log10(std::max(mt, 1e-9)) - x0) / (x1 - x0);
    return std::pow(10.0, y);
}

}

ImpactResult imCompute(const ImpactParams& p) {
    ImpactResult r;
    const double rho = imDensity(p.type), L = p.diaM, v = p.speedKms * 1000.0;
    const double th = p.angleDeg * PI / 180.0, sn = std::sin(th);
    const double mass = rho * PI / 6.0 * L * L * L;
    r.massKg = (float)mass;
    r.energyJ = 0.5 * mass * v * v;
    r.energyMt = r.energyJ / 4.184e15;
    r.hiroshimas = r.energyMt / 0.015;
    r.tsars = r.energyMt / 50.0;

    static const double dcrit[IM_TYPES] = {50.0, 30.0, 150.0};
    static const double yield[IM_TYPES] = {5.0e6, 5.0e7, 1.0e6};
    double dc = dcrit[p.type % IM_TYPES] * std::sqrt(0.70711 / sn);
    r.airburst = L < dc;
    if (r.airburst) {
        const double H = 8000.0;
        double hb = H * std::log(std::max(1.0, 1.2 * v * v / yield[p.type % IM_TYPES]));
        double rair = 1.2 * std::exp(-hb / H);
        double drop = 2.0 * H * std::log(1.0 + (L / (2.0 * H)) * std::sqrt(rho / rair));
        double alt = std::max(0.0, hb - drop) * (1.0 - std::pow(L / dc, 6.0));
        r.burstKm = (float)std::min(alt / 1000.0, 60.0);
    } else {
        double dtc = 1.161 * std::cbrt(rho / 2500.0) * std::pow(L, 0.78) * std::pow(v, 0.44) * std::pow(9.81, -0.22) * std::cbrt(sn);
        double fin = 1.25 * dtc;
        if (fin >= 3200.0) {
            fin = 1.17 * std::pow(dtc, 1.13) / std::pow(3200.0, 0.13);
            r.complexCrater = true;
        }
        r.transientKm = (float)(dtc / 1000.0);
        r.finalKm = (float)(fin / 1000.0);
    }
    r.fireballKm = (float)(0.002 * std::cbrt(r.energyJ) / 1000.0);
    double cr = std::cbrt(std::max(r.energyMt, 1e-12));
    r.r5Km = (float)(6.0 * cr);
    r.r1Km = (float)(17.0 * cr);
    r.recurYears = recurrence(r.energyMt);
    r.veil = (float)std::max(0.0, std::min(1.0, (std::log10(std::max(r.energyMt, 1e-9)) - 4.5) / 1.5));
    r.extinction = r.energyMt >= 1.0e7;
    return r;
}

std::string imNum(double v) {
    char b[48];
    if (v >= 1.0e6 || (v > 0.0 && v < 0.01)) {
        int e = (int)std::floor(std::log10(v));
        double m = v / std::pow(10.0, e);
        std::snprintf(b, sizeof b, "%.1fE%d", m, e);
    } else if (v >= 100.0) {
        long long n = (long long)std::llround(v);
        if (n >= 1000) std::snprintf(b, sizeof b, "%lld %03lld", n / 1000, n % 1000);
        else std::snprintf(b, sizeof b, "%lld", n);
    } else if (v >= 10.0) {
        std::snprintf(b, sizeof b, "%.0f", v);
    } else {
        std::snprintf(b, sizeof b, "%.1f", v);
    }
    return b;
}

std::string imKm(float km) {
    char b[48];
    if (km < 1.0f) std::snprintf(b, sizeof b, "%d M", (int)std::lround(km * 1000.0f));
    else if (km < 100.0f) std::snprintf(b, sizeof b, "%.1f KM", km);
    else std::snprintf(b, sizeof b, "%s KM", imNum(km).c_str());
    return b;
}

std::string imRecur(double y) {
    char b[64];
    if (y < 0.5) return "SEVERAL TIMES A YEAR";
    if (y < 1.5) return "ABOUT ONCE A YEAR";
    if (y < 1000.0) {
        double r = y < 100.0 ? std::round(y / 5.0) * 5.0 : std::round(y / 10.0) * 10.0;
        std::snprintf(b, sizeof b, "EVERY ~%d YEARS", (int)std::max(2.0, r));
    } else if (y < 1.0e6) {
        double r = y < 1.0e4 ? std::round(y / 100.0) * 100.0 : std::round(y / 1000.0) * 1000.0;
        std::snprintf(b, sizeof b, "EVERY ~%s YEARS", imNum(r).c_str());
    } else if (y < 1.0e9) {
        std::snprintf(b, sizeof b, "EVERY ~%d MILLION YEARS", (int)std::lround(y / 1.0e6));
    } else {
        std::snprintf(b, sizeof b, "EVERY ~%.1f BILLION YEARS", y / 1.0e9);
    }
    return b;
}
