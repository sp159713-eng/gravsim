#include "futuresky.h"
#include <algorithm>
#include <cmath>

namespace {

struct Key {
    FsGal a, b;
    float band, warp, burst;
};

const Key KEYS[FS_STAGES] = {
    {{-32, 44, 0.88f, 38, 0.30f, 1.0f, 0.0f, 0.0f, 2.4f}, {28, 24, 2.3f, 0, 1.0f, 0, 0, 0, 0.0f}, 1.0f, 0.0f, 0.0f},
    {{-27, 49, 3.09f, 30, 0.30f, 1.0f, 0.0f, 0.0f, 0.8f}, {28, 24, 2.3f, 0, 1.0f, 0, 0, 0, 0.0f}, 1.0f, 0.1f, 0.0f},
    {{-12, 56, 3.95f, 20, 0.55f, 1.0f, 0.0f, 0.45f, 0.9f}, {24, 30, 2.9f, -20, 0.6f, 0.4f, 0, 0.5f, 0.4f}, 1.0f, 0.7f, 0.25f},
    {{-4, 60, 4.13f, 12, 0.70f, 1.0f, 0.05f, 0.8f, 1.0f}, {16, 42, 3.26f, -25, 0.6f, 0.6f, 0, 0.8f, 0.7f}, 0.95f, 1.0f, 1.0f},
    {{2, 62, 4.03f, 5, 0.80f, 1.0f, 0.1f, 1.0f, 1.0f}, {8, 50, 3.53f, -20, 0.7f, 0.8f, 0.1f, 1.0f, 0.9f}, 0.7f, 1.0f, 0.7f},
    {{15, 67, 3.3f, 0, 0.85f, 0.55f, 0.55f, 0.5f, 1.1f}, {-15, 61, 3.2f, 0, 0.85f, 0.5f, 0.55f, 0.5f, 1.1f}, 0.35f, 0.6f, 0.2f},
    {{0, 62, 4.13f, 0, 0.90f, 0.0f, 1.0f, 0.0f, 0.6f}, {0, 62, 4.0f, 0, 0.88f, 0.0f, 1.0f, 0.0f, 0.6f}, 0.0f, 0.0f, 0.0f},
};

float cr(float p0, float p1, float p2, float p3, float u) {
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u * u * u);
}

void gal(const FsGal* g[4], float u, FsGal& o) {
    const float* f[4] = {(const float*)g[0], (const float*)g[1], (const float*)g[2], (const float*)g[3]};
    float* r = (float*)&o;
    for (int i = 0; i < 9; i++) r[i] = cr(f[0][i], f[1][i], f[2][i], f[3][i], u);
    o.lnR = std::max(o.lnR, 0.0f);
    o.q = std::min(1.0f, std::max(0.05f, o.q));
    o.disc = std::min(1.0f, std::max(0.0f, o.disc));
    o.ell = std::min(1.0f, std::max(0.0f, o.ell));
    o.tide = std::min(1.0f, std::max(0.0f, o.tide));
    o.bright = std::max(0.0f, o.bright);
}

const char* HEADS[FS_STAGES] = {
    "NOW - A FAINT SMUDGE IN THE NIGHT SKY",
    "+2 BILLION YEARS - ANDROMEDA GROWS LARGE",
    "+3.75 BILLION YEARS - ANDROMEDA FILLS THE SKY",
    "+3.85 BILLION YEARS - A SKY ABLAZE",
    "+4 BILLION YEARS - BOTH GALAXIES TIDALLY TORN",
    "+5.1 BILLION YEARS - TWIN BRIGHT CORES",
    "+7 BILLION YEARS - MILKOMEDA",
};

const char* DETAILS[FS_STAGES] = {
    "ANDROMEDA IS ABOUT 3 DEGREES WIDE - 6 FULL MOONS - BUT ONLY ITS CORE SHOWS TO THE EYE",
    "IT HAS BEEN FALLING TOWARD US ALL THIS TIME AND NOW LOOMS OVER THE NIGHT SKY",
    "THE MILKY WAY BAND IS DISTORTED BY TIDES AS THE TWO GALAXIES PULL ON EACH OTHER",
    "COLLIDING GAS CLOUDS IGNITE STAR FORMATION - PINK AND BLUE NEBULAE EVERYWHERE",
    "LONG TAILS OF STARS ARE FLUNG OUT AND BRIGHT NEW STAR CLUSTERS LIGHT UP",
    "THE TWO GALACTIC CORES CIRCLE EACH OTHER, ABOUT TO MERGE",
    "ONE GIANT ELLIPTICAL GALAXY GLOWS ACROSS THE SKY - THE MILKY WAY BAND IS GONE",
};

}

void fsEval(float t, FsParams& out) {
    t = std::min(FS_MAX_GYR, std::max(0.0f, t));
    int i = 0;
    while (i < FS_STAGES - 2 && t >= FS_STAGE_T[i + 1]) i++;
    float u = (t - FS_STAGE_T[i]) / (FS_STAGE_T[i + 1] - FS_STAGE_T[i]);
    int idx[4] = {std::max(0, i - 1), i, i + 1, std::min(FS_STAGES - 1, i + 2)};
    const FsGal* ga[4];
    const FsGal* gb[4];
    for (int k = 0; k < 4; k++) {
        ga[k] = &KEYS[idx[k]].a;
        gb[k] = &KEYS[idx[k]].b;
    }
    gal(ga, u, out.a);
    gal(gb, u, out.b);
    auto sc = [&](float Key::*m) {
        return std::min(1.0f, std::max(0.0f, cr(KEYS[idx[0]].*m, KEYS[idx[1]].*m, KEYS[idx[2]].*m, KEYS[idx[3]].*m, u)));
    };
    out.band = sc(&Key::band);
    out.warp = sc(&Key::warp);
    out.burst = sc(&Key::burst);
    out.lum = 1.0f / (1.0f - 0.4f * t / 4.57f);
    out.sun = std::min(1.0f, std::max(0.0f, (out.lum - 1.0f) / 1.6f));
}

int fsStage(float t) {
    int i = 0;
    while (i < FS_STAGES - 1 && t >= FS_STAGE_T[i + 1] - 1e-4f) i++;
    return i;
}

const char* fsStageHead(int i) { return HEADS[i]; }
const char* fsStageDetail(int i) { return DETAILS[i]; }
