#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr double T_PASS = 9.0, T_MERGE = 36.0, T_END = 46.0;
constexpr float KLY_PER_UNIT = 4.3f;
constexpr float SUN_START = 6.05f;

struct Stage {
    float gyr;
    const char* text;
};

const Stage STAGES[4] = {
    {0.0f, "NOW"},
    {2.0f, "+2 BILLION YEARS: FIRST PASS"},
    {4.5f, "+4.5 BILLION YEARS: MERGER"},
    {6.0f, "+6 BILLION YEARS: MILKOMEDA (AN ELLIPTICAL GALAXY)"},
};

float gyrOf(double t) {
    if (t < T_PASS) return (float)(2.0 * t / T_PASS);
    if (t < T_MERGE) return (float)(2.0 + 2.5 * (t - T_PASS) / (T_MERGE - T_PASS));
    if (t < T_END) return (float)(4.5 + 1.5 * (t - T_MERGE) / (T_END - T_MERGE));
    return (float)(6.0 + 0.1 * (t - T_END));
}

bool project(const float m[16], const float p[3], float& x, float& y) {
    float clip[4];
    for (int row = 0; row < 4; row++)
        clip[row] = m[row] * p[0] + m[row + 4] * p[1] + m[row + 8] * p[2] + m[row + 12];
    if (clip[3] <= 1e-9f) return false;
    x = clip[0] / clip[3];
    y = clip[1] / clip[3];
    return true;
}

void frameTarget(const AppContext& app, const float a[3], const float b[3], float yaw, float pitch, float out[3]) {
    const float rA = 11.0f, rB = 19.25f;
    float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    float sep = std::max(1e-3f, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
    float len = sep + rA + rB;
    float s0 = -rA, mid = s0 + 0.5f * len;
    float c[3];
    for (int i = 0; i < 3; i++) c[i] = a[i] + d[i] / sep * mid;
    float aspect = (float)app.swapchainExtent.width / (float)std::max(1u, app.swapchainExtent.height);
    float half = std::tan(30.0f * 3.14159265f / 180.0f);
    float dist = 0.5f * len * 1.3f / std::min(half, half * aspect);
    float f[3] = {-std::cos(pitch) * std::sin(yaw), -std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
    for (int i = 0; i < 3; i++) out[i] = c[i] - f[i] * dist;
}

}

void andromedaFrameReset(AppContext& app) {
    AndFrame& f = app.andFrame;
    f.on = app.scene == Scene::ANDROMEDA;
    if (!f.on) return;
    Camera& c = app.camera;
    const float a[3] = {-55.0f, 0.0f, -3.0f}, b[3] = {55.0f, 0.0f, 3.0f};
    frameTarget(app, a, b, c.yaw, c.pitch, c.pos);
    for (int i = 0; i < 3; i++) f.pos[i] = c.pos[i];
    f.yaw = c.yaw;
    f.pitch = c.pitch;
}

void andromedaFrameStep(AppContext& app, float dt) {
    AndFrame& f = app.andFrame;
    if (!f.on) return;
    Camera& c = app.camera;
    bool moved = app.scene != Scene::ANDROMEDA || !c.free || app.follow >= 0 || c.yaw != f.yaw || c.pitch != f.pitch;
    for (int i = 0; i < 3; i++) moved = moved || c.pos[i] != f.pos[i];
    if (moved || app.simTime >= T_PASS || !app.readValid[app.currentFrame] || app.n < 2) {
        if (moved || app.simTime >= T_PASS) f.on = false;
        return;
    }
    const Particle* p = static_cast<const Particle*>(app.readMapped[app.currentFrame]);
    float goal[3];
    frameTarget(app, p[0].pos, p[1].pos, c.yaw, c.pitch, goal);
    float k = 1.0f - std::exp(-std::min(dt, 0.1f) * 1.5f);
    for (int i = 0; i < 3; i++) {
        c.pos[i] += (goal[i] - c.pos[i]) * k;
        f.pos[i] = c.pos[i];
    }
}

void andromedaEmit(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (app.scene != Scene::ANDROMEDA) return;
    float px = 2.0f / (float)app.swapchainExtent.width;
    float py = 2.0f / (float)app.swapchainExtent.height;
    float gyr = gyrOf(app.simTime);
    int stage = 0;
    for (int i = 1; i < 4; i++)
        if (gyr >= STAGES[i].gyr) stage = i;
    const char* cap = STAGES[stage].text;
    float w = (float)std::strlen(cap) * 6.0f * 3.0f * px;
    emitText(out, used, cap, -0.5f * w, 1.0f - 56.0f * py, px, py, 3.0f, 0.95f, 0.9f, 0.7f);
    char clock[48];
    std::snprintf(clock, sizeof(clock), "%.1f BILLION YEARS FROM NOW", (double)gyr);
    w = (float)std::strlen(clock) * 6.0f * 2.0f * px;
    emitText(out, used, clock, -0.5f * w, 1.0f - 88.0f * py, px, py, 2.0f, 0.6f, 0.75f, 0.95f);

    if (!app.readValid[app.currentFrame] || app.n <= ANDROMEDA_SUN) return;
    const Particle* p = static_cast<const Particle*>(app.readMapped[app.currentFrame]);
    const Particle& sun = p[ANDROMEDA_SUN];
    float best = 1e30f;
    for (int h = 0; h < 2; h++) {
        if (p[h].pos[3] <= 0.0f) continue;
        float d = std::sqrt((sun.pos[0] - p[h].pos[0]) * (sun.pos[0] - p[h].pos[0]) + (sun.pos[1] - p[h].pos[1]) * (sun.pos[1] - p[h].pos[1]) + (sun.pos[2] - p[h].pos[2]) * (sun.pos[2] - p[h].pos[2]));
        best = std::min(best, d);
    }
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float vp[16];
    cameraViewProj(app.camera, aspect, vp);
    float sx, sy;
    if (project(vp, sun.pos, sx, sy)) {
        for (int k = 0; k < 40; k++) {
            float a = 6.2831853f * (float)k / 40.0f;
            out[used++] = {sx + 11.0f * px * std::cos(a), sy + 11.0f * py * std::sin(a), 1.0f, 0.9f, 0.4f};
        }
        emitText(out, used, "SUN - HERE", sx + 16.0f * px, sy - 4.0f * py, px, py, 2.0f, 1.0f, 0.9f, 0.4f);
    }
    char text[80];
    std::snprintf(text, sizeof(text), "SUN IS %.0f THOUSAND LIGHT-YEARS FROM THE CENTRE", (double)(best * KLY_PER_UNIT));
    emitText(out, used, text, -1.0f + 12.0f * px, -1.0f + 150.0f * py, px, py, 2.0f, 1.0f, 0.9f, 0.4f);
    if (gyr >= 6.0f) {
        bool flung = best > 3.0f * SUN_START;
        emitText(out, used, flung ? "OUR SUN WAS FLUNG OUT OF THE REMNANT" : "OUR SUN STAYS INSIDE THE REMNANT", -1.0f + 12.0f * px, -1.0f + 172.0f * py, px, py, 2.0f, 1.0f, flung ? 0.5f : 0.9f, flung ? 0.4f : 0.4f);
    }
}
