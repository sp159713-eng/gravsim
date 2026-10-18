#include "vk_app.h"
#include "gpu.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

VkPipeline darkPipeline(AppContext& app, VkPipelineLayout layout);

namespace {

struct DarkPush {
    float eye[4], fwd[4], right[4], up[4], halo[4], params[4];
};

constexpr float HALO_GAIN = 0.4f;
constexpr float CAPTION_HOLD = 5.0f;
constexpr float CAPTION_FADE = 2.0f;
constexpr int BINS = 14;

VkPipelineLayout g_layout = VK_NULL_HANDLE;
VkPipeline g_pipe = VK_NULL_HANDLE;

void camBasis(const Camera& c, float f[3], float r[3], float u[3]) {
    float cp = std::cos(c.pitch), sp = std::sin(c.pitch), sy = std::sin(c.yaw), cy = std::cos(c.yaw);
    f[0] = -cp * sy; f[1] = -sp; f[2] = -cp * cy;
    float rl = std::sqrt(f[2] * f[2] + f[0] * f[0]);
    r[0] = rl > 1e-6f ? -f[2] / rl : 0.0f; r[1] = 0.0f; r[2] = rl > 1e-6f ? f[0] / rl : 0.0f;
    u[0] = r[1] * f[2] - r[2] * f[1];
    u[1] = r[2] * f[0] - r[0] * f[2];
    u[2] = r[0] * f[1] - r[1] * f[0];
}

float smooth(float a, float b, float x) {
    float t = std::min(std::max((x - a) / (b - a), 0.0f), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float discRadius(const AppContext& app, int hole) {
    if (app.scene == Scene::GALAXY) return 30.0f;
    if (app.scene == Scene::ANDROMEDA && hole == 1) return 19.25f;
    return 11.0f;
}

struct Curve {
    bool ok = false;
    float r[BINS], v[BINS], vk[BINS];
    bool has[BINS];
    float vmax = 1.0f, rmax = 31.0f;
};

Curve rotationCurve(const AppContext& app) {
    Curve c;
    if (app.scene != Scene::GALAXY || !app.showDark || !app.readValid[app.currentFrame] || app.n < 8) return c;
    const Particle* p = static_cast<const Particle*>(app.readMapped[app.currentFrame]);
    const float r0 = 4.0f, r1 = c.rmax;
    float sum[BINS] = {}, cnt[BINS] = {};
    for (int b = 0; b < BINS; b++) c.r[b] = r0 + (r1 - r0) * ((float)b + 0.5f) / BINS;
    for (uint32_t i = 1; i < app.n; i++) {
        if (p[i].pos[3] <= 0.0f || p[i].vel[3] != 0.0f) continue;
        float dx = p[i].pos[0] - p[0].pos[0], dz = p[i].pos[2] - p[0].pos[2];
        float r = std::sqrt(dx * dx + dz * dz);
        int b = (int)((r - r0) / (r1 - r0) * BINS);
        if (r < r0 || b < 0 || b >= BINS) continue;
        float vx = p[i].vel[0] - p[0].vel[0], vz = p[i].vel[2] - p[0].vel[2];
        sum[b] += std::fabs(dx * vz - dz * vx) / std::max(r, 1e-3f);
        cnt[b] += 1.0f;
    }
    for (int b = 0; b < BINS; b++) {
        c.has[b] = cnt[b] >= 3.0f;
        c.v[b] = c.has[b] ? sum[b] / cnt[b] : 0.0f;
        c.vk[b] = std::sqrt(app.simG * p[0].pos[3] / c.r[b]);
        c.vmax = std::max(c.vmax, std::max(c.v[b], c.vk[b]));
    }
    c.vmax *= 1.15f;
    c.ok = true;
    return c;
}

struct Box {
    float x0, y0, x1, y1;
};

Box insetBox(const AppContext& app) {
    float px = 2.0f / (float)app.swapchainExtent.width;
    float py = 2.0f / (float)app.swapchainExtent.height;
    return {1.0f - 380.0f * px, 1.0f - 105.0f * py, 1.0f - 40.0f * px, 1.0f - 315.0f * py};
}

float flatLevel(const Curve& c) {
    for (int b = 0; b < BINS; b++)
        if (c.has[b]) return c.v[b];
    return 0.0f;
}

}

void createDark(AppContext& app) {
    VkPushConstantRange pushF{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DarkPush)};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &pushF};
    if (vkCreatePipelineLayout(app.device, &li, nullptr, &g_layout) != VK_SUCCESS)
        throw std::runtime_error("failed dark layout!");
    g_pipe = darkPipeline(app, g_layout);
}

void destroyDark(AppContext& app) {
    vkDestroyPipeline(app.device, g_pipe, nullptr);
    vkDestroyPipelineLayout(app.device, g_layout, nullptr);
}

void darkToggleKey(AppContext& app) {
    app.showDark = !app.showDark;
    if (app.showDark) app.darkToggle = glfwGetTime();
}

void darkHalo(AppContext& app, VkCommandBuffer cmd, const float eye[3], const float centre[3], float core, float radius, float gain, float aspect) {
    if (gain <= 0.0005f) return;
    DarkPush push{};
    float f[3], r[3], u[3];
    camBasis(app.camera, f, r, u);
    const float tanH = std::tan(30.0f * 3.14159265f / 180.0f);
    for (int a = 0; a < 3; a++) {
        push.eye[a] = eye[a];
        push.fwd[a] = f[a];
        push.right[a] = r[a];
        push.up[a] = u[a];
        push.halo[a] = centre[a];
    }
    push.halo[3] = core;
    push.params[0] = gain;
    push.params[1] = aspect * tanH;
    push.params[2] = tanH;
    push.params[3] = radius;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipe);
    vkCmdPushConstants(cmd, g_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DarkPush), &push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void drawDark(AppContext& app, VkCommandBuffer cmd, float aspect) {
    if (!app.showDark) return;
    if (app.scene == Scene::SOLAR) {
        if (app.gal.active) return;
        float e[3], c[3];
        galaxyEye(app, e);
        galaxyCentreLy(c);
        const float disc = 50000.0f;
        darkHalo(app, cmd, e, c, 0.6f * disc, 10.0f * disc, 0.3f * HALO_GAIN * galaxyLayerFade(app), aspect);
        return;
    }
    if (app.scene != Scene::GALAXY && !collisionLike(app.scene)) return;
    if (!app.readValid[app.currentFrame] || readbackCount(app) == 0) return;
    const Particle* p = static_cast<const Particle*>(app.readMapped[app.currentFrame]);
    float eye[3];
    cameraEye(app.camera, eye);
    uint32_t holes = blackHoleCount(app);
    bool merged = holes > 1 && p[1].pos[3] <= 0.0f;
    for (uint32_t h = 0; h < holes; h++) {
        if (p[h].pos[3] <= 0.0f) continue;
        float disc = merged ? 1.15f * discRadius(app, 1) : discRadius(app, (int)h);
        darkHalo(app, cmd, eye, p[h].pos, 0.6f * disc, 10.0f * disc, HALO_GAIN, aspect);
    }
}

void emitDarkLines(AppContext& app, OverlayVertex* out, uint32_t& used) {
    Curve c = rotationCurve(app);
    if (!c.ok) return;
    Box b = insetBox(app);
    auto X = [&](float r) { return b.x0 + (b.x1 - b.x0) * r / c.rmax; };
    auto Y = [&](float v) { return b.y0 + (b.y1 - b.y0) * v / c.vmax; };
    auto seg = [&](float x0, float y0, float x1, float y1, float cr, float cg, float cb) {
        out[used++] = {x0, y0, cr, cg, cb};
        out[used++] = {x1, y1, cr, cg, cb};
    };
    seg(b.x0, b.y0, b.x1, b.y0, 0.6f, 0.6f, 0.7f);
    seg(b.x0, b.y0, b.x0, b.y1, 0.6f, 0.6f, 0.7f);
    float flat = flatLevel(c);
    for (int k = 0; k < 20; k += 2) seg(X(c.rmax * k / 20.0f), Y(flat), X(c.rmax * (k + 1) / 20.0f), Y(flat), 1.0f, 0.85f, 0.3f);
    for (int k = 0; k + 1 < BINS; k++) seg(X(c.r[k]), Y(c.vk[k]), X(c.r[k + 1]), Y(c.vk[k + 1]), 0.9f, 0.4f, 0.45f);
}

void emitDarkText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!app.showDark) return;
    float px = 2.0f / (float)app.swapchainExtent.width;
    float py = 2.0f / (float)app.swapchainExtent.height;
    float wall = (float)(glfwGetTime() - app.darkToggle);
    float alpha = 1.0f - smooth(CAPTION_HOLD, CAPTION_HOLD + CAPTION_FADE, wall);
    if (alpha > 0.0f) {
        const char* cap = "DARK MATTER: ~85% OF ALL MATTER - INVISIBLE, SEEN ONLY BY ITS GRAVITY";
        float w = (float)std::strlen(cap) * 6.0f * 2.0f * px;
        emitText(out, used, cap, -0.5f * w, 1.0f - 130.0f * py, px, py, 2.0f, 0.75f * alpha, 0.5f * alpha, 1.0f * alpha);
    }
    Curve c = rotationCurve(app);
    if (!c.ok) return;
    Box b = insetBox(app);
    emitText(out, used, "ROTATION CURVE", b.x0, b.y1 - 26.0f * py, px, py, 2.0f, 0.85f, 0.85f, 0.95f);
    emitText(out, used, "SPEED", b.x0 - 4.0f * px, b.y1 - 10.0f * py, px, py, 1.0f, 0.7f, 0.7f, 0.8f);
    emitText(out, used, "DISTANCE FROM CENTRE", b.x0, b.y0 + 8.0f * py, px, py, 1.0f, 0.7f, 0.7f, 0.8f);
    emitText(out, used, "REAL GALAXIES: FLAT (DARK MATTER)", b.x0, b.y0 + 24.0f * py, px, py, 1.0f, 1.0f, 0.85f, 0.3f);
    emitText(out, used, "EXPECTED FROM VISIBLE MASS (KEPLER)", b.x0, b.y0 + 38.0f * py, px, py, 1.0f, 0.9f, 0.4f, 0.45f);
    emitText(out, used, "+ THIS SIM STARS (MEASURED)", b.x0, b.y0 + 52.0f * py, px, py, 1.0f, 1.0f, 1.0f, 1.0f);
    for (int k = 0; k < BINS; k++) {
        if (!c.has[k]) continue;
        float x = b.x0 + (b.x1 - b.x0) * c.r[k] / c.rmax, y = b.y0 + (b.y1 - b.y0) * c.v[k] / c.vmax;
        for (int d = -2; d <= 2; d++) {
            out[used++] = {x + (float)d * px, y, 1.0f, 1.0f, 1.0f};
            out[used++] = {x, y + (float)d * py, 1.0f, 1.0f, 1.0f};
        }
    }
}
