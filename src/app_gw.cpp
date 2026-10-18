#include "vk_app.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr float CLOSE_ON = 8.0f;
constexpr float CLOSE_OFF = 20.0f;
constexpr double SMOOTH = 1.5;
constexpr float REF_STRAIN = 2.5e7f / 8.0f;
constexpr float CAPTION_HOLD = 4.0f;
constexpr float CAPTION_FADE = 2.0f;
constexpr float WAVE_C = 14.0f;

struct GwState {
    bool seenBoth = false, active = false, merged = false;
    double t0 = 0.0, tm = 0.0, lastSim = 0.0, wallMerge = 0.0;
    float sepS = 0.0f, omega = 0.0f, omegaDot = 0.0f, phi = 0.0f, amp = 0.0f, cx = 0.0f, cz = 0.0f;
    float phiM = 0.0f, omegaM = 0.0f, qM = 0.0f, ampM = 0.0f;
};

GwState g;

float smooth(float a, float b, float x) {
    float t = std::min(std::max((x - a) / (b - a), 0.0f), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float fadeOut(const AppContext& app) {
    return g.merged ? 1.0f - smooth(10.0f, 18.0f, (float)(app.simTime - g.tm)) : 1.0f;
}

}

void gwUpdate(AppContext& app) {
    if (!collisionLike(app.scene) || app.simTime < g.lastSim) g = GwState{};
    double dsim = app.simTime - g.lastSim;
    g.lastSim = app.simTime;
    if (!collisionLike(app.scene) || !app.readValid[app.currentFrame] || blackHoleCount(app) < 2) return;
    const Particle* p = static_cast<const Particle*>(app.readMapped[app.currentFrame]);
    float m0 = p[0].pos[3], m1 = p[1].pos[3];
    if (g.merged) return;
    if (m1 <= 0.0f) {
        if (!g.seenBoth) return;
        g.merged = true;
        g.tm = app.simTime;
        g.wallMerge = glfwGetTime();
        g.phiM = g.phi;
        g.omegaM = g.omega;
        g.qM = g.omegaDot;
        g.ampM = g.amp;
        return;
    }
    if (m0 <= 0.0f) return;
    float d[3];
    for (int a = 0; a < 3; a++) d[a] = p[1].pos[a] - p[0].pos[a];
    float sep = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (sep < 1e-3f) return;
    float mt = m0 + m1;
    float prev = g.omega;
    float k = (float)std::min(dsim / SMOOTH, 1.0);
    g.sepS = g.seenBoth ? g.sepS + (sep - g.sepS) * k : sep;
    g.omega = std::sqrt(app.simG * mt / (g.sepS * g.sepS * g.sepS));
    if (!g.seenBoth) g.phi = std::atan2(d[2], d[0]);
    if (g.seenBoth && dsim > 1e-6) {
        float dd = (float)((g.omega - prev) / dsim);
        g.omegaDot += (dd - g.omegaDot) * 0.02f;
        g.omegaDot = std::min(std::max(g.omegaDot, 0.0f), 4.0f * g.omega * g.omega);
        g.phi += g.omega * (float)std::min(dsim, 0.08);
    }
    g.seenBoth = true;
    g.amp = std::min(3.0f * (m0 * m1 / g.sepS) / REF_STRAIN, 9.0f);
    g.cx = (m0 * p[0].pos[0] + m1 * p[1].pos[0]) / mt;
    g.cz = (m0 * p[0].pos[2] + m1 * p[1].pos[2]) / mt;
    if (!g.active && sep < CLOSE_ON) {
        g.active = true;
        g.t0 = app.simTime;
    } else if (g.active && g.sepS > CLOSE_OFF) {
        g.active = false;
    }
}

void gwFill(const AppContext& app, float out[8]) {
    std::memset(out, 0, 8 * sizeof(float));
    out[6] = -1.0f;
    if (!collisionLike(app.scene) || (!g.active && !g.merged)) return;
    out[0] = g.cx;
    out[1] = g.cz;
    out[5] = WAVE_C * (float)(app.simTime - g.t0);
    if (g.merged) {
        float s = (float)(app.simTime - g.tm);
        out[2] = 2.0f * (g.phiM + g.omegaM * s);
        out[3] = 2.0f * g.omegaM / WAVE_C;
        out[4] = g.ampM * fadeOut(app);
        out[6] = s;
        out[7] = g.qM / (WAVE_C * WAVE_C);
    } else {
        out[2] = 2.0f * g.phi;
        out[3] = 2.0f * g.omega / WAVE_C;
        out[4] = g.amp;
        out[7] = g.omegaDot / (WAVE_C * WAVE_C);
    }
}

void gwEmitText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!collisionLike(app.scene) || (!g.active && !g.merged)) return;
    float px = 2.0f / (float)app.swapchainExtent.width;
    float py = 2.0f / (float)app.swapchainExtent.height;
    float wall = g.merged ? (float)(glfwGetTime() - g.wallMerge) : 0.0f;
    float freq = g.merged ? 1.2f * g.omegaM / 0.314159265f : g.omega / 0.314159265f;
    float alpha = 1.0f;
    if (g.merged) {
        alpha = 1.0f - smooth(CAPTION_HOLD, CAPTION_HOLD + CAPTION_FADE, wall);
        if (alpha <= 0.0f) return;
        const char* cap = "BLACK HOLES MERGED - GRAVITATIONAL WAVES";
        float w = (float)std::strlen(cap) * 6.0f * 3.0f * px;
        emitText(out, used, cap, -0.5f * w, -1.0f + 70.0f * py, px, py, 3.0f, 0.55f * alpha, 0.85f * alpha, 1.0f * alpha);
    }
    char text[48];
    std::snprintf(text, sizeof(text), "GW FREQUENCY %.2f", (double)freq);
    float w = (float)std::strlen(text) * 6.0f * 2.0f * px;
    emitText(out, used, text, -0.5f * w, -1.0f + 112.0f * py, px, py, 2.0f, 0.4f * alpha, 0.7f * alpha, 0.95f * alpha);
}
