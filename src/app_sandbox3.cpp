#include "vk_app.h"
#include <cmath>
#include <cstdio>

namespace {

struct Planet {
    const char* name;
    double au, mass, radius;
    int type;
    float col[3];
};

const Planet PLANETS[8] = {
    {"MERCURY", 0.387, 0.055, 0.9, 0, {0.66f, 0.62f, 0.58f}},
    {"VENUS", 0.723, 0.815, 1.5, 0, {0.92f, 0.8f, 0.5f}},
    {"EARTH", 1.0, 1.0, 1.6, 0, {0.35f, 0.6f, 1.0f}},
    {"MARS", 1.524, 0.107, 1.1, 0, {0.85f, 0.4f, 0.25f}},
    {"JUPITER", 5.203, 317.8, 4.4, 1, {0.85f, 0.68f, 0.48f}},
    {"SATURN", 9.537, 95.2, 4.0, 1, {0.92f, 0.82f, 0.55f}},
    {"URANUS", 19.19, 14.5, 3.5, 1, {0.45f, 0.88f, 0.95f}},
    {"NEPTUNE", 30.07, 17.1, 3.4, 1, {0.22f, 0.4f, 0.95f}},
};

struct Info {
    const char* name;
    const char* fact;
    float dist, pitch;
    size_t trail;
};

const Info INFO[8] = {
    {"SOLAR SYSTEM", "8 PLANETS, ONE STAR - THE SUN HOLDS 99.8 PERCENT OF THE SYSTEM MASS", 300.0f, 0.9f, 80},
    {"TRAPPIST-1", "7 EARTH-SIZED WORLDS ORBIT CLOSER THAN MERCURY - 3 MAY BE IN THE HABITABLE ZONE", 100.0f, 0.95f, 80},
    {"KEPLER-16 - TATOOINE", "A PLANET ORBITING TWO SUNS, 245 LIGHT-YEARS AWAY - LIKE LUKE SKYWALKER HOME", 190.0f, 0.9f, 140},
    {"BINARY MASS TRANSFER", "A RED GIANT FEEDS A WHITE DWARF - THIS CAN END IN A TYPE IA SUPERNOVA", 170.0f, 1.0f, 60},
    {"THREE-BODY FIGURE-8", "THREE EQUAL MASSES SHARE ONE FIGURE-8 PATH - FOUND IN 1993, PROVEN IN 2000", 170.0f, 1.35f, 600},
    {"CHAOTIC THREE-BODY", "NO FORMULA PREDICTS THIS - THE THREE-BODY PROBLEM", 160.0f, 1.2f, 300},
    {"ROGUE PLANET FLYBY", "BILLIONS OF ROGUE PLANETS MAY WANDER THE MILKY WAY WITH NO STAR", 300.0f, 0.9f, 120},
    {"BLACK HOLE VS SOLAR SYSTEM", "A 1-SUN BLACK HOLE IS ONLY ABOUT 6 KM WIDE - YET IT WRECKS EVERY ORBIT", 300.0f, 0.9f, 120},
};

const double SUN_GM = 10000.0;
const double MASS_SCALE = 0.02;
const double ORBIT_SCALE = 24.0;

}

void sbSolarBuild(SandboxState& S) {
    SbBody sun;
    sun.m = SUN_GM;
    sun.r = 8.0;
    sun.type = 2;
    sun.sun = true;
    sun.name = "SUN";
    sun.col[0] = 1.0f; sun.col[1] = 0.82f; sun.col[2] = 0.35f;
    S.bodies.push_back(sun);
    for (int i = 0; i < 8; i++) {
        const Planet& P = PLANETS[i];
        double r = 12.0 + ORBIT_SCALE * (std::log(1.0 + 2.0 * P.au) - std::log(1.0 + 2.0 * PLANETS[0].au));
        double phi = 0.9 * i + 0.4;
        double sp = std::sqrt(SUN_GM / r);
        SbBody b;
        b.x = r * std::cos(phi);
        b.z = r * std::sin(phi);
        b.vx = -std::sin(phi) * sp;
        b.vz = std::cos(phi) * sp;
        b.m = P.mass * MASS_SCALE;
        b.r = P.radius;
        b.type = P.type;
        b.name = P.name;
        for (int c = 0; c < 3; c++) b.col[c] = P.col[c];
        S.bodies.push_back(b);
    }
}

void sbPresetBegin(AppContext& app, int idx, const char* name, const char* fact) {
    SandboxState& S = app.sb;
    if (!S.ready) sbRestart(app);
    S.bodies.clear();
    S.flashes.clear();
    S.preview.clear();
    S.parts.clear();
    S.emitAcc = 0;
    S.dragging = false;
    S.full = false;
    S.mk = 0;
    S.clock = 0;
    S.preset = idx;
    S.trailCap = INFO[idx].trail;
    std::snprintf(S.capName, sizeof(S.capName), "%d OF 8   %s", idx + 1, name);
    std::snprintf(S.capFact, sizeof(S.capFact), "%s", fact);
    S.capT = 8.0f;
    app.camera.dist = S.cam[2] = INFO[idx].dist;
    app.camera.pitch = S.cam[1] = INFO[idx].pitch;
    for (int a = 0; a < 3; a++) app.camera.target[a] = S.cam[3 + a] = 0.0f;
}

void sbPresetLoad(AppContext& app, int idx) {
    idx = ((idx % 8) + 8) % 8;
    sbPresetBegin(app, idx, INFO[idx].name, INFO[idx].fact);
    if (idx == 0) sbSolarBuild(app.sb);
    else sbPresetBuildMore(app, idx);
}

void sbPresetStep(AppContext& app, int dir) {
    int cur = app.sb.preset;
    sbPresetLoad(app, cur < 0 ? (dir > 0 ? 0 : 7) : cur + dir);
}

void sbSolarPreset(AppContext& app) {
    sbPresetLoad(app, 0);
}
