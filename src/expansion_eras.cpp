#include "expansion.h"
#include "cosmo.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr double Y = cosmo::YR_S;
constexpr double G = cosmo::GYR_S;

}

const std::vector<ExEra>& exEras() {
    static const std::vector<ExEra> E = {
        {1e-43, 1e-36, "PLANCK ERA - 1E-43 S",
         "GRAVITY AND QUANTUM PHYSICS ARE ONE FORCE - NO KNOWN PHYSICS CAN DESCRIBE THIS MOMENT.",
         "THE TEMPERATURE IS ABOUT 10^32 K AND THE WHOLE VISIBLE UNIVERSE IS SMALLER THAN A PROTON.",
         "PLANCK", {0.9f, 0.8f, 1.0f}},
        {1e-36, 1e-32, "INFLATION - 1E-36 TO 1E-32 S",
         "SPACE STRETCHES BY AT LEAST A FACTOR OF 10^26 IN A TRILLIONTH OF A TRILLIONTH OF A TRILLIONTH OF A SECOND.",
         "TINY QUANTUM RIPPLES ARE STRETCHED INTO THE SEEDS OF EVERY GALAXY. (SCALE FACTOR SHOWN IS SCHEMATIC HERE)",
         "INFLATION", {1.0f, 0.7f, 0.9f}},
        {1e-32, 1e-6, "QUARK-GLUON PLASMA",
         "A WHITE-HOT SOUP OF FREE QUARKS AND GLUONS, TRILLIONS OF DEGREES.",
         "THE FUNDAMENTAL FORCES SEPARATE AND THE HIGGS FIELD SWITCHES ON.",
         "QUARK SOUP", {1.0f, 0.85f, 0.7f}},
        {1e-6, 180.0, "FIRST PROTONS AND NEUTRONS - 1E-6 S",
         "THE PLASMA COOLS ENOUGH FOR QUARKS TO LOCK TOGETHER INTO PROTONS AND NEUTRONS.",
         "AT ABOUT 1 SECOND NEUTRINOS BREAK FREE - THE UNIVERSE IS 10 BILLION DEGREES.",
         "PROTONS", {1.0f, 0.8f, 0.5f}},
        {180.0, 3.79e5 * Y, "NUCLEOSYNTHESIS - 3 MINUTES",
         "PROTONS AND NEUTRONS FUSE INTO HELIUM AND TRACES OF LITHIUM IN ABOUT 17 MINUTES.",
         "RESULT: ABOUT 75% HYDROGEN AND 25% HELIUM BY MASS - STILL FAR TOO HOT FOR ATOMS.",
         "3 MIN", {1.0f, 0.75f, 0.4f}},
        {3.79e5 * Y, 1e6 * Y, "RECOMBINATION - THE CMB IS RELEASED - 380 THOUSAND YEARS",
         "AT ABOUT 3000 K ELECTRONS JOIN NUCLEI AND THE OPAQUE FOG OF PLASMA CLEARS.",
         "THE LIGHT THAT ESCAPED THEN IS THE COSMIC MICROWAVE BACKGROUND - NOW STRETCHED TO 2.7 K.",
         "CMB", {1.0f, 0.6f, 0.3f}},
        {1e6 * Y, 1e8 * Y, "THE DARK AGES",
         "NO STARS YET - ONLY HYDROGEN AND HELIUM GAS AND DARK MATTER CLUMPING UNDER GRAVITY.",
         "THIS ERA IS INVISIBLE TO TELESCOPES EXCEPT AS FAINT 21 CM RADIO WAVES.",
         "DARK AGES", {0.6f, 0.5f, 0.9f}},
        {1e8 * Y, 2e8 * Y, "FIRST STARS - 100 TO 200 MILLION YEARS",
         "HUGE, HOT, METAL-FREE STARS IGNITE IN THE DARK MATTER CLUMPS AND LIGHT UP THE UNIVERSE.",
         "THEIR ULTRAVIOLET LIGHT STRIPS THE ELECTRONS OFF THE HYDROGEN GAS AGAIN.",
         "FIRST STARS", {0.7f, 0.85f, 1.0f}},
        {2e8 * Y, 7.6 * G, "GALAXIES FORM AND GROW",
         "GAS AND DARK MATTER PULL TOGETHER INTO GALAXIES THAT MERGE AND FILL WITH STARS.",
         "STAR BIRTH PEAKS ABOUT 3.5 BILLION YEARS AFTER THE BANG, 10 BILLION YEARS AGO.",
         "GALAXIES", {0.8f, 0.9f, 1.0f}},
        {7.6 * G, 9.2 * G, "THE TURNING POINT - 7.6 BILLION YEARS",
         "MATTER THINS OUT AS SPACE GROWS WHILE DARK ENERGY DOES NOT: THE SLOWDOWN ENDS HERE.",
         "FROM NOW ON THE EXPANSION SPEEDS UP (REDSHIFT Z 0.64, ABOUT 6 BILLION YEARS AGO).",
         "SPEED-UP 7.6", {1.0f, 0.9f, 0.5f}},
        {9.2 * G, 9.5 * G, "THE SUN FORMS - 9.2 BILLION YEARS",
         "A CLOUD OF GAS AND DUST COLLAPSES 4.6 BILLION YEARS AGO AND BUILDS OUR SOLAR SYSTEM.",
         "THE UNIVERSE IS ALREADY EXPANDING FASTER AND FASTER.",
         "SUN 9.2", {1.0f, 0.85f, 0.3f}},
        {9.5 * G, 13.6 * G, "DARK ENERGY TAKES OVER - ABOUT 9-10 BILLION YEARS",
         "THE EXPANSION STARTED SPEEDING UP - DARK ENERGY OUTWEIGHS MATTER FROM ABOUT 10.2 GYR ON.",
         "NEW DATA HINTS DARK ENERGY MAY BE WEAKENING - STILL UNCERTAIN (DESI 2025).",
         "DARK ENERGY", {1.0f, 0.55f, 0.4f}},
        {13.6 * G, 13.95 * G, "NOW - 13.8 BILLION YEARS",
         "BY DEFINITION A = 1. THE CMB IS 2.7 K AND SPACE EXPANDS AT 67.7 KM/S PER MEGAPARSEC.",
         "ABOUT 69% DARK ENERGY, 26% DARK MATTER AND ONLY 5% ORDINARY MATTER.",
         "NOW 13.8", {0.5f, 1.0f, 0.6f}},
        {13.95 * G, 25.0 * G, "THE FUTURE - THE EXPANSION KEEPS ACCELERATING",
         "GALAXY CLUSTERS DRIFT APART WHILE THE LOCAL GROUP (MILKY WAY + ANDROMEDA) STAYS BOUND.",
         "IN ABOUT 5 BILLION YEARS THE SUN BECOMES A RED GIANT.",
         "FUTURE", {0.6f, 0.9f, 0.7f}},
        {25.0 * G, 1e3 * G, "FAR FUTURE - GALAXIES BEYOND THE LOCAL GROUP FADE FROM VIEW",
         "THEIR LIGHT IS STRETCHED SO FAR THAT IT DIMS AND FINALLY NEVER REACHES US.",
         "A FUTURE ASTRONOMER WOULD SEE ONLY OUR OWN GALAXY GROUP IN AN EMPTY DARK SKY.",
         "GALAXIES FADE", {0.7f, 0.7f, 0.9f}},
    };
    return E;
}

int exEraAt(double tSec) {
    const std::vector<ExEra>& E = exEras();
    int r = 0;
    for (int i = 0; i < (int)E.size(); i++)
        if (tSec >= E[i].t0 * 0.9999) r = i;
    return r;
}

double exTimeToU(double tSec) {
    if (tSec <= 1.0) return EX_BREAK * (std::log10(std::max(tSec, EX_TMIN)) + 43.0) / 43.0;
    return EX_BREAK + (1.0 - EX_BREAK) * std::min(1.0, tSec / cosmo::GYR_S / EX_TEND_GYR);
}

double exUToTime(double u) {
    u = std::clamp(u, 0.0, 1.0);
    if (u <= EX_BREAK) return std::pow(10.0, u / EX_BREAK * 43.0 - 43.0);
    return std::max(1.0, (u - EX_BREAK) / (1.0 - EX_BREAK) * EX_TEND_GYR * cosmo::GYR_S);
}

void exTempColor(double kelvin, float rgb[3]) {
    struct P {
        float lg, r, g, b;
    };
    static const P pts[] = {
        {5.0f, 0.52f, 0.50f, 0.46f}, {4.0f, 0.50f, 0.40f, 0.26f}, {3.48f, 0.38f, 0.16f, 0.05f},
        {3.2f, 0.22f, 0.07f, 0.025f}, {2.7f, 0.08f, 0.02f, 0.012f}, {2.0f, 0.016f, 0.012f, 0.022f},
    };
    float lg = (float)std::log10(std::max(kelvin, 1.0));
    if (lg >= pts[0].lg) {
        rgb[0] = pts[0].r; rgb[1] = pts[0].g; rgb[2] = pts[0].b;
        return;
    }
    for (int i = 0; i < 5; i++) {
        if (lg <= pts[i].lg && lg >= pts[i + 1].lg) {
            float f = (pts[i].lg - lg) / (pts[i].lg - pts[i + 1].lg);
            rgb[0] = pts[i].r + f * (pts[i + 1].r - pts[i].r);
            rgb[1] = pts[i].g + f * (pts[i + 1].g - pts[i].g);
            rgb[2] = pts[i].b + f * (pts[i + 1].b - pts[i].b);
            return;
        }
    }
    rgb[0] = pts[5].r; rgb[1] = pts[5].g; rgb[2] = pts[5].b;
}

std::string exFmtAge(double t) {
    char b[64];
    double yr = t / cosmo::YR_S;
    if (t < 0.1) std::snprintf(b, sizeof b, "%.1E S", t);
    else if (t < 60.0) std::snprintf(b, sizeof b, "%.1f S", t);
    else if (t < 3600.0) std::snprintf(b, sizeof b, "%.1f MIN", t / 60.0);
    else if (t < 86400.0 * 2.0) std::snprintf(b, sizeof b, "%.1f HOURS", t / 3600.0);
    else if (yr < 1.0) std::snprintf(b, sizeof b, "%.0f DAYS", t / 86400.0);
    else if (yr < 1e3) std::snprintf(b, sizeof b, "%.0f YEARS", yr);
    else if (yr < 1e6) std::snprintf(b, sizeof b, "%.0f THOUSAND YEARS", yr / 1e3);
    else if (yr < 1e9) std::snprintf(b, sizeof b, "%.1f MILLION YEARS", yr / 1e6);
    else std::snprintf(b, sizeof b, "%.2f BILLION YEARS", yr / 1e9);
    return b;
}

std::string exFmtTemp(double k) {
    char b[48];
    if (k >= 1e5) std::snprintf(b, sizeof b, "%.1E K", k);
    else if (k >= 100.0) std::snprintf(b, sizeof b, "%.0f K", k);
    else std::snprintf(b, sizeof b, "%.1f K", k);
    return b;
}

std::string exFmtHubble(double h) {
    char b[48];
    if (h < 1e5) std::snprintf(b, sizeof b, "%.1f KM/S/MPC", h);
    else std::snprintf(b, sizeof b, "%.2E KM/S/MPC", h);
    return b;
}

std::string exFmtScale(double a) {
    char b[48];
    if (a < 0.01) std::snprintf(b, sizeof b, "%.2E", a);
    else std::snprintf(b, sizeof b, "%.3f", a);
    return b;
}

int cosmoCheck() {
    double tn = cosmo::nowSec();
    double ta = cosmo::turnA();
    std::printf("age at a=1: %.3f Gyr   a(now)=%.6f\n", tn / cosmo::GYR_S, cosmo::scaleAt(tn));
    std::printf("q=0 at a=%.4f  z=%.3f  t=%.2f Gyr  (%.2f Gyr ago)\n", ta, 1.0 / ta - 1.0, cosmo::timeAt(ta) / cosmo::GYR_S, (tn - cosmo::timeAt(ta)) / cosmo::GYR_S);
    std::printf("a(380kyr)=%.6f (1/a=%.1f) T=%.0f K\n", cosmo::scaleAt(380000.0 * cosmo::YR_S), 1.0 / cosmo::scaleAt(380000.0 * cosmo::YR_S), cosmo::tempK(cosmo::scaleAt(380000.0 * cosmo::YR_S)));
    std::printf("a(3 Gyr)=%.4f q=%.3f   a(50 Gyr)=%.3f q=%.3f\n", cosmo::scaleAt(3.0 * cosmo::GYR_S), cosmo::decel(cosmo::scaleAt(3.0 * cosmo::GYR_S)), cosmo::scaleAt(50.0 * cosmo::GYR_S), cosmo::decel(cosmo::scaleAt(50.0 * cosmo::GYR_S)));
    std::printf("H(now)=%.2f km/s/Mpc  T(now)=%.3f K\n", cosmo::hubble(1.0), cosmo::tempK(1.0));
    const double zs[3] = {0.597, 2.22, 0.87};
    for (double z : zs) std::printf("z=%.3f lookback=%.2f Gyr\n", z, cosmo::lookbackGyrOfZ(z));
    return 0;
}
