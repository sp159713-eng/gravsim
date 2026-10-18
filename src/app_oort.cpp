#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

struct Dot {
    float au[3];
    float shade;
};

uint32_t g_seed = 12345u;

float rnd() {
    g_seed = g_seed * 1664525u + 1013904223u;
    return (float)(g_seed >> 8) / 16777216.0f;
}

const std::vector<Dot>& dots() {
    static std::vector<Dot> d;
    if (!d.empty()) return d;
    g_seed = 12345u;
    for (int i = 0; i < 4200; i++) {
        float r = i % 8 == 0 ? 2000.0f * std::pow(50.0f, rnd()) : 2000.0f + 98000.0f * std::pow(rnd(), 0.4f);
        float z = 2.0f * rnd() - 1.0f, ph = 6.2831853f * rnd();
        float s = std::sqrt(std::max(0.0f, 1.0f - z * z));
        d.push_back({{r * s * std::cos(ph), r * z, r * s * std::sin(ph)}, 0.35f + 0.65f * rnd()});
    }
    for (int i = 0; i < 1300; i++) {
        float r = 2000.0f * std::pow(10.0f, 1.0f * rnd());
        float ph = 6.2831853f * rnd();
        float y = r * 0.12f * (rnd() + rnd() - 1.0f);
        d.push_back({{r * std::cos(ph), y, r * std::sin(ph)}, 0.5f + 0.5f * rnd()});
    }
    for (int i = 0; i < 260; i++) {
        float r = 30.0f + 20.0f * rnd(), ph = 6.2831853f * rnd();
        d.push_back({{r * std::cos(ph), 0.0f, r * std::sin(ph)}, 0.8f});
    }
    return d;
}

bool setup(AppContext& app, float viewProj[16], float eyeOff[3], float& a) {
    a = tourOortAlpha(app);
    if (a < 0.01f) return false;
    float sun[3];
    sunPosition(app, sun);
    Camera cam = app.camera;
    for (int i = 0; i < 3; i++) {
        cam.pos[i] -= sun[i];
        eyeOff[i] = cam.pos[i];
    }
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    cameraViewProj(cam, aspect, viewProj);
    return true;
}

bool proj(const float m[16], const float p[3], float& x, float& y) {
    float c[4];
    for (int r = 0; r < 4; r++) c[r] = m[r] * p[0] + m[r + 4] * p[1] + m[r + 8] * p[2] + m[r + 12];
    if (c[3] <= 1e-9f) return false;
    x = c[0] / c[3];
    y = c[1] / c[3];
    return std::fabs(x) < 1.2f && std::fabs(y) < 1.2f;
}

}

void emitOortLines(AppContext& app, OverlayVertex* out, uint32_t& used) {
    float vp[16], eye[3], a;
    if (!setup(app, vp, eye, a)) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H, s = std::max(1.0f, H / 720.0f);
    float perAu = solarUnitsPerAu(app.trueScale);
    const std::vector<Dot>& d = dots();
    float eyeDist = std::sqrt(eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2]) / perAu;
    for (const Dot& o : d) {
        if (used + 40 > MAX_VERTS) break;
        float p[3] = {o.au[0] * perAu, o.au[1] * perAu, o.au[2] * perAu}, x, y;
        if (!proj(vp, p, x, y)) continue;
        float dx = p[0] - eye[0], dy = p[1] - eye[1], dz = p[2] - eye[2];
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz) / perAu;
        float near = std::min(1.0f, std::max(0.45f, 1.3f - 0.5f * dist / std::max(1.0f, eyeDist)));
        float b = 0.9f * a * o.shade * near;
        float hw = 1.1f * s * px;
        float hh = hw * W / H;
        out[used++] = {x - hw, y, 0.55f * b, 0.72f * b, 1.0f * b};
        out[used++] = {x + hw, y, 0.55f * b, 0.72f * b, 1.0f * b};
        out[used++] = {x, y - hh, 0.55f * b, 0.72f * b, 1.0f * b};
        out[used++] = {x, y + hh, 0.55f * b, 0.72f * b, 1.0f * b};
    }
    auto ring = [&](float r, float cr, float cg, float cb) {
        float prevx = 0.0f, prevy = 0.0f;
        bool have = false;
        for (int i = 0; i <= 48; i++) {
            float ph = 6.2831853f * (float)i / 48.0f;
            float p[3] = {r * std::cos(ph) * perAu, 0.0f, r * std::sin(ph) * perAu}, x, y;
            bool ok = proj(vp, p, x, y);
            if (ok && have && used + 4 < MAX_VERTS) {
                out[used++] = {prevx, prevy, cr * a, cg * a, cb * a};
                out[used++] = {x, y, cr * a, cg * a, cb * a};
            }
            have = ok;
            prevx = x;
            prevy = y;
        }
    };
    ring(30.1f, 0.35f, 0.55f, 0.95f);
    ring(40.0f, 0.3f, 0.4f, 0.55f);
    float z[3] = {0.0f, 0.0f, 0.0f}, sx, sy;
    if (proj(vp, z, sx, sy) && used + 12 < MAX_VERTS) {
        float r = 4.0f * s;
        const float c[3] = {1.0f * a, 0.9f * a, 0.5f * a};
        out[used++] = {sx - r * px, sy, c[0], c[1], c[2]};
        out[used++] = {sx + r * px, sy, c[0], c[1], c[2]};
        out[used++] = {sx, sy - r * py, c[0], c[1], c[2]};
        out[used++] = {sx, sy + r * py, c[0], c[1], c[2]};
        out[used++] = {sx - 0.7f * r * px, sy - 0.7f * r * py, c[0], c[1], c[2]};
        out[used++] = {sx + 0.7f * r * px, sy + 0.7f * r * py, c[0], c[1], c[2]};
        out[used++] = {sx - 0.7f * r * px, sy + 0.7f * r * py, c[0], c[1], c[2]};
        out[used++] = {sx + 0.7f * r * px, sy - 0.7f * r * py, c[0], c[1], c[2]};
    }
}

void emitOortText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    float vp[16], eye[3], a;
    if (!setup(app, vp, eye, a)) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H, s = std::max(1.0f, H / 720.0f);
    const char* title = "OORT CLOUD (ILLUSTRATION)";
    int sc = fitScale(title, W, std::max(2, (int)std::lround(3.0f * s)));
    float len = (float)std::strlen(title) * 6.0f * (float)sc;
    emitText(out, used, title, -1.0f + (W - len) * 0.5f * px, -1.0f + 0.1f * H * py, px, py, (float)sc, 0.6f * a, 0.78f * a, 1.0f * a);
    float z[3] = {0.0f, 0.0f, 0.0f}, sx, sy;
    if (proj(vp, z, sx, sy)) {
        float fs = std::max(1.0f, std::floor(1.5f * s));
        emitText(out, used, "SUN", sx + 10.0f * s * px, sy - 14.0f * s * py, px, py, fs, 1.0f * a, 0.9f * a, 0.5f * a);
    }
}
